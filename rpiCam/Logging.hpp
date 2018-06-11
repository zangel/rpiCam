#pragma once

#include "Config.hpp"

namespace rpiCam
{
    enum LOG_LEVEL
    {
        LOG_DEBUG = 0,
        LOG_INFO,
        LOG_WARNING,
        LOG_ERROR,
        LOG_SILENT
    };

    typedef void (*LOG_CALLBACK)(LOG_LEVEL level, const char *format, ...);

    void log(LOG_LEVEL level, const char *format, ...);
    LOG_LEVEL getLogLevel();
    void setLogLevel(LOG_LEVEL level);
    void setLogCallback(LOG_CALLBACK callback);

}

#define RPI_LOG(level, format, ...) ::rpiCam::log(::rpiCam::LOG_##level, format, ##__VA_ARGS__)
