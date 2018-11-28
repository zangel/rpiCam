#pragma once

#include "../PixelSampleBuffer.hpp"
#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_buffer.h>

namespace rpiCam
{
    class RPIPixelSampleBuffer
        : public PixelSampleBuffer
    {
    public:
        RPIPixelSampleBuffer(MMAL_BUFFER_HEADER_T *buffer, Vec2ui const &size, ePixelFormat format);
        ~RPIPixelSampleBuffer();

        // Buffer overrides
        bool isValid() const override;
        void* data() override;
        std::size_t size() const override;

        // PixelBuffer overrides
        ePixelFormat format() const override;
        std::size_t planeCount() const override;
        Vec2ui planeSize(std::size_t pi = 0) const override;
        void* planeData(std::size_t pi = 0) override;
        std::size_t planeRowBytes(std::size_t pi = 0) const override;

        // SampleBuffer overrides
        std::error_code lock() override;
        std::error_code unlock() override;

    private:
        void updatePlanes();


    private:
        MMAL_BUFFER_HEADER_T *m_Buffer;
        Vec2ui m_Size;
        ePixelFormat m_Format;
        std::atomic<uint32_t> m_LockCounter;
        std::size_t m_PlaneCount;
        Vec2ui m_PlaneSizeShift[4];
        std::size_t m_PlaneDataOffset[4];
        std::size_t m_PlaneRowBytes[4];
    };
}
