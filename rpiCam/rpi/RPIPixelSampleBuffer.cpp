#include "RPIPixelSampleBuffer.hpp"

namespace rpiCam
{
    RPIPixelSampleBuffer::RPIPixelSampleBuffer(MMAL_BUFFER_HEADER_T *buffer, Vec2ui const &size, ePixelFormat format)
        : PixelSampleBuffer()
        , m_Buffer(buffer)
        , m_Size(size)
        , m_Format(format)
        , m_LockCounter(0)
        , m_PlaneCount(0)
    {
        time = TimeClock::now();

        if (m_Buffer)
            mmal_buffer_header_acquire(m_Buffer);

        updatePlanes();
    }

    RPIPixelSampleBuffer::~RPIPixelSampleBuffer()
    {
        if (m_Buffer)
        {
            if (m_LockCounter)
            {
                mmal_buffer_header_mem_unlock(m_Buffer);
            }
            mmal_buffer_header_release(m_Buffer);
        }
    }

    bool RPIPixelSampleBuffer::isValid() const
    {
        return m_Buffer;
    }

    void* RPIPixelSampleBuffer::data()
    {
        if (isValid() && m_LockCounter)
            return m_Buffer->data;

        return nullptr;
    }

    std::size_t RPIPixelSampleBuffer::size() const
    {
        if (isValid())
            return m_Buffer->alloc_size;

        return 0;
    }

    ePixelFormat RPIPixelSampleBuffer::format() const
    {
        return isValid() ? m_Format : kPixelFormatInvalid;
    }

    std::size_t RPIPixelSampleBuffer::planeCount() const
    {
        return m_PlaneCount;
    }

    Vec2ui RPIPixelSampleBuffer::planeSize(std::size_t pi) const
    {
        if (!isValid() || pi >= m_PlaneCount)
            return Vec2ui(0,0);

        return Vec2ui(
            m_Size(0) >> m_PlaneSizeShift[pi][0],
            m_Size(1) >> m_PlaneSizeShift[pi][1]
        );
    }

    void* RPIPixelSampleBuffer::planeData(std::size_t pi)
    {
        if (!isValid() || pi >= m_PlaneCount || !m_LockCounter)
            return nullptr;

        return m_Buffer->data + m_PlaneDataOffset[pi];
    }

    std::size_t RPIPixelSampleBuffer::planeRowBytes(std::size_t pi) const
    {
        if (!isValid() || pi >= m_PlaneCount)
            return 0;

        return m_PlaneRowBytes[pi];
    }

    std::error_code RPIPixelSampleBuffer::lock()
    {
        if (!isValid())
            return std::make_error_code(std::errc::no_lock_available);

        if(m_LockCounter++)
            return std::error_code();

        if (mmal_buffer_header_mem_lock(m_Buffer) != MMAL_SUCCESS)
        {
            m_LockCounter--;
            return std::make_error_code(std::errc::no_lock_available);
        }

        return std::error_code();
    }


    std::error_code RPIPixelSampleBuffer::unlock()
    {
        if(!isValid())
            return std::make_error_code(std::errc::no_lock_available);

        if(!m_LockCounter)
            return std::make_error_code(std::errc::no_lock_available);

        if(--m_LockCounter)
            return std::error_code();

        mmal_buffer_header_mem_unlock(m_Buffer);

        return std::error_code();
    }

    void RPIPixelSampleBuffer::updatePlanes()
    {
        switch(m_Format)
        {
        case kPixelFormatRGB8:
            {
                m_PlaneCount = 1;
                m_PlaneSizeShift[0] = Vec2ui(0,0);
                m_PlaneDataOffset[0] = 0;
                m_PlaneRowBytes[0] = m_Size(0) * 3;
            }
            break;

        case kPixelFormatYUV420:
            {
                m_PlaneCount = 2;
                m_PlaneSizeShift[0] = Vec2ui(0,0);
                m_PlaneSizeShift[1] = Vec2ui(1,1);
                m_PlaneDataOffset[0] = 0;
                m_PlaneDataOffset[1] = m_Size(1) * m_Size(0);
                m_PlaneRowBytes[0] = m_Size(0);
                m_PlaneRowBytes[1] = m_Size(0);
            }
            break;

        default:
            break;
        }
    }
}
