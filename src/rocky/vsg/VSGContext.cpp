/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#include "VSGContext.h"
#include "MapNode.h"
#include "Utils.h"
#include <rocky/Image.h>
#include <rocky/URI.h>

#include <spdlog/sinks/stdout_color_sinks.h>

ROCKY_ABOUT(vulkanscenegraph, VSG_VERSION_STRING)

#ifdef ROCKY_HAS_VSGXCHANGE
#include <vsgXchange/all.h>
ROCKY_ABOUT(vsgxchange, VSGXCHANGE_VERSION_STRING)
#endif

#ifdef ROCKY_HAS_GDAL
#include <rocky/GDAL.h>
#endif

using namespace ROCKY_NAMESPACE;

namespace
{
    // custom VSG logger that redirects to spdlog.
    class VSG_to_Spdlog_Logger : public Inherit<vsg::Logger, VSG_to_Spdlog_Logger>
    {
    public:
        std::shared_ptr<spdlog::logger> vsg_logger;

        VSG_to_Spdlog_Logger()
        {
            vsg_logger = spdlog::stdout_color_mt("vsg");
            vsg_logger->set_pattern("%^[%n %l]%$ %v");
        }

    protected:
        const char* ignore = "[rocky.ignore]";

        void debug_implementation(const std::string_view& message) override {
            if (message.rfind(ignore, 0) != 0) {
                vsg_logger->set_level(Log()->level());
                vsg_logger->debug(message);
            }
        }
        void info_implementation(const std::string_view& message) override {
            if (message.rfind(ignore, 0) != 0) {
                vsg_logger->set_level(Log()->level());
                vsg_logger->info(message);
            }
        }
        void warn_implementation(const std::string_view& message) override {
            if (message.rfind(ignore, 0) != 0) {
                vsg_logger->set_level(Log()->level());
                vsg_logger->warn(message);
            }
        }
        void error_implementation(const std::string_view& message) override {
            if (message.rfind(ignore, 0) != 0) {
                vsg_logger->set_level(Log()->level());
                vsg_logger->error(message);
            }
        }
        void fatal_implementation(const std::string_view& message) override {
            if (message.rfind(ignore, 0) != 0) {
                vsg_logger->set_level(Log()->level());
                vsg_logger->critical(message);
            }
        }
    };

    // recursive search for a vsg::ReaderWriters that matches the extension
    // TODO: expand to include 'protocols' I guess
    vsg::ref_ptr<vsg::ReaderWriter> findReaderWriter(const std::string& extension, const vsg::ReaderWriters& readerWriters)
    {
        vsg::ref_ptr<vsg::ReaderWriter> output;

        for (auto& rw : readerWriters)
        {
            vsg::ReaderWriter::Features features;
            auto crw = dynamic_cast<vsg::CompositeReaderWriter*>(rw.get());
            if (crw)
            {
                output = findReaderWriter(extension, crw->readerWriters);
            }
            else if (rw->getFeatures(features))
            {
                auto j = features.extensionFeatureMap.find(extension);

                if (j != features.extensionFeatureMap.end())
                {
                    if (j->second & vsg::ReaderWriter::FeatureMask::READ_ISTREAM)
                    {
                        output = rw;
                    }
                }
            }

            if (output)
                break;
        }

        return output;
    }

#ifdef ROCKY_HAS_GDAL
    /**
    * VSG reader-writer that uses GDAL to read some image formats that are
    * not supported by vsgXchange
    */
    class GDAL_VSG_ReaderWriter : public vsg::Inherit<vsg::ReaderWriter, GDAL_VSG_ReaderWriter>
    {
    public:
        Features _features;

        GDAL_VSG_ReaderWriter()
        {
            _features.extensionFeatureMap[vsg::Path(".webp")] = READ_ISTREAM;
            _features.extensionFeatureMap[vsg::Path(".tif")] = READ_ISTREAM;
            _features.extensionFeatureMap[vsg::Path(".jpg")] = READ_ISTREAM;
            _features.extensionFeatureMap[vsg::Path(".png")] = READ_ISTREAM;
        }

        bool getFeatures(Features& out) const override
        {
            out = _features;
            return true;
        }

        vsg::ref_ptr<vsg::Object> read(std::istream& in, vsg::ref_ptr<const vsg::Options> options = {}) const override
        {
            if (!options || _features.extensionFeatureMap.count(options->extensionHint) == 0)
                return {};

            std::stringstream buf;
            buf << in.rdbuf() << std::flush;
            std::string data = buf.str();

            std::string gdal_driver =
                options->extensionHint.string() == ".webp" ? "webp" :
                options->extensionHint.string() == ".tif" ? "gtiff" :
                options->extensionHint.string() == ".jpg" ? "jpeg" :
                options->extensionHint.string() == ".png" ? "png" :
                "";

            auto result = GDAL::readImage((unsigned char*)data.c_str(), data.length(), gdal_driver);

            if (result.status.ok())
                return util::moveImageToVSG(result.value);
            else
                return { };
        }
    };
#endif

    // Adapted from https://oroboro.com/image-format-magic-bytes
    std::string deduceContentTypeFromStream(std::istream& stream)
    {
        // Get the length of the stream
        stream.seekg(0, std::ios::end);
        unsigned int len = stream.tellg();
        stream.seekg(0, std::ios::beg);

        if (len < 16) return {};

        // Read a 16 byte header
        char data[16];
        stream.read(data, 16);

        // Reset reading
        stream.seekg(0, std::ios::beg);

        // .jpg:  FF D8 FF
        // .png:  89 50 4E 47 0D 0A 1A 0A
        // .gif:  GIF87a
        //        GIF89a
        // .tiff: 49 49 2A 00
        //        4D 4D 00 2A
        // .bmp:  BM
        // .webp: RIFF ???? WEBP
        // .ico   00 00 01 00
        //        00 00 02 00 ( cursor files )
        switch (data[0])
        {
        case '\xFF':
            return (!strncmp((const char*)data, "\xFF\xD8\xFF", 3)) ? "image/jpg" : "";

        case '\x89':
            return (!strncmp((const char*)data,
                "\x89\x50\x4E\x47\x0D\x0A\x1A\x0A", 8)) ? "image/png" : "";

        case 'G':
            return (!strncmp((const char*)data, "GIF87a", 6) ||
                !strncmp((const char*)data, "GIF89a", 6)) ? "image/gif" : "";

        case 'I':
            return (!strncmp((const char*)data, "\x49\x49\x2A\x00", 4)) ? "image/tif" : "";

        case 'M':
            return (!strncmp((const char*)data, "\x4D\x4D\x00\x2A", 4)) ? "image/tif" : "";

        case 'B':
            return ((data[1] == 'M')) ? "image/bmp" : "";

        case 'R':
            return (!strncmp((const char*)data, "RIFF", 4)) ? "image/webp" : "";
        }

        return { };
    }

    bool foundShaders(const vsg::Paths& searchPaths)
    {
        auto options = vsg::Options::create();
        options->paths = searchPaths;
        auto found = vsg::findFile(vsg::Path("shaders/rocky.terrain.vert"), options);
        return !found.empty();
    }    
    
    /**
    * An update operation that maintains a priroity queue for update tasks.
    * This sits in the VSG viewer's update operations queue indefinitely
    * and runs once per frame. It chooses the highest priority task in its
    * queue and runs it. It will run one task per frame so that we do not
    * risk frame drops. It will automatically discard any tasks that have
    * been abandoned (no Future exists).
    */
    struct PriorityUpdateQueue : public vsg::Inherit<vsg::Operation, PriorityUpdateQueue>
    {
        std::mutex _mutex;

        struct Task {
            vsg::ref_ptr<vsg::Operation> function;
            std::function<float()> get_priority;
        };
        std::vector<Task> _queue;

        // runs one task per frame.
        void run() override
        {
            if (!_queue.empty())
            {
                Task task;
                {
                    std::scoped_lock lock(_mutex);

                    // sort from low to high priority
                    std::sort(_queue.begin(), _queue.end(),
                        [](const Task& lhs, const Task& rhs)
                        {
                            if (lhs.get_priority == nullptr)
                                return false;
                            else if (rhs.get_priority == nullptr)
                                return true;
                            else
                                return lhs.get_priority() < rhs.get_priority();
                        }
                    );

                    while (!_queue.empty())
                    {
                        // pop the highest priority off the back.
                        task = _queue.back();
                        _queue.pop_back();

                        // check for cancelation - if the task is already canceled, 
                        // discard it and fetch the next one.
                        auto po = dynamic_cast<Cancelable*>(task.function.get());
                        if (po == nullptr || !po->canceled())
                            break;
                        else
                            task = { };
                    }
                }

                if (task.function)
                {
                    task.function->run();
                }
            }
        }
    };

    struct SimpleUpdateOperation : public vsg::Inherit<vsg::Operation, SimpleUpdateOperation>
    {
        std::function<void()> _function;

        SimpleUpdateOperation(std::function<void()> function) :
            _function(function) { }

        void run() override
        {
            _function();
        };
    };
}




VSGContextImpl::VSGContextImpl(vsg::ref_ptr<vsg::Viewer> viewer_) :
    rocky::ContextImpl(),
    viewer(viewer_)
{
    int argc = 0;
    const char* argv[1] = { "rocky" };
    ctor(argc, (char**)argv);
}

VSGContextImpl::VSGContextImpl(vsg::ref_ptr<vsg::Viewer> viewer_, int& argc, char** argv) :
    rocky::ContextImpl(),
    viewer(viewer_)
{
    ctor(argc, argv);
}

void
VSGContextImpl::ctor(int& argc, char** argv)
{
    vsg::CommandLine args(&argc, argv);

    readerWriterOptions = vsg::Options::create();

    shaderCompileSettings = vsg::ShaderCompileSettings::create();

    _priorityUpdateQueue = PriorityUpdateQueue::create();

    // initialize the deferred deletion collection.
    // a large number of frames ensures objects will be safely destroyed and
    // and we won't have too many deletions per frame.
    _disposal_queue.resize(8);

    args.read(readerWriterOptions);

    // redirect the VSG logger to our spdlog
    vsg::Logger::instance() = new VSG_to_Spdlog_Logger();

    // set the logging level from the command line
    std::string log_level;
    if (args.read("--log-level", log_level))
    {
        if (log_level == "debug") Log()->set_level(spdlog::level::debug);
        else if (log_level == "info") Log()->set_level(spdlog::level::info);
        else if (log_level == "warn") Log()->set_level(spdlog::level::warn);
        else if (log_level == "error") Log()->set_level(spdlog::level::err);
        else if (log_level == "critical") Log()->set_level(spdlog::level::critical);
        else if (log_level == "off") Log()->set_level(spdlog::level::off);
    }

    // set on-demand rendering mode from the command line
    if (args.read("--on-demand"))
    {
        renderOnDemand = true;
    }

#ifdef ROCKY_HAS_GDAL
    readerWriterOptions->add(GDAL_VSG_ReaderWriter::create());
#endif

#ifdef ROCKY_HAS_VSGXCHANGE
    // Adds all the readerwriters in vsgxchange to the options data.
    readerWriterOptions->add(vsgXchange::all::create());
#endif

    // For system fonts
    readerWriterOptions->paths.push_back("C:/windows/fonts");
    readerWriterOptions->paths.push_back("/etc/fonts");
    readerWriterOptions->paths.push_back("/usr/local/share/rocky/data");

    // Load a default font if there is one
    auto font_file = util::getEnvVar("ROCKY_DEFAULT_FONT");
    if (font_file.empty())
    {
#ifdef WIN32
        font_file = "arialbd.ttf";
#else
        font_file = "times.vsgb";
#endif
    }

    defaultFont = vsg::read_cast<vsg::Font>(font_file, readerWriterOptions);
    if (!defaultFont)
    {
        Log()->warn("Cannot load font \"" + font_file + "\"");
    }

    // establish search paths for shaders and data:
    auto vsgPaths = vsg::getEnvPaths("VSG_FILE_PATH");
    searchPaths.insert(searchPaths.end(), vsgPaths.begin(), vsgPaths.end());

    auto rockyPaths = vsg::getEnvPaths("ROCKY_FILE_PATH");
    searchPaths.insert(searchPaths.end(), rockyPaths.begin(), rockyPaths.end());

    // add some default places to look for shaders and resources, relative to the executable.
    auto exec_path = std::filesystem::path(util::getExecutableLocation());
    auto path = (exec_path.remove_filename() / "../share/rocky").lexically_normal();
    if (!path.empty())
        searchPaths.push_back(vsg::Path(path.generic_string()));

    path = (exec_path.remove_filename() / "../../../../build_share").lexically_normal();
    if (!path.empty())
        searchPaths.push_back(vsg::Path(path.generic_string()));

    if (!foundShaders(searchPaths))
    {
        Log()->warn("Trouble: Rocky may not be able to find its shaders. "
            "Consider setting one of the environment variables VSG_FILE_PATH or ROCKY_FILE_PATH.");
    }

    Log()->debug("Search paths:");
    for (auto& path : searchPaths)
        Log()->debug("  {}", path.string());

    // Install a readImage function that uses the VSG facility
    // for reading data. We may want to subclass Image with something like
    // NativeImage that just hangs on to the vsg::Data instead of
    // stripping it out and later converting it back; or that only transcodes
    // it if it needs to. vsg::read_cast() might do some internal caching
    // as well -- need to look into that.
    io.services.readImageFromURI = [](const std::string& location, const rocky::IOOptions& io)
    {
        auto result = URI(location).read(io);
        if (result.status.ok())
        {
            std::stringstream buf(result.value.data);
            return io.services.readImageFromStream(buf, result.value.contentType, io);
        }
        return Result<std::shared_ptr<Image>>(Status(Status::ResourceUnavailable, "Data is null"));
    };

    // map of mime-types to extensions that VSG can understand
    static const std::unordered_map<std::string, std::string> ext_for_mime_type = {
        { "image/bmp", ".bmp" },
        { "image/gif", ".gif" },
        { "image/jpg", ".jpg" },
        { "image/jpeg", ".jpg" },
        { "image/png", ".png" },
        { "image/tga", ".tga" },
        { "image/tif", ".tif" },
        { "image/tiff", ".tif" },
        { "image/webp", ".webp" }
    };

    // To read from a stream, we have to search all the VS readerwriters to
    // find one that matches the 'extension' we want. We also have to put that
    // extension in the options structure as a hint.
    io.services.readImageFromStream = [options(readerWriterOptions)](std::istream& location, std::string contentType, const rocky::IOOptions& io) -> Result<std::shared_ptr<Image>>
        {
            // try the mime-type mapping:
            auto i = ext_for_mime_type.find(contentType);
            if (i != ext_for_mime_type.end())
            {
                auto rw = findReaderWriter(i->second, options->readerWriters);
                if (rw != nullptr)
                {
                    auto local_options = vsg::Options::create(*options);
                    local_options->extensionHint = i->second;
                    auto result = rw->read_cast<vsg::Data>(location, local_options);
                    return util::makeImageFromVSG(result);
                }
            }

            // mime-type didn't work; try the content type directly as an extension
            if (!contentType.empty())
            {
                auto contentTypeAsExtension = contentType[0] != '.' ? ("." + contentType) : contentType;
                auto rw = findReaderWriter(contentTypeAsExtension, options->readerWriters);
                if (rw != nullptr)
                {
                    auto local_options = vsg::Options::create(*options);
                    local_options->extensionHint = contentTypeAsExtension;
                    auto result = rw->read_cast<vsg::Data>(location, local_options);
                    return util::makeImageFromVSG(result);
                }
            }

            // last resort, try checking the data itself
            auto decudedContentType = deduceContentTypeFromStream(location);
            if (!decudedContentType.empty())
            {
                auto i = ext_for_mime_type.find(decudedContentType);
                if (i != ext_for_mime_type.end())
                {
                    auto rw = findReaderWriter(i->second, options->readerWriters);
                    if (rw != nullptr)
                    {
                        auto local_options = vsg::Options::create(*options);
                        local_options->extensionHint = i->second;
                        auto result = rw->read_cast<vsg::Data>(location, local_options);
                        return util::makeImageFromVSG(result);
                    }
                }
            }

            return Status(Status::ServiceUnavailable, "No image reader for \"" + contentType + "\"");
        };

    io.services.contentCache = std::make_shared<ContentCache>(128);

    io.uriGate = std::make_shared<util::Gate<std::string>>();
}


void
VSGContextImpl::onNextUpdate(vsg::ref_ptr<vsg::Operation> function, std::function<float()> get_priority)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(viewer.valid(), void());

    auto pq = dynamic_cast<PriorityUpdateQueue*>(_priorityUpdateQueue.get());
    if (pq)
    {
        std::scoped_lock lock(pq->_mutex);

        if (pq->referenceCount() == 1)
        {
            viewer->updateOperations->add(_priorityUpdateQueue, vsg::UpdateOperations::ALL_FRAMES);
        }

        pq->_queue.push_back({ function, get_priority });
    }
}

void
VSGContextImpl::onNextUpdate(std::function<void()> function)
{
    ROCKY_SOFT_ASSERT_AND_RETURN(viewer.valid(), void(), "Developer: failure to set VSGContext->viewer");

    viewer->updateOperations->add(SimpleUpdateOperation::create(function));
}

void
VSGContextImpl::compile(vsg::ref_ptr<vsg::Object> compilable)
{
    ROCKY_SOFT_ASSERT(viewer.valid(), "Developer: failure to set VSGContext->viewer");
    ROCKY_SOFT_ASSERT_AND_RETURN(compilable.valid(), void());

    // note: this can block (with a fence) until a compile traversal is available.
    // Be sure to group as many compiles together as possible for maximum performance.
    auto cr = viewer->compileManager->compile(compilable);

    if (cr && cr.requiresViewerUpdate())
    {
        // compile results are stored and processed later during update
        std::unique_lock lock(_compileMutex);
        _compileResult.add(cr);
    }
}

void
VSGContextImpl::dispose(vsg::ref_ptr<vsg::Object> object)
{
    if (object)
    {
        // if the user installed a custom disposer, use it
        if (disposer)
        {
            disposer(object);
        }

        // otherwise use our own
        else
        {
            std::unique_lock lock(_disposal_queue_mutex);
            _disposal_queue.back().emplace_back(object);
        }
    }
}

void
VSGContextImpl::dirtyShaders()
{
    ++shaderSettingsRevision;
}

void
VSGContextImpl::requestFrame()
{
    ++renderRequests;
}

bool
VSGContextImpl::update()
{
    ROCKY_SOFT_ASSERT_AND_RETURN(viewer.valid(), false, "Developer: failure to set VSGContext->viewer");

    bool updates_occurred = false;

    if (_compileResult)
    {
        std::unique_lock lock(_compileMutex);

        if (_compileResult.requiresViewerUpdate())
        {
            vsg::updateViewer(*viewer, _compileResult);
            updates_occurred = true;
            requestFrame();
        }
        _compileResult.reset();
    }

    // process the deferred unref list
    // TODO: make this a ring buffer?
    {
        std::unique_lock lock(_disposal_queue_mutex);
        // unref everything in the oldest collection:
        _disposal_queue.front().clear();
        // move the empty collection to the back:
        _disposal_queue.emplace_back(std::move(_disposal_queue.front()));
        _disposal_queue.pop_front();
    }

    // reset the view IDs list
    activeViewIDs.clear();

    return updates_occurred;
}
