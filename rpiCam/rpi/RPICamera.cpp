#include "RPICamera.hpp"
#include "RPIPixelSampleBuffer.hpp"
#include "../Logging.hpp"

#define MMAL_CAMERA_VIDEO_PORT          1
#define MMAL_CAMERA_CAPTURE_PORT        2

#define VIDEO_OUTPUT_BUFFERS_NUM        3

namespace rpiCam
{

    namespace
    {
        std::list< std::shared_ptr<Camera> > enumerateRPICameras()
        {
            std::list< std::shared_ptr<Camera> >  cameras;
            for(std::int32_t ic = 0; ic < 10; ++ic)
            {
                MMAL_COMPONENT_T *camera = nullptr;
                if (mmal_component_create(MMAL_COMPONENT_DEFAULT_CAMERA, &camera) != MMAL_SUCCESS)
                    break;

                std::shared_ptr<MMAL_COMPONENT_T> spCamera = std::shared_ptr<MMAL_COMPONENT_T>(camera, mmal_component_destroy);

                MMAL_PARAMETER_INT32_T cameraNum = {{MMAL_PARAMETER_CAMERA_NUM, sizeof(cameraNum)}, ic};
                if (mmal_port_parameter_set(spCamera->control, &cameraNum.hdr) != MMAL_SUCCESS)
                {
                    break;
                }

                if (!spCamera->output_num)
                {
                    continue;
                }

                std::string name = "Camera";
                name += std::to_string(ic);

                cameras.push_back(std::make_shared<RPICamera>(spCamera, name));
            }
            return cameras;
        }
    }

    RPICamera::RPICamera(std::shared_ptr<MMAL_COMPONENT_T> camera, std::string const &name)
        : Camera()
        , m_Camera(camera)
        , m_VideoPort(m_Camera->output[MMAL_CAMERA_VIDEO_PORT])
        , m_CapturePort(m_Camera->output[MMAL_CAMERA_CAPTURE_PORT])
        , m_Name(name)
        , m_SupportedVideoFormats()
        , m_SupportedVideoSizes()
        , m_VideoFrameRateMin()
        , m_VideoFrameRateMax()
        , m_VideoFormat(kPixelFormatRGB8)
        , m_VideoSize(1920, 1080)
        , m_VideoFrameRate(30, 1)
        , m_bConfiguring(false)
        , m_bConfigurationChanged(false)
    {
        m_Camera->userdata = reinterpret_cast<struct MMAL_COMPONENT_USERDATA_T*>(this);
    }

    RPICamera::~RPICamera()
    {
        m_Camera->userdata = nullptr;
    }

    std::string const& RPICamera::name() const
    {
        return m_Name;
    }

    bool RPICamera::isOpen() const
    {
        return m_Camera->is_enabled;
    }

    std::error_code RPICamera::open()
    {
        RPI_LOG(DEBUG, "Camera::open(): opening camera '%s' ...", m_Name.c_str());
        if (isOpen())
        {
            RPI_LOG(WARNING, "Camera::open(): camera is already opened!");
            return std::make_error_code(std::errc::already_connected);
        }

        MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T changeEventRequest =
            {{MMAL_PARAMETER_CHANGE_EVENT_REQUEST, sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T)}, MMAL_PARAMETER_CAMERA_SETTINGS, 1};

        if (mmal_port_parameter_set(m_Camera->control, &changeEventRequest.hdr) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::open(): mmal_port_parameter_set() failed!");
            return std::make_error_code(std::errc::io_error);
        }

        if (mmal_port_enable(m_Camera->control, &RPICamera::_mmalCameraControlCallback) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::open(): mmal_port_enable() failed!");
            return std::make_error_code(std::errc::io_error);
        }

        if (std::error_code acce = initializeCameraControlPort())
        {
            RPI_LOG(WARNING, "Camera::open(): initializeCameraControlPort() failed!");
            mmal_port_disable(m_Camera->control);
            return acce;
        }

        if (std::error_code ivpe = initializeVideoPort())
        {
            RPI_LOG(WARNING, "Camera::open(): initializeVideoPort() failed!");
            mmal_port_disable(m_Camera->control);
            return ivpe;
        }

        if (std::error_code ispe = initializeCapturePort())
        {
            RPI_LOG(WARNING, "Camera::open(): initializeCapturePort() failed!");
            mmal_port_disable(m_Camera->control);
            return ispe;
        }

        if (mmal_component_enable(m_Camera.get()) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::open(): mmal_component_enable() failed!");
            mmal_port_disable(m_Camera->control);
            return std::make_error_code(std::errc::io_error);
        }

        dispatchOnDeviceOpened();

        RPI_LOG(DEBUG, "Camera::open(): successfully opened camera '%s'!", m_Name.c_str());

        return std::error_code();
    }

    std::error_code RPICamera::close()
    {
        RPI_LOG(DEBUG, "Camera::close(): closing camera '%s' ...", m_Name.c_str());
        if (!isOpen())
        {
            RPI_LOG(WARNING, "Camera::open(): camera is not open");
            return std::make_error_code(std::errc::not_connected);
        }

        if (isVideoStarted())
            stopVideo();

        mmal_component_disable(m_Camera.get());
        mmal_port_disable(m_Camera->control);

        dispatchOnDeviceClosed();

        RPI_LOG(DEBUG, "Camera::close(): successfully closed camera '%s'!", m_Name.c_str());

        return std::error_code();
    }

    std::error_code RPICamera::beginConfiguration()
    {
        if (m_bConfiguring)
            return std::make_error_code(std::errc::not_supported);

        m_bConfiguring = true;
        m_bConfigurationChanged = false;

        return std::error_code();
    }

    std::error_code RPICamera::endConfiguration()
    {
        if (!m_bConfiguring)
            return std::make_error_code(std::errc::not_supported);

        if (m_bConfigurationChanged)
        {
            dispatchOnCameraConfigurationChanged();
        }
        m_bConfiguring = false;
        m_bConfigurationChanged = false;
        return std::error_code();
    }

    std::list<ePixelFormat> const& RPICamera::getSupportedVideoFormats() const
    {
        return m_SupportedVideoFormats;
    }

    std::list<Vec2ui> const& RPICamera::getSupportedVideoSizes() const
    {
        return m_SupportedVideoSizes;

    }

    Rational RPICamera::getVideoFrameRateMin() const
    {
        return m_VideoFrameRateMin;
    }

    Rational RPICamera::getVideoFrameRateMax() const
    {
        return m_VideoFrameRateMax;
    }

    ePixelFormat RPICamera::getVideoFormat() const
    {
        return m_VideoFormat;
    }

    std::error_code RPICamera::setVideoFormat(ePixelFormat fmt)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);

        if (isVideoStarted())
            return std::make_error_code(std::errc::not_supported);

        if (std::error_code avfe = applyVideoFormat(fmt))
        {
            applyVideoFormat(m_VideoFormat);
            return avfe;
        }

        m_VideoFormat = fmt;
        m_bConfigurationChanged = true;

        return std::error_code();
    }

    Vec2ui RPICamera::getVideoSize() const
    {
        return m_VideoSize;
    }

    std::error_code RPICamera::setVideoSize(Vec2ui const &sz)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);

        if (isVideoStarted())
            return std::make_error_code(std::errc::not_supported);

        if (std::error_code avse = applyVideoSize(sz))
        {
            applyVideoSize(m_VideoSize);
            return avse;
        }

        m_VideoSize = sz;
        m_bConfigurationChanged = true;

        return std::error_code();
    }

    Rational RPICamera::getVideoFrameRate() const
    {
        return m_VideoFrameRate;
    }

    std::error_code RPICamera::setVideoFrameRate(Rational const &rate)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);

        if (isVideoStarted())
            return std::make_error_code(std::errc::not_supported);

        if (std::error_code avfre = applyVideoFrameRate(rate))
        {
            applyVideoFrameRate(m_VideoFrameRate);
            return avfre;
        }

        m_VideoFrameRate = rate;
        m_bConfigurationChanged = true;

        return std::error_code();
    }

    std::error_code RPICamera::startVideo()
    {
        RPI_LOG(DEBUG, "Camera::startVideo(): starting video on camera '%s' ...", m_Name.c_str());

        if (!isOpen())
        {
            RPI_LOG(WARNING, "Camera::startVideo(): camera is not open");
            return std::make_error_code(std::errc::not_connected);
        }

        if (isVideoStarted())
        {
            RPI_LOG(WARNING, "Camera::startVideo(): video already started!");
            return std::make_error_code(std::errc::already_connected);
        }

        m_VideoBufferPool = mmal_port_pool_create(m_VideoPort, m_VideoPort->buffer_num, m_VideoPort->buffer_size);
        if (!m_VideoBufferPool)
        {
            RPI_LOG(WARNING, "Camera::startVideo(): mmal_port_pool_create() failed!");
            return std::make_error_code(std::errc::no_buffer_space);
        }

        int const numVideoBuffers = mmal_queue_length(m_VideoBufferPool->queue);

        if (numVideoBuffers !=  m_VideoPort->buffer_num)
        {
            RPI_LOG(WARNING, "Camera::startVideo(): m_VideoBufferPool length is zero!");
            mmal_port_pool_destroy(m_VideoPort, m_VideoBufferPool);
            m_VideoBufferPool = nullptr;
            return std::make_error_code(std::errc::io_error);
        }

        if (mmal_port_enable(m_VideoPort, RPICamera::_mmalCameraVideoBufferCallback) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::startVideo(): mmal_port_enable() failed!");
            mmal_port_pool_destroy(m_VideoPort, m_VideoBufferPool);
            m_VideoBufferPool = nullptr;
            return std::make_error_code(std::errc::io_error);
        }

        for (int ivb=0; ivb < numVideoBuffers; ivb++)
        {
            MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(m_VideoBufferPool->queue);

            if (!buffer)
                continue;

            mmal_port_send_buffer(m_VideoPort, buffer);
        }

        mmal_port_parameter_set_boolean(m_VideoPort, MMAL_PARAMETER_CAPTURE, MMAL_TRUE);

        dispatchOnCameraVideoStarted();

        RPI_LOG(DEBUG, "Camera::startVideo(): video started successfully on camera '%s'!", m_Name.c_str());

        return std::error_code();
    }


    bool RPICamera::isVideoStarted() const
    {
        return m_VideoPort->is_enabled;
    }

    std::error_code RPICamera::stopVideo()
    {
        RPI_LOG(DEBUG, "Camera::stopVideo(): stopping video on camera '%s' ...", m_Name.c_str());

        if (!isOpen())
        {
            RPI_LOG(WARNING, "Camera::stopVideo(): camera is not open");
            return std::make_error_code(std::errc::not_connected);
        }

        if (!isVideoStarted())
        {
            RPI_LOG(WARNING, "Camera::stopVideo(): video is not started");
            return std::make_error_code(std::errc::not_connected);
        }

        mmal_port_parameter_set_boolean(m_VideoPort, MMAL_PARAMETER_CAPTURE, MMAL_FALSE);

        mmal_port_disable(m_VideoPort);
        while(mmal_queue_length(m_VideoBufferPool->queue) < m_VideoPort->buffer_num)
            std::this_thread::yield();

        mmal_port_pool_destroy(m_VideoPort, m_VideoBufferPool);
        m_VideoBufferPool = nullptr;

        dispatchOnCameraVideoStopped();

        RPI_LOG(DEBUG, "Camera::stopVideo(): video stopped successfully on camera '%s'!", m_Name.c_str());

        return std::error_code();
    }

    std::error_code RPICamera::applyVideoFormat(ePixelFormat fmt)
    {
        switch(fmt)
        {
        case kPixelFormatRGB8:
            m_VideoPort->format->encoding = MMAL_ENCODING_RGB24;
            m_VideoPort->format->encoding_variant = 0;
            break;

        case kPixelFormatYUV420:
            m_VideoPort->format->encoding = MMAL_ENCODING_I420;
            m_VideoPort->format->encoding_variant = 0;
            break;

        default:
            return std::make_error_code(std::errc::io_error);
            break;
        }

        if (mmal_port_format_commit(m_VideoPort) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::io_error);

        return std::error_code();
    }

    std::error_code RPICamera::applyVideoSize(Vec2ui const &sz)
    {
        RPI_LOG(DEBUG, "Camera::applyVideoSize(): applying video size on camera '%s' ...", m_Name.c_str());

        m_VideoPort->format->es->video.width = VCOS_ALIGN_UP(sz(0), 32);
        m_VideoPort->format->es->video.height = VCOS_ALIGN_UP(sz(1), 16);
        m_VideoPort->format->es->video.crop.x = 0;
        m_VideoPort->format->es->video.crop.y = 0;
        m_VideoPort->format->es->video.crop.width = sz(0);
        m_VideoPort->format->es->video.crop.height = sz(1);

        m_CapturePort->format->es->video.width = VCOS_ALIGN_UP(sz(0), 32);
        m_CapturePort->format->es->video.height = VCOS_ALIGN_UP(sz(1), 16);
        m_CapturePort->format->es->video.crop.x = 0;
        m_CapturePort->format->es->video.crop.y = 0;
        m_CapturePort->format->es->video.crop.width = sz(0);
        m_CapturePort->format->es->video.crop.height = sz(1);


        if (mmal_port_format_commit(m_VideoPort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::applyVideoSize(): mmal_port_format_commit(m_VideoPort) failed");
            return std::make_error_code(std::errc::io_error);
        }

        if (mmal_port_format_commit(m_CapturePort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::applyVideoSize(): mmal_port_format_commit(m_CapturePort) failed");
            return std::make_error_code(std::errc::io_error);
        }

        RPI_LOG(DEBUG, "Camera::applyVideoSize(): video size applied successfully on camera '%s' ...", m_Name.c_str());

        return std::error_code();
    }

    std::error_code RPICamera::applyVideoFrameRate(Rational const &rate)
    {
        RPI_LOG(DEBUG, "Camera::applyVideoFrameRate(): applying video frame rate on camera '%s' ...", m_Name.c_str());

        m_VideoPort->format->es->video.frame_rate.num = rate.numerator;
        m_VideoPort->format->es->video.frame_rate.den = rate.denominator;

        if (mmal_port_format_commit(m_VideoPort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::applyVideoFrameRate(): mmal_port_format_commit(m_VideoPort) failed");
            return std::make_error_code(std::errc::io_error);
        }

        RPI_LOG(DEBUG, "Camera::applyVideoFrameRate(): video frame rate applied successfully on camera '%s' ...", m_Name.c_str());

        return std::error_code();
    }


    std::error_code RPICamera::initializeCameraControlPort()
    {
        RPI_LOG(DEBUG, "Camera::initializeCameraControlPort(): initializing camera control port on '%s' ...", m_Name.c_str());

        MMAL_PARAMETER_CAMERA_CONFIG_T camConfig =
        {
            {
                MMAL_PARAMETER_CAMERA_CONFIG,
                sizeof(camConfig)
            },
            .max_stills_w = m_VideoSize(0),
            .max_stills_h = m_VideoSize(1),
            .stills_yuv422 = 0,
            .one_shot_stills = 0,
            .max_preview_video_w = m_VideoSize(0),
            .max_preview_video_h = m_VideoSize(1),
            .num_preview_video_frames = 3,
            .stills_capture_circular_buffer_height = 0,
            .fast_preview_resume = 0,
            .use_stc_timestamp = MMAL_PARAM_TIMESTAMP_MODE_RESET_STC
        };

        if (mmal_port_parameter_set(m_Camera->control, &camConfig.hdr) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::initializeCameraControlPort(): mmal_port_parameter_set(m_VideoPort) failed");
            return std::make_error_code(std::errc::io_error);
        }

        RPI_LOG(DEBUG, "Camera::initializeCameraControlPort(): camera control port successfully initialized on '%s' ...", m_Name.c_str());

        return std::error_code();
    }

    std::error_code RPICamera::initializeVideoPort()
    {
        RPI_LOG(DEBUG, "Camera::initializeVideoPort(): initializing video port on '%s' ...", m_Name.c_str());

        MMAL_PARAMETER_FPS_RANGE_T fpsRange =
        {
            {MMAL_PARAMETER_FPS_RANGE, sizeof(fpsRange)},
            {m_VideoFrameRate.numerator, m_VideoFrameRate.denominator},
            {m_VideoFrameRate.numerator, m_VideoFrameRate.denominator}
        };

        if (mmal_port_parameter_set(m_VideoPort, &fpsRange.hdr) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::initializeVideoPort(): mmal_port_parameter_set(m_VideoPort) failed");
            return std::make_error_code(std::errc::io_error);
        }

        switch(m_VideoFormat)
        {
        case kPixelFormatRGB8:
            m_VideoPort->format->encoding = MMAL_ENCODING_RGB24;
            m_VideoPort->format->encoding_variant = 0;
            break;

        case kPixelFormatYUV420:
            m_VideoPort->format->encoding = MMAL_ENCODING_I420;
            m_VideoPort->format->encoding_variant = 0;
            break;

        default:
            {
                RPI_LOG(WARNING, "Camera::initializeVideoPort(): unsupported video format %d", m_VideoFormat);
                return std::make_error_code(std::errc::io_error);
            }
            break;
        }

        m_VideoPort->format->es->video.width = VCOS_ALIGN_UP(m_VideoSize(0), 32);
        m_VideoPort->format->es->video.height = VCOS_ALIGN_UP(m_VideoSize(1), 16);
        m_VideoPort->format->es->video.crop.x = 0;
        m_VideoPort->format->es->video.crop.y = 0;
        m_VideoPort->format->es->video.crop.width = m_VideoSize(0);
        m_VideoPort->format->es->video.crop.height = m_VideoSize(1);
        m_VideoPort->format->es->video.frame_rate.num = m_VideoFrameRate.numerator;
        m_VideoPort->format->es->video.frame_rate.den = m_VideoFrameRate.denominator;

        if (mmal_port_format_commit(m_VideoPort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::initializeVideoPort(): mmal_port_format_commit() failed");
            return std::make_error_code(std::errc::io_error);
        }

        if (m_VideoPort->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
              m_VideoPort->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

        if (mmal_port_parameter_set_boolean(m_VideoPort, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::initializeVideoPort(): mmal_port_parameter_set_boolean() failed");
            return std::make_error_code(std::errc::io_error);
        }

        RPI_LOG(DEBUG, "Camera::initializeVideoPort(): video port successfully initialized on '%s' ...", m_Name.c_str());

        return std::error_code();
    }

    std::error_code RPICamera::initializeCapturePort()
    {
        RPI_LOG(DEBUG, "Camera::initializeCapturePort(): initializing capture port on '%s' ...", m_Name.c_str());

        switch(m_VideoFormat)
        {
        case kPixelFormatRGB8:
            m_CapturePort->format->encoding = MMAL_ENCODING_RGB24;
            m_CapturePort->format->encoding_variant = 0;
            break;

        case kPixelFormatYUV420:
            m_CapturePort->format->encoding = MMAL_ENCODING_I420;
            m_CapturePort->format->encoding_variant = 0;
            break;

        default:
            {
                RPI_LOG(WARNING, "Camera::initializeCapturePort(): unsupported video format %d", m_VideoFormat);
                return std::make_error_code(std::errc::io_error);
            }
            break;
        }

        m_CapturePort->format->es->video.width = VCOS_ALIGN_UP(m_VideoSize(0), 32);
        m_CapturePort->format->es->video.height = VCOS_ALIGN_UP(m_VideoSize(1), 16);
        m_CapturePort->format->es->video.crop.x = 0;
        m_CapturePort->format->es->video.crop.y = 0;
        m_CapturePort->format->es->video.crop.width = m_VideoSize(0);
        m_CapturePort->format->es->video.crop.height = m_VideoSize(1);
        m_CapturePort->format->es->video.frame_rate.num = 1;
        m_CapturePort->format->es->video.frame_rate.den = 1;

        if (mmal_port_format_commit(m_CapturePort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::initializeCapturePort(): mmal_port_format_commit() failed");
            return std::make_error_code(std::errc::io_error);
        }

        if (m_CapturePort->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
              m_CapturePort->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;


        RPI_LOG(DEBUG, "Camera::initializeCapturePort(): capture port successfully initialized on '%s' ...", m_Name.c_str());

        return std::error_code();
    }

    void RPICamera::_mmalCameraControlCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
    {
        RPICamera *rpiCamera = reinterpret_cast<RPICamera*>(port->component->userdata);
        if (!rpiCamera)
            return;

        rpiCamera->mmalCameraControlCallback(buffer);
    }

    void RPICamera::mmalCameraControlCallback(MMAL_BUFFER_HEADER_T *buffer)
    {
        mmal_buffer_header_release(buffer);
    }

    void RPICamera::_mmalCameraVideoBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
    {
        RPICamera *rpiCamera = reinterpret_cast<RPICamera*>(port->component->userdata);
        if (!rpiCamera)
            return;

        rpiCamera->mmalCameraVideoBufferCallback(buffer);
    }

    void RPICamera::mmalCameraVideoBufferCallback(MMAL_BUFFER_HEADER_T *buffer)
    {
        {
            std::shared_ptr<PixelSampleBuffer> pixelSampleBuffer = std::make_shared<RPIPixelSampleBuffer>(
                buffer,
                Vec2ui(m_VideoPort->format->es->video.width, m_VideoPort->format->es->video.height),
                m_VideoFormat
            );
            dispatchOnCameraVideoFrame(pixelSampleBuffer);
        }

        mmal_buffer_header_release(buffer);

        if (!m_VideoPort->is_enabled)
            return;

        MMAL_BUFFER_HEADER_T *newBuffer = mmal_queue_get(m_VideoBufferPool->queue);

        if (newBuffer)
            mmal_port_send_buffer(m_VideoPort, newBuffer);
    }

    template <>
    std::list< std::shared_ptr<Camera> > Device::list<Camera>()
    {
        static std::list< std::shared_ptr<Camera> > cameras = enumerateRPICameras();
        return cameras;
    }
}
