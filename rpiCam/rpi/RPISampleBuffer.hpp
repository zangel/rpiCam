#pragma once

#include "../SampleBuffer.hpp"
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_buffer.h>

namespace rpiCam
{
    class RPISampleBuffer
        : public SampleBuffer
    {
    public:
        RPISampleBuffer(MMAL_BUFFER_HEADER_T *buffer);
        ~RPISampleBuffer();

        // Buffer overrides
        bool isValid() const override;
        void* data() override;
        std::size_t size() const override;

        // SampleBuffer overrides
        std::error_code lock() override;
        std::error_code unlock() override;

    private:

    private:
        MMAL_BUFFER_HEADER_T *m_Buffer;
        std::atomic<uint32_t> m_LockCounter;
    };
}
