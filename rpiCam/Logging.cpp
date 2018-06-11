#include "Logging.hpp"
#include <iostream>
#include <cstdarg>
#include <ctime>
#include <iomanip>

namespace rpiCam
{
    namespace
    {
        static LOG_LEVEL logLevel = LOG_DEBUG;
        static LOG_CALLBACK logCallback = nullptr;
        static std::recursive_mutex logSync;
        static const char *logLevelNames[]=
        {
            "DEBUG",
            "INFO",
            "WARNING",
            "ERROR"
        };

    }

    void log(LOG_LEVEL level, const char *format, ...)
    {
        level = std::min(LOG_ERROR, std::max(LOG_DEBUG, level));
        if (level < logLevel)
            return;

        std::lock_guard<std::recursive_mutex> lock(logSync);
        va_list args;
        va_start(args, format);

        if (logCallback)
        {
            logCallback(level, format, args);
        }
        else
        {
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            auto now_time_t = std::ctime(&now);
            std::tm now_tm = *std::localtime(&now);
            static char msg[1024];
            std::vsnprintf(msg, sizeof(msg), format, args);
            std::cout << "[" << std::put_time(&now_tm, "%c") << ":" << logLevelNames[level] << "]: " << msg << std::endl;
        }
        va_end(args);
    }


    LOG_LEVEL getLogLevel()
    {
        return logLevel;
    }

    void setLogLevel(LOG_LEVEL level)
    {
        logLevel = std::min(LOG_SILENT, std::max(LOG_DEBUG, level));
    }

    void setLogCallback(LOG_CALLBACK callback)
    {
        std::lock_guard<std::recursive_mutex> lock(logSync);
        logCallback = callback;
    }
}
