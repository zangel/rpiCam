#pragma once

#include "Config.hpp"

namespace rpiCam
{
    typedef enum
    {
        kPixelFormatInvalid=0,
        kPixelFormatRGB8,
        kPixelFormatYUV420,
        kPixelFormatMax
    } ePixelFormat;
}
