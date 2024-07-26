/**
 * rocky c++
 * Copyright 2023-2024 Pelican Mapping, Chris Djali
 * MIT License
 */
#include "BingImageLayer.h"
#ifdef ROCKY_HAS_BING

#include "Instance.h"
#include "json.h"

using namespace ROCKY_NAMESPACE;
using namespace ROCKY_NAMESPACE::Bing;

#undef LC
#define LC "[Bing] "

ROCKY_ADD_OBJECT_FACTORY(BingImage,
    [](const JSON& conf) { return BingImageLayer::create(conf); })

BingImageLayer::BingImageLayer() :
    super()
{
    construct(JSON());
}

BingImageLayer::BingImageLayer(const JSON& conf) :
    super(conf)
{
    construct(conf);
}

void
BingImageLayer::construct(const JSON& conf)
{
    setConfigKey("BingImage");
    const auto j = parse_json(conf);
    get_to(j, "key", apiKey);
    get_to(j, "imagery_set", imagerySet);
    get_to(j, "imagery_metadata_api_url", imageryMetadataUrl);
}

JSON
BingImageLayer::to_json() const
{
    auto j = parse_json(super::to_json());
    set(j, "key", apiKey);
    set(j, "imagery_set", imagerySet);
    set(j, "imagery_metadata_api_url", imageryMetadataUrl);
    return j.dump();
}

Status
BingImageLayer::openImplementation(const IOOptions& io)
{
    Status parent = super::openImplementation(io);
    if (parent.failed())
        return parent;

    _profile = Profile(SRS::SPHERICAL_MERCATOR, Profile::SPHERICAL_MERCATOR.extent().bounds(), 2, 2);
    setDataExtents({ _profile->extent() });

    _tileURICache = std::make_unique<TileURICache>();

    ROCKY_TODO("When disk cache is implemented, disable it here as it violates the ToS");

    ROCKY_TODO("Update attribution - it's included in the per-tile metadata, but we don't track which tiles are still visible and only have the data in a const function");

    return StatusOK;
}

void
BingImageLayer::closeImplementation()
{
    _tileURICache.reset();
    super::closeImplementation();
}

Result<GeoImage>
BingImageLayer::createImageImplementation(const TileKey& key, const IOOptions& io) const
{
    ROCKY_PROFILE_FUNCTION();

    auto imageURI = _tileURICache->get(key);

    if (!imageURI.has_value())
    {
        // Bing's zoom is indexed slightly differently
        auto zoom = key.levelOfDetail() + 1;
        GeoPoint centre = key.extent().centroid();
        centre.transformInPlace(centre.srs.geoSRS());

        std::stringstream relative;
        relative << std::setprecision(12);
        relative << "/" << imagerySet.value();
        relative << "/" << centre.y << "," << centre.x;
        relative << "?zl=" << zoom;
        relative << "&o=json";
        if (apiKey.has_value())
            relative << "&key=" << apiKey.value();

        URI metadataURI(imageryMetadataUrl->full() + relative.str(), imageryMetadataUrl->context());

        auto metaFetch = metadataURI.read(io);
        if (metaFetch.status.ok())
        {
            auto json = parse_json(metaFetch->data);
            const auto& vintage = json["/resourceSets/0/resources/0/vintageEnd"_json_pointer];
            const auto& jsonURI = json["/resourceSets/0/resources/0/imageUrl"_json_pointer];
            if (!vintage.empty() && !jsonURI.empty())
                imageURI = URI(jsonURI.get<std::string>(), imageryMetadataUrl->context());
            else
                imageURI = Status(Status::ResourceUnavailable, "No data at this level");
        }
        else
        {
            imageURI = metaFetch.status;
        }

        _tileURICache->put(key, imageURI);
    }

    if (imageURI->status.failed())
        return imageURI->status;

    auto fetch = imageURI->value.read(io);
    if (fetch.status.failed())
        return fetch.status;

    std::istringstream buf(fetch->data);
    auto image_rr = io.services.readImageFromStream(buf, fetch->contentType, io);

    if (image_rr.status.failed())
        return image_rr.status;

    shared_ptr<Image> image = image_rr.value;

    if (image)
        return GeoImage(image, key.extent());
    else
        return StatusError;
}

#endif // ROCKY_HAS_BING
