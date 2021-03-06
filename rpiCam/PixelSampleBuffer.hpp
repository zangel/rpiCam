#pragma once

#include "Config.hpp"
#include "PixelBuffer.hpp"
#include "SampleBuffer.hpp"

namespace rpiCam
{
    class PixelSampleBuffer
        : public PixelBuffer
        , public SampleBuffer
    {
    public:
        PixelSampleBuffer();
        virtual ~PixelSampleBuffer();
    };
}
