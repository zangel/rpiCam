#include "RPICamera.hpp"

#define MMAL_CAMERA_PREVIEW_PORT 0
#define MMAL_CAMERA_VIDEO_PORT 1
#define MMAL_CAMERA_CAPTURE_PORT 2


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
    , m_PreviewPort(m_Camera->output[MMAL_CAMERA_PREVIEW_PORT])
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
    if (isOpen())
        return std::make_error_code(std::errc::already_connected);

    MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T changeEventRequest =
        {{MMAL_PARAMETER_CHANGE_EVENT_REQUEST, sizeof(MMAL_PARAMETER_CHANGE_EVENT_REQUEST_T)}, MMAL_PARAMETER_CAMERA_SETTINGS, 1};

    if (mmal_port_parameter_set(m_Camera->control, &changeEventRequest.hdr) != MMAL_SUCCESS)
        return std::make_error_code(std::errc::io_error);

    mmal_port_enable(m_Camera->control, &RPICamera::_mmalCameraControlCallback);

    if (std::error_code acce = applyCameraConfiguration())
    {
        mmal_port_disable(m_Camera->control);
        return acce;
    }

    if (std::error_code appce = applyPreviewPortConfiguration())
    {
        mmal_port_disable(m_Camera->control);
        return appce;
    }

    if (std::error_code avpce = applyVideoPortConfiguration())
    {
        mmal_port_disable(m_Camera->control);
        return avpce;
    }

    return std::error_code();
}

std::error_code RPICamera::close()
{
    if (!isOpen())
        return std::make_error_code(std::errc::not_connected);



    m_Camera.reset();

    return std::error_code();
}

std::error_code RPICamera::beginConfiguration()
{
    return std::error_code();
}

std::error_code RPICamera::endConfiguration()
{
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
    return std::error_code();
}

Vec2ui RPICamera::getVideoSize() const
{
    return m_VideoSize;
}

std::error_code RPICamera::setVideoSize(Vec2ui const &sz)
{
    return std::error_code();
}

Rational RPICamera::getVideoFrameRate() const
{
    return m_VideoFrameRate;
}

std::error_code RPICamera::setVideoFrameRate(Rational const &rate)
{
    return std::error_code();
}

std::error_code RPICamera::startVideo()
{
    return std::error_code();
}


bool RPICamera::isVideoStarted() const
{
    return false;
}

std::error_code RPICamera::stopVideo()
{
    return std::error_code();
}

std::error_code RPICamera::applyCameraConfiguration()
{
    MMAL_PARAMETER_CAMERA_CONFIG_T camConfig =
    {
        { MMAL_PARAMETER_CAMERA_CONFIG, sizeof(camConfig) },
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
        return std::make_error_code(std::errc::io_error);

    return std::error_code();
}

std::error_code RPICamera::applyPreviewPortConfiguration()
{
    MMAL_PARAMETER_FPS_RANGE_T fpsRange =
    {
        {MMAL_PARAMETER_FPS_RANGE, sizeof(fpsRange)},
        {m_VideoFrameRate.numerator, m_VideoFrameRate.denominator},
        {m_VideoFrameRate.numerator, m_VideoFrameRate.denominator}
    };

    if (mmal_port_parameter_set(m_PreviewPort, &fpsRange.hdr) != MMAL_SUCCESS)
        return std::make_error_code(std::errc::io_error);

    switch(m_VideoFormat)
    {
    case kPixelFormatRGB8:
        m_PreviewPort->format->encoding = MMAL_ENCODING_RGB24;
        m_PreviewPort->format->encoding_variant = 0;
        break;

    case kPixelFormatYUV420sp:
        m_PreviewPort->format->encoding = MMAL_ENCODING_NV12;
        m_PreviewPort->format->encoding_variant = 0;
        break;

    default:
        return std::make_error_code(std::errc::io_error);
        break;
    }

    m_PreviewPort->format->es->video.width = VCOS_ALIGN_UP(m_VideoSize(0), 32);
    m_PreviewPort->format->es->video.height = VCOS_ALIGN_UP(m_VideoSize(1), 16);
    m_PreviewPort->format->es->video.crop.x = 0;
    m_PreviewPort->format->es->video.crop.y = 0;
    m_PreviewPort->format->es->video.crop.width = m_VideoSize(0);
    m_PreviewPort->format->es->video.crop.height = m_VideoSize(1);
    m_PreviewPort->format->es->video.frame_rate.num = m_VideoFrameRate.numerator;
    m_PreviewPort->format->es->video.frame_rate.den = m_VideoFrameRate.denominator;

    if (mmal_port_format_commit(m_PreviewPort) != MMAL_SUCCESS)
        return std::make_error_code(std::errc::io_error);

}

std::error_code RPICamera::applyVideoPortConfiguration()
{
    MMAL_PARAMETER_FPS_RANGE_T fpsRange =
    {
        {MMAL_PARAMETER_FPS_RANGE, sizeof(fpsRange)},
        {m_VideoFrameRate.numerator, m_VideoFrameRate.denominator},
        {m_VideoFrameRate.numerator, m_VideoFrameRate.denominator}
    };

    if (mmal_port_parameter_set(m_VideoPort, &fpsRange.hdr) != MMAL_SUCCESS)
        return std::make_error_code(std::errc::io_error);

    switch(m_VideoFormat)
    {
    case kPixelFormatRGB8:
        m_VideoPort->format->encoding = MMAL_ENCODING_RGB24;
        m_VideoPort->format->encoding_variant = 0;
        break;

    case kPixelFormatYUV420sp:
        m_VideoPort->format->encoding = MMAL_ENCODING_NV12;
        m_VideoPort->format->encoding_variant = 0;
        break;

    default:
        return std::make_error_code(std::errc::io_error);
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
        return std::make_error_code(std::errc::io_error);

    return std::error_code();

}

void RPICamera::_mmalCameraControlCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
    RPICamera *rpiCamera = reinterpret_cast<RPICamera*>(port->component->userdata);
    if (rpiCamera)
    {
        rpiCamera->mmalCameraControlCallback(port, buffer);
    }
    mmal_buffer_header_release(buffer);
}

void RPICamera::mmalCameraControlCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{

}

template <>
std::list< std::shared_ptr<Camera> > Device::list<Camera>()
{
    static std::list< std::shared_ptr<Camera> > cameras = enumerateRPICameras();
    return cameras;
}
