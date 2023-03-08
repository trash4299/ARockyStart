/**
 * rocky c++
 * Copyright 2023 Pelican Mapping
 * MIT License
 */
#pragma once

#include <rocky/Image.h>

namespace ROCKY_NAMESPACE
{
    constexpr float NO_DATA_VALUE = -FLT_MAX;

#if 0
    class ROCKY_EXPORT Heightfield
    {
    public:
        Heightfield(shared_ptr<Image> image) : _image(image) { }

        //! Access the height value at col, row
        inline float& heightAt(unsigned col, unsigned row);
        inline float heightAt(unsigned col, unsigned row) const;

        //! Visits each height in the field with a user-provided function
        //! that takes "float" or "float&" as an argument.
        template<typename FUNC>
        void forEachHeight(FUNC func);

        template<typename FUNC>
        void forEachHeight(FUNC func) const;

        //! Interpolated height at a normalized (u,v) location
        float heightAtUV(
            double u, double v,
            Image::Interpolation interp = Image::BILINEAR) const;

        //! Interpolated height at a floating point col/row location
        float heightAtPixel(
            double col, double row,
            Image::Interpolation interp = Image::BILINEAR) const;

        //! Fill with a single height value
        void fill(float value);

        //! Size of the heightfield
        inline unsigned width() const;
        inline unsigned height() const;

    private:
        shared_ptr<Image> _image;
    };

    // inline functions

    float& Heightfield::heightAt(unsigned c, unsigned r)
    {
        return _image->data<float>(c, r);
    }

    float Heightfield::heightAt(unsigned c, unsigned r) const
    {
        return _image->data<float>(c, r);
    }

    template<typename FUNC>
    void Heightfield::forEachHeight(FUNC func)
    {
        float* ptr = _image->data<float>();
        for (auto i = 0u; i < _image->sizeInPixels(); ++i, ++ptr)
            func(*ptr);
    }

    template<typename FUNC>
    void Heightfield::forEachHeight(FUNC func) const
    {
        float* ptr = _image->data<float>();
        for (auto i = 0u; i < _image->sizeInPixels(); ++i, ++ptr)
            func(*ptr);
    }

    unsigned Heightfield::width() const
    {
        return _image->width();
    }

    unsigned Heightfield::height() const
    {
        return _image->height();
    }

#else

    /**
     * A grid of height values (32-bit floats).
     */
    class ROCKY_EXPORT Heightfield : public Inherit<Image, Heightfield>
    {
    public:
        //! Construct an empty (and invalid) heightfield
        Heightfield();

        //! Construct a heightfield with the given dimensions
        Heightfield(
            unsigned cols,
            unsigned rows);

        //! Make a heightfield, stealing data from an image.
        explicit Heightfield(Image* rhs);

        //! Pointer that can be used to call heightfield functions on an Image 
        //! object, as long as that Image object is a valid heightfield format.
        //! usage: auto hf = Heightfield::cast_from(image);
        static const Heightfield* cast_from(const Image* rhs);

        //! Access the height value at col, row
        inline float& heightAt(unsigned col, unsigned row);
        inline float heightAt(unsigned col, unsigned row) const;

        //! Visits each height in the field with a user-provided function
        //! that takes "float" or "float&" as an argument.
        template<typename FUNC>
        void forEachHeight(FUNC func);

        template<typename FUNC>
        void forEachHeight(FUNC func) const;

        //! Interpolated height at a normalized (u,v) location
        float heightAtUV(
            double u, double v,
            Interpolation interp = BILINEAR) const;

        //! Interpolated height at a floating point col/row location
        float heightAtPixel(
            double col, double row,
            Interpolation interp = BILINEAR) const;

        //! Fill with a single height value
        void fill(float value);
    };


    // inline functions

    float& Heightfield::heightAt(unsigned c, unsigned r)
    {
        return data<float>(c, r);
    }

    float Heightfield::heightAt(unsigned c, unsigned r) const
    {
        return data<float>(c, r);
    }    

    template<typename FUNC>
    void Heightfield::forEachHeight(FUNC func)
    {
        float* ptr = data<float>();
        for (auto i = 0u; i < sizeInPixels(); ++i, ++ptr)
            func(*ptr);
    }

    template<typename FUNC>
    void Heightfield::forEachHeight(FUNC func) const
    {
        float* ptr = data<float>();
        for (auto i = 0u; i < sizeInPixels(); ++i, ++ptr)
            func(*ptr);
    }
#endif
}
