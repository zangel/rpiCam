#pragma once

#include "Config.hpp"
#include "Buffer.hpp"

namespace rpiCam
{
    class SampleBuffer
        : public virtual Buffer
    {
    public:
        SampleBuffer();
        virtual ~SampleBuffer();

        virtual std::error_code lock() = 0;
        virtual std::error_code unlock() = 0;

        TimePoint time;
    };
}
