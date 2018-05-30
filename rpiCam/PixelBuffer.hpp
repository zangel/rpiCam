#pragma once

#include "Config.hpp"
#include "Buffer.hpp"
#include "PixelFormat.hpp"

namespace rpiCam
{
    class PixelBuffer
        : public virtual Buffer
    {
    public:
        PixelBuffer();
        virtual ~PixelBuffer();

        virtual ePixelFormat format() const = 0;
        virtual std::size_t planeCount() const = 0;

        virtual Vec2ui planeSize(std::size_t pi = 0) const = 0;
        virtual void* planeData(std::size_t pi = 0) = 0;
        virtual std::size_t planeRowBytes(std::size_t pi = 0) const = 0;
    };
}
