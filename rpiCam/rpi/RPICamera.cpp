#include "RPICamera.hpp"
#include "RPIPixelSampleBuffer.hpp"
#include "../Logging.hpp"

#define MMAL_CAMERA_VIDEO_PORT          1
#define MMAL_CAMERA_SNAPSHOT_PORT       2

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
            if (cameras.empty())
            {
                RPI_LOG(DEBUG, "enumerateRPICameras(): found no cameras!");
            }
            return cameras;
        }
    }

    RPICamera::RPICamera(std::shared_ptr<MMAL_COMPONENT_T> camera, std::string const &name)
        : Camera()
        , m_Camera(camera)
        , m_VideoPort(m_Camera->output[MMAL_CAMERA_VIDEO_PORT])
        , m_SnapshotPort(m_Camera->output[MMAL_CAMERA_SNAPSHOT_PORT])
        , m_Name(name)
        , m_SupportedVideoFormats()
        , m_SupportedVideoSizes()
        , m_VideoFrameRateMin()
        , m_VideoFrameRateMax()
        , m_VideoFormat(kPixelFormatRGB8)
        , m_VideoSize(1920, 1080)
        , m_VideoFrameRate(30, 1)
        , m_SupportedSnapshotFormats()
        , m_SupportedSnapshotSizes()
        , m_SnapshotFormat(kPixelFormatRGB8)
        , m_SnapshotSize(2592, 1944)
        , m_bConfiguring(false)
        , m_bConfigurationChanged(false)
        , m_bTakingSnapshot(false)
        , m_VideoBufferPool(nullptr)
        , m_SnapshotBufferPool(nullptr)
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
        RPI_LOG(DEBUG, "Camera::open(): opening camera ...");
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

        if (std::error_code ispe = initializeSnapshotPort())
        {
            RPI_LOG(WARNING, "Camera::open(): initializeSnapshotPort() failed!");
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

        RPI_LOG(DEBUG, "Camera::open(): successfully opened camera!");

        return std::error_code();
    }

    std::error_code RPICamera::close()
    {
        RPI_LOG(DEBUG, "Camera::close(): closing camera ...");
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

        RPI_LOG(DEBUG, "Camera::close(): successfully closed camera!");

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
        RPI_LOG(DEBUG, "Camera::startVideo(): starting video ...");

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

        if (std::error_code evpe = enableVideoPort())
        {
            RPI_LOG(WARNING, "Camera::startVideo(): could not enable video port!");
            return std::make_error_code(std::errc::io_error);
        }

        if (std::error_code espe = enableSnapshotPort())
        {
            RPI_LOG(WARNING, "Camera::startVideo(): could not enable snapshot port!");
            disableVideoPort();
            return std::make_error_code(std::errc::io_error);
        }

        mmal_port_parameter_set_boolean(m_VideoPort, MMAL_PARAMETER_CAPTURE, MMAL_TRUE);

        dispatchOnCameraVideoStarted();

        RPI_LOG(DEBUG, "Camera::startVideo(): video started successfully!");

        return std::error_code();
    }


    bool RPICamera::isVideoStarted() const
    {
        return m_VideoPort->is_enabled;
    }

    std::error_code RPICamera::stopVideo()
    {
        RPI_LOG(DEBUG, "Camera::stopVideo(): stopping video ...");

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

        disableVideoPort();
        disableSnapshotPort();

        dispatchOnCameraVideoStopped();

        RPI_LOG(DEBUG, "Camera::stopVideo(): video stopped successfully!");

        return std::error_code();
    }

    std::list<ePixelFormat> const& RPICamera::getSupportedSnapshotFormats() const
    {
        return m_SupportedSnapshotFormats;
    }

    std::list<Vec2ui> const& RPICamera::getSupportedSnapshotSizes() const
    {
        return m_SupportedSnapshotSizes;
    }

    ePixelFormat RPICamera::getSnapshotFormat() const
    {
        return m_SnapshotFormat;
    }

    std::error_code RPICamera::setSnapshotFormat(ePixelFormat fmt)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);

        if (isVideoStarted())
            return std::make_error_code(std::errc::not_supported);

        if (std::error_code asfe = applySnapshotFormat(fmt))
        {
            applySnapshotFormat(m_SnapshotFormat);
            return asfe;
        }

        m_SnapshotFormat = fmt;
        m_bConfigurationChanged = true;

        return std::error_code();
    }

    Vec2ui RPICamera::getSnapshotSize() const
    {
        return m_SnapshotSize;
    }

    std::error_code RPICamera::setSnapshotSize(Vec2ui const &sz)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);

        if (isVideoStarted())
            return std::make_error_code(std::errc::not_supported);

        if (std::error_code asse = applySnapshotSize(sz))
        {
            applySnapshotSize(m_SnapshotSize);
            return asse;
        }

        m_SnapshotSize = sz;
        m_bConfigurationChanged = true;

        return std::error_code();
    }

    std::error_code RPICamera::takeSnapshot()
    {
        RPI_LOG(DEBUG, "Camera::takeSnapshot(): taking snapshot ...");

        if (!isOpen())
        {
            RPI_LOG(WARNING, "Camera::takeSnapshot(): camera is not open");
            return std::make_error_code(std::errc::not_connected);
        }

        if (!isVideoStarted())
        {
            RPI_LOG(WARNING, "Camera::takeSnapshot(): video is not started!");
            return std::make_error_code(std::errc::not_connected);
        }

        if (m_bTakingSnapshot)
        {
            RPI_LOG(DEBUG, "Camera::takeSnapshot(): previous snapshot taking is in progress!");
            return std::make_error_code(std::errc::connection_already_in_progress);
        }

        /*if (m_SnapshotBufferPool)
        {
            disableSnapshotPort();
        }
        enableSnapshotPort();
        */

        m_bTakingSnapshot = true;
        mmal_port_parameter_set_boolean(m_SnapshotPort, MMAL_PARAMETER_CAPTURE, MMAL_TRUE);

        //MMAL_PARAMETER_CAPTURE_STATUS_T

        RPI_LOG(DEBUG, "Camera::takeSnapshot(): snapshot taken successfully!");

        return std::error_code();
    }


    std::error_code RPICamera::applyVideoFormat(ePixelFormat fmt)
    {
        RPI_LOG(DEBUG, "Camera::applyVideoFormat(): applying video format ...");

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
        {
            RPI_LOG(WARNING, "Camera::applyVideoFormat(): mmal_port_format_commit(m_VideoPort) failed");
            return std::make_error_code(std::errc::io_error);
        }

        RPI_LOG(DEBUG, "Camera::applyVideoFormat(): video format applied successfully!");

        return std::error_code();
    }

    std::error_code RPICamera::applyVideoSize(Vec2ui const &sz)
    {
        RPI_LOG(DEBUG, "Camera::applyVideoSize(): applying video size ...");

        m_VideoPort->format->es->video.width = VCOS_ALIGN_UP(sz(0), 32);
        m_VideoPort->format->es->video.height = VCOS_ALIGN_UP(sz(1), 16);
        m_VideoPort->format->es->video.crop.x = 0;
        m_VideoPort->format->es->video.crop.y = 0;
        m_VideoPort->format->es->video.crop.width = sz(0);
        m_VideoPort->format->es->video.crop.height = sz(1);

        if (mmal_port_format_commit(m_VideoPort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::applyVideoSize(): mmal_port_format_commit(m_VideoPort) failed");
            return std::make_error_code(std::errc::io_error);
        }

        if (mmal_port_format_commit(m_SnapshotPort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::applyVideoSize(): mmal_port_format_commit(m_SnapshotPort) failed");
            return std::make_error_code(std::errc::io_error);
        }

        RPI_LOG(DEBUG, "Camera::applyVideoSize(): video size applied successfully!");

        return std::error_code();
    }

    std::error_code RPICamera::applyVideoFrameRate(Rational const &rate)
    {
        RPI_LOG(DEBUG, "Camera::applyVideoFrameRate(): applying video frame rate ...");

        m_VideoPort->format->es->video.frame_rate.num = rate.numerator;
        m_VideoPort->format->es->video.frame_rate.den = rate.denominator;

        if (mmal_port_format_commit(m_VideoPort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::applyVideoFrameRate(): mmal_port_format_commit(m_VideoPort) failed");
            return std::make_error_code(std::errc::io_error);
        }

        RPI_LOG(DEBUG, "Camera::applyVideoFrameRate(): video frame rate applied successfully!");

        return std::error_code();
    }

    std::error_code RPICamera::applySnapshotFormat(ePixelFormat fmt)
    {
        RPI_LOG(DEBUG, "Camera::applySnapshotFormat(): applying snapshot format ...");

        switch(fmt)
        {
        case kPixelFormatRGB8:
            m_SnapshotPort->format->encoding = MMAL_ENCODING_RGB24;
            m_SnapshotPort->format->encoding_variant = 0;
            break;

        case kPixelFormatYUV420:
            m_SnapshotPort->format->encoding = MMAL_ENCODING_I420;
            m_SnapshotPort->format->encoding_variant = 0;
            break;

        default:
            return std::make_error_code(std::errc::io_error);
            break;
        }

        if (mmal_port_format_commit(m_SnapshotPort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::applySnapshotFormat(): mmal_port_format_commit(m_SnapshotPort) failed");
            return std::make_error_code(std::errc::io_error);
        }

        RPI_LOG(DEBUG, "Camera::applySnapshotFormat(): snapshot format applied successfully!");

        return std::error_code();
    }

    std::error_code RPICamera::applySnapshotSize(Vec2ui const &sz)
    {
        RPI_LOG(DEBUG, "Camera::applySnapshotSize(): applying snapshot size ...");

        m_SnapshotPort->format->es->video.width = VCOS_ALIGN_UP(sz(0), 32);
        m_SnapshotPort->format->es->video.height = VCOS_ALIGN_UP(sz(1), 16);
        m_SnapshotPort->format->es->video.crop.x = 0;
        m_SnapshotPort->format->es->video.crop.y = 0;
        m_SnapshotPort->format->es->video.crop.width = sz(0);
        m_SnapshotPort->format->es->video.crop.height = sz(1);

        if (mmal_port_format_commit(m_SnapshotPort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::applySnapshotSize(): mmal_port_format_commit(m_SnapshotPort) failed");
            return std::make_error_code(std::errc::io_error);
        }

        RPI_LOG(DEBUG, "Camera::applySnapshotSize(): video size applied successfully!");

        return std::error_code();
    }


    std::error_code RPICamera::initializeCameraControlPort()
    {
        RPI_LOG(DEBUG, "Camera::initializeCameraControlPort(): initializing camera control port ...");

        MMAL_PARAMETER_CAMERA_CONFIG_T camConfig =
        {
            {
                MMAL_PARAMETER_CAMERA_CONFIG,
                sizeof(camConfig)
            },
            .max_stills_w = m_SnapshotSize(0),
            .max_stills_h = m_SnapshotSize(1),
            .stills_yuv422 = 0,
            .one_shot_stills = 1,
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

        RPI_LOG(DEBUG, "Camera::initializeCameraControlPort(): camera control port successfully initialized!");

        return std::error_code();
    }

    std::error_code RPICamera::initializeVideoPort()
    {
        RPI_LOG(DEBUG, "Camera::initializeVideoPort(): initializing video port ...");

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

        RPI_LOG(DEBUG, "Camera::initializeVideoPort(): video port successfully initialized!");

        return std::error_code();
    }

    std::error_code RPICamera::initializeSnapshotPort()
    {
        RPI_LOG(DEBUG, "Camera::initializeSnapshotPort): initializing capture ...");

        switch(m_SnapshotFormat)
        {
        case kPixelFormatRGB8:
            m_SnapshotPort->format->encoding = MMAL_ENCODING_RGB24;
            m_SnapshotPort->format->encoding_variant = 0;
            break;

        case kPixelFormatYUV420:
            m_SnapshotPort->format->encoding = MMAL_ENCODING_I420;
            m_SnapshotPort->format->encoding_variant = 0;
            break;

        default:
            {
                RPI_LOG(WARNING, "Camera::initializeSnapshotPort(): unsupported snapshot format %d", m_SnapshotFormat);
                return std::make_error_code(std::errc::io_error);
            }
            break;
        }

        m_SnapshotPort->format->es->video.width = VCOS_ALIGN_UP(m_SnapshotSize(0), 32);
        m_SnapshotPort->format->es->video.height = VCOS_ALIGN_UP(m_SnapshotSize(1), 16);
        m_SnapshotPort->format->es->video.crop.x = 0;
        m_SnapshotPort->format->es->video.crop.y = 0;
        m_SnapshotPort->format->es->video.crop.width = m_SnapshotSize(0);
        m_SnapshotPort->format->es->video.crop.height = m_SnapshotSize(1);
        m_SnapshotPort->format->es->video.frame_rate.num = 0;
        m_SnapshotPort->format->es->video.frame_rate.den = 1;

        if (mmal_port_format_commit(m_SnapshotPort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::initializeSnapshotPort(): mmal_port_format_commit() failed");
            return std::make_error_code(std::errc::io_error);
        }

        //if (m_SnapshotPort->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        //      m_SnapshotPort->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

        m_bTakingSnapshot = false;

        RPI_LOG(DEBUG, "Camera::initializeSnapshotPort(): snapshot port successfully initialized!");

        return std::error_code();
    }

    std::error_code RPICamera::enableVideoPort()
    {
        m_VideoBufferPool = mmal_port_pool_create(m_VideoPort, m_VideoPort->buffer_num, m_VideoPort->buffer_size);
        if (!m_VideoBufferPool)
        {
            RPI_LOG(WARNING, "Camera::enableVideoPort(): mmal_port_pool_create() failed!");
            return std::make_error_code(std::errc::no_buffer_space);
        }

        int const numVideoBuffers = mmal_queue_length(m_VideoBufferPool->queue);

        if (numVideoBuffers !=  m_VideoPort->buffer_num)
        {
            RPI_LOG(WARNING, "Camera::enableVideoPort(): m_VideoBufferPool length is zero!");
            mmal_port_pool_destroy(m_VideoPort, m_VideoBufferPool);
            m_VideoBufferPool = nullptr;
            return std::make_error_code(std::errc::io_error);
        }

        if (mmal_port_enable(m_VideoPort, RPICamera::_mmalCameraVideoBufferCallback) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::enableVideoPort(): mmal_port_enable() failed!");
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

        return std::error_code();
    }

    std::error_code RPICamera::enableSnapshotPort()
    {
        m_SnapshotBufferPool = mmal_port_pool_create(m_SnapshotPort, m_SnapshotPort->buffer_num, m_SnapshotPort->buffer_size);
        if (!m_SnapshotBufferPool)
        {
            RPI_LOG(WARNING, "Camera::enableSnapshotPort(): mmal_port_pool_create() failed!");
            return std::make_error_code(std::errc::no_buffer_space);
        }

        int const numSnapshotBuffers = mmal_queue_length(m_SnapshotBufferPool->queue);

        if (numSnapshotBuffers !=  m_SnapshotPort->buffer_num)
        {
            RPI_LOG(WARNING, "Camera::enableSnapshotPort(): m_SnapshotBufferPool length is zero!");
            mmal_port_pool_destroy(m_SnapshotPort, m_SnapshotBufferPool);
            m_SnapshotBufferPool = nullptr;
            return std::make_error_code(std::errc::io_error);
        }

        if (mmal_port_enable(m_SnapshotPort, RPICamera::_mmalCameraSnapshotBufferCallback) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::enableSnapshotPort(): mmal_port_enable() failed!");
            mmal_port_pool_destroy(m_SnapshotPort, m_SnapshotBufferPool);
            m_SnapshotBufferPool = nullptr;
            return std::make_error_code(std::errc::io_error);
        }

        for (int ivb=0; ivb < numSnapshotBuffers; ivb++)
        {
            MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(m_SnapshotBufferPool->queue);

            if (!buffer)
                continue;

            mmal_port_send_buffer(m_SnapshotPort, buffer);
        }

        return std::error_code();
    }

    void RPICamera::disableVideoPort()
    {
        mmal_port_disable(m_VideoPort);
        // flush all video buffers
        mmal_port_flush(m_VideoPort);
        //while(mmal_queue_length(m_VideoBufferPool->queue) < m_VideoPort->buffer_num)
        //    std::this_thread::yield();
        int const numBuffersInUse = (m_VideoPort->buffer_num - mmal_queue_length(m_VideoBufferPool->queue));
        if (numBuffersInUse)
        {
            RPI_LOG(WARNING, "Camera::disableVideoPort(): detected buffers in use after flush: %d!", numBuffersInUse);
        }

        mmal_port_pool_destroy(m_VideoPort, m_VideoBufferPool);
        m_VideoBufferPool = nullptr;
    }

    void RPICamera::disableSnapshotPort()
    {
        while (m_bTakingSnapshot)
            std::this_thread::yield();

        mmal_port_disable(m_SnapshotPort);
        // flush all snapshot buffers
        mmal_port_flush(m_SnapshotPort);
        //while(mmal_queue_length(m_SnapshotBufferPool->queue) < m_SnapshotPort->buffer_num)
        //    std::this_thread::yield();
        int const numBuffersInUse = (m_SnapshotPort->buffer_num - mmal_queue_length(m_SnapshotBufferPool->queue));
        if (numBuffersInUse)
        {
            RPI_LOG(WARNING, "Camera::disableSnapshotPort(): detected buffers in use after flush: %d!", numBuffersInUse);
        }

        mmal_port_pool_destroy(m_SnapshotPort, m_SnapshotBufferPool);
        m_SnapshotBufferPool = nullptr;
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
        if (m_VideoPort->is_enabled)
        {
            std::shared_ptr<PixelSampleBuffer> pixelSampleBuffer = std::make_shared<RPIPixelSampleBuffer>(
                buffer,
                m_VideoSize,
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

    void RPICamera::_mmalCameraSnapshotBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
    {
        RPICamera *rpiCamera = reinterpret_cast<RPICamera*>(port->component->userdata);
        if (!rpiCamera)
            return;

        rpiCamera->mmalCameraSnapshotBufferCallback(buffer);
    }

    void RPICamera::mmalCameraSnapshotBufferCallback(MMAL_BUFFER_HEADER_T *buffer)
    {
        if (m_SnapshotPort->is_enabled)
        {
            mmal_port_parameter_set_boolean(m_SnapshotPort, MMAL_PARAMETER_CAPTURE, MMAL_FALSE);
            if (m_bTakingSnapshot)
            {
                std::shared_ptr<PixelSampleBuffer> pixelSampleBuffer = std::make_shared<RPIPixelSampleBuffer>(
                    buffer,
                    m_SnapshotSize,
                    m_SnapshotFormat
                );
                dispatchOnCameraSnapshotTaken(pixelSampleBuffer);
            }
        }

        mmal_buffer_header_release(buffer);

        if (m_SnapshotPort->is_enabled)
        {
            MMAL_BUFFER_HEADER_T *newBuffer = mmal_queue_get(m_SnapshotBufferPool->queue);

            if (newBuffer)
                mmal_port_send_buffer(m_SnapshotPort, newBuffer);
        }
        m_bTakingSnapshot = false;
    }


    template <>
    std::list< std::shared_ptr<Camera> > Device::list<Camera>()
    {
        static std::list< std::shared_ptr<Camera> > cameras = enumerateRPICameras();
        return cameras;
    }
}
