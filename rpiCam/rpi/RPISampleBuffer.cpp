#include "RPISampleBuffer.hpp"

namespace rpiCam
{
    RPISampleBuffer::RPISampleBuffer(MMAL_BUFFER_HEADER_T *buffer)
        : SampleBuffer()
        , m_Buffer(buffer)
        , m_LockCounter(0)
    {
        time = TimeClock::now();

        if (m_Buffer)
            mmal_buffer_header_acquire(m_Buffer);
    }

    RPISampleBuffer::~RPISampleBuffer()
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

    bool RPISampleBuffer::isValid() const
    {
        return m_Buffer;
    }

    void* RPISampleBuffer::data()
    {
        if (isValid() && m_LockCounter)
            return m_Buffer->data;

        return nullptr;
    }

    std::size_t RPISampleBuffer::size() const
    {
        if (isValid())
            return m_Buffer->length;

        return 0;
    }

    std::error_code RPISampleBuffer::lock()
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


    std::error_code RPISampleBuffer::unlock()
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
}
