#include "RPICamera.hpp"
#include "RPIPixelSampleBuffer.hpp"
#include "../Logging.hpp"

#define MMAL_CAMERA_PREVIEW_PORT        0
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
        , m_PreviewPort(m_Camera->output[MMAL_CAMERA_PREVIEW_PORT])
        , m_VideoPort(m_Camera->output[MMAL_CAMERA_VIDEO_PORT])
        , m_SnapshotPort(m_Camera->output[MMAL_CAMERA_SNAPSHOT_PORT])
        , m_Name(name)
        , m_SupportedVideoFormats()
        , m_SupportedVideoSizes()
        , m_VideoFrameRateMin()
        , m_VideoFrameRateMax()
        , m_VideoFormat(kPixelFormatYUV420)
        , m_VideoSize(1920, 1080)
        , m_VideoFrameRate(60, 1)
        , m_SupportedSnapshotFormats()
        , m_SupportedSnapshotSizes()
        , m_SnapshotFormat(kPixelFormatYUV420)
        , m_SnapshotSize(1920, 1080)
        , m_Brightness(0.5f)
        , m_Contrast(0.0f)
        , m_Sharpness(0.0f)
        , m_Saturation(0.0f)
        , m_ISO(0)
        , m_ShutterSpeed(std::chrono::seconds(0))
        , m_AWBMode(AWBMode::Auto)
        , m_ExposureCompensation(0)
        , m_ExposureMode(ExposureMode::Auto)
        , m_ExposureMeteringMode(ExposureMeteringMode::Average)
        , m_AnalogGain(0.0f)
        , m_DigitalGain(0.0f)
        , m_DRCStrength(DRCStrength::Off)
        , m_VideoStabilisation(false)
        , m_FlickerAvoid(FlickerAvoid::Off)
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

        /*
        if (std::error_code ippe = initializePreviewPort())
        {
            RPI_LOG(WARNING, "Camera::open(): initializePreviewPort() failed!");
            mmal_port_disable(m_Camera->control);
            return ippe;
        }
        */

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

        if (isTakingSnapshotsStarted())
            stopTakingSnapshots();

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

        if (isTakingSnapshotsStarted())
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

        if (isTakingSnapshotsStarted())
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

    std::error_code RPICamera::startTakingSnapshots()
    {
        RPI_LOG(DEBUG, "Camera::startTakingSnapshots(): starting snapshots taking ...");

        if (!isOpen())
        {
            RPI_LOG(WARNING, "Camera::startTakingSnapshots(): camera is not open");
            return std::make_error_code(std::errc::not_connected);
        }

        if (isTakingSnapshotsStarted())
        {
            RPI_LOG(WARNING, "Camera::startTakingSnapshots(): snapshots taking already started!");
            return std::make_error_code(std::errc::already_connected);
        }

        if (std::error_code espe = enableSnapshotPort())
        {
            RPI_LOG(WARNING, "Camera::startTakingSnapshots(): could not enable snapshot port!");
            return std::make_error_code(std::errc::io_error);
        }

        dispatchOnCameraTakingSnapshotsStarted();

        RPI_LOG(DEBUG, "Camera::startTakingSnapshots(): taking snapshots started successfully!");

        return std::error_code();
    }

    bool RPICamera::isTakingSnapshotsStarted() const
    {
        return m_SnapshotPort->is_enabled;
    }

    std::error_code RPICamera::stopTakingSnapshots()
    {
        RPI_LOG(DEBUG, "Camera::stopTakingSnapshots(): stopping taking snapshots ...");

        if (!isOpen())
        {
            RPI_LOG(WARNING, "Camera::stopTakingSnapshots(): camera is not open");
            return std::make_error_code(std::errc::not_connected);
        }

        if (!isTakingSnapshotsStarted())
        {
            RPI_LOG(WARNING, "Camera::stopTakingSnapshots(): taking snapshots is not started");
            return std::make_error_code(std::errc::not_connected);
        }

        disableSnapshotPort();

        dispatchOnCameraTakingSnapshotsStopped();

        RPI_LOG(DEBUG, "Camera::stopTakingSnapshots(): taking snapshots stopped successfully!");

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

        if (!isTakingSnapshotsStarted())
        {
            RPI_LOG(WARNING, "Camera::takeSnapshot(): taking snapshots is not started!");
            return std::make_error_code(std::errc::not_connected);
        }

        if (m_bTakingSnapshot)
        {
            RPI_LOG(DEBUG, "Camera::takeSnapshot(): previous snapshot taking is in progress!");
            return std::make_error_code(std::errc::connection_already_in_progress);
        }

        m_bTakingSnapshot = true;
        mmal_port_parameter_set_boolean(m_SnapshotPort, MMAL_PARAMETER_CAPTURE, MMAL_TRUE);

        RPI_LOG(DEBUG, "Camera::takeSnapshot(): snapshot taken successfully!");

        return std::error_code();
    }
    
    float RPICamera::getBrightness() const
    {
        return m_Brightness;
    }

    std::error_code RPICamera::setBrightness(float brightness)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accbe = applyCameraControlBrightness(brightness))
        {
            applyCameraControlBrightness(m_Brightness);
            return accbe;
        }
        
        m_Brightness = brightness;
        m_bConfigurationChanged = true;
        return std::error_code();
    }

    float RPICamera::getContrast() const
    {
        return m_Contrast;
    }

    std::error_code RPICamera::setContrast(float contrast)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accce = applyCameraControlContrast(contrast))
        {
            applyCameraControlContrast(m_Contrast);
            return accce;
        }
        
        m_Contrast = contrast;
        m_bConfigurationChanged = true;
        return std::error_code();
    }


    float RPICamera::getSharpness() const
    {
        return m_Sharpness;
    }

    std::error_code RPICamera::setSharpness(float sharpness)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accse = applyCameraControlSharpness(sharpness))
        {
            applyCameraControlSharpness(m_Sharpness);
            return accse;
        }
        
        m_Sharpness = sharpness;
        m_bConfigurationChanged = true;
        return std::error_code();
    }

    float RPICamera::getSaturation() const
    {
        return m_Saturation;
    }

    std::error_code RPICamera::setSaturation(float saturation)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accse = applyCameraControlSaturation(saturation))
        {
            applyCameraControlSaturation(m_Saturation);
            return accse;
        }
        
        m_Saturation = saturation;
        m_bConfigurationChanged = true;
        return std::error_code();
    }


    int RPICamera::getISO() const
    {
        return m_ISO;
    }

    std::error_code RPICamera::setISO(int ISO)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accie = applyCameraControlISO(ISO))
        {
            applyCameraControlISO(m_ISO);
            return accie;
        }
        
        m_ISO = ISO;
        m_bConfigurationChanged = true;
        return std::error_code();
    }

    Duration RPICamera::getShutterSpeed() const
    {
        return m_ShutterSpeed;
    }

    std::error_code RPICamera::setShutterSpeed(Duration shutterSpeed)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accsse = applyCameraControlShutterSpeed(shutterSpeed))
        {
            applyCameraControlShutterSpeed(m_ShutterSpeed);
            return accsse;
        }
        
        m_ShutterSpeed = shutterSpeed;
        m_bConfigurationChanged = true;
        return std::error_code();
    }

    Camera::AWBMode RPICamera::getAWBMode() const
    {
        return m_AWBMode;
    }

    std::error_code RPICamera::setAWBMode(AWBMode awbMode)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accae = applyCameraControlAWBMode(awbMode))
        {
            applyCameraControlAWBMode(m_AWBMode);
            return accae;
        }
        
        m_AWBMode = awbMode;
        m_bConfigurationChanged = true;
        return std::error_code();
    }


    int RPICamera::getExposureCompensation() const
    {
        return m_ExposureCompensation;
    }

    std::error_code RPICamera::setExposureCompensation(int exposureCompensation)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accece = applyCameraControlExposureCompensation(exposureCompensation))
        {
            applyCameraControlExposureCompensation(m_ExposureCompensation);
            return accece;
        }
        
        m_ExposureCompensation = exposureCompensation;
        m_bConfigurationChanged = true;
        return std::error_code();
    }


    Camera::ExposureMode RPICamera::getExposureMode() const
    {
        return m_ExposureMode;
    }

    std::error_code RPICamera::setExposureMode(ExposureMode exposureMode)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code acceme = applyCameraControlExposureMode(exposureMode))
        {
            applyCameraControlExposureMode(m_ExposureMode);
            return acceme;
        }
        
        m_ExposureMode = exposureMode;
        m_bConfigurationChanged = true;
        return std::error_code();
    }


    Camera::ExposureMeteringMode RPICamera::getExposureMeteringMode() const
    {
        return m_ExposureMeteringMode;
    }

    std::error_code RPICamera::setExposureMeteringMode(ExposureMeteringMode exposureMeteringMode)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accemme = applyCameraControlExposureMeteringMode(exposureMeteringMode))
        {
            applyCameraControlExposureMeteringMode(m_ExposureMeteringMode);
            return accemme;
        }
        
        m_ExposureMeteringMode = exposureMeteringMode;
        m_bConfigurationChanged = true;
        return std::error_code();
    }

    float RPICamera::getAnalogGain() const
    {
        return m_AnalogGain;
    }

    std::error_code RPICamera::setAnalogGain(float gain)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accage = applyCameraControlAnalogGain(gain))
        {
            applyCameraControlAnalogGain(m_AnalogGain);
            return accage;
        }
        
        m_AnalogGain = gain;
        m_bConfigurationChanged = true;
        return std::error_code();
    }


    float RPICamera::getDigitalGain() const
    {
        return m_DigitalGain;
    }

    std::error_code RPICamera::setDigitalGain(float gain)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accdge = applyCameraControlDigitalGain(gain))
        {
            applyCameraControlDigitalGain(m_DigitalGain);
            return accdge;
        }
        
        m_DigitalGain = gain;
        m_bConfigurationChanged = true;
        return std::error_code();
    }


    Camera::DRCStrength RPICamera::getDRCStrength() const
    {
        return m_DRCStrength;
    }

    std::error_code RPICamera::setDRCStrength(DRCStrength strength)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accdse = applyCameraControlDRCStrength(strength))
        {
            applyCameraControlDRCStrength(m_DRCStrength);
            return accdse;
        }
        
        m_DRCStrength = strength;
        m_bConfigurationChanged = true;
        return std::error_code();
    }

    bool RPICamera::getVideoStabilisation() const
    {
        return m_VideoStabilisation;
    }

    std::error_code RPICamera::setVideoStabilisation(bool videoStabilisation)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accvse = applyCameraControlVideoStabilisation(videoStabilisation))
        {
            applyCameraControlVideoStabilisation(m_VideoStabilisation);
            return accvse;
        }
        
        m_VideoStabilisation = videoStabilisation;
        m_bConfigurationChanged = true;
        return std::error_code();
    }


    Camera::FlickerAvoid RPICamera::getFlickerAvoid() const
    {
        return m_FlickerAvoid;
    }

    std::error_code RPICamera::setFlickerAvoid(FlickerAvoid flickerAvoid)
    {
        if (!isOpen())
            return std::make_error_code(std::errc::not_connected);
            
        if (std::error_code accfae = applyCameraControlFlickerAvoid(flickerAvoid))
        {
            applyCameraControlFlickerAvoid(m_FlickerAvoid);
            return accfae;
        }
        
        m_FlickerAvoid = flickerAvoid;
        m_bConfigurationChanged = true;
        return std::error_code();
	}

    std::error_code RPICamera::applyCameraControlSize(Vec2ui const &vsz, Vec2ui const &ssz)
    {
        MMAL_PARAMETER_CAMERA_CONFIG_T camConfig =
        {
            {
                MMAL_PARAMETER_CAMERA_CONFIG,
                sizeof(camConfig)
            },
            .max_stills_w = ssz(0),
            .max_stills_h = ssz(1),
            .stills_yuv422 = 0,
            .one_shot_stills = 1,
            .max_preview_video_w = vsz(0),
            .max_preview_video_h = vsz(1),
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

        return std::error_code();
    }
    
    std::error_code RPICamera::applyCameraControlBrightness(float brightness)
    {
        MMAL_RATIONAL_T value = { static_cast<int>(brightness * 100), 100 };
        if (mmal_port_parameter_set_rational(m_Camera->control, MMAL_PARAMETER_BRIGHTNESS, value) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }
    
    std::error_code RPICamera::applyCameraControlContrast(float contrast)
    {
        MMAL_RATIONAL_T value = { static_cast<int>(contrast * 100), 100 };
        if (mmal_port_parameter_set_rational(m_Camera->control, MMAL_PARAMETER_CONTRAST, value) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }
    
    std::error_code RPICamera::applyCameraControlSharpness(float sharpness)
    {
        MMAL_RATIONAL_T value = { static_cast<int>(sharpness * 100), 100 };
        if (mmal_port_parameter_set_rational(m_Camera->control, MMAL_PARAMETER_SHARPNESS, value) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }

    std::error_code RPICamera::applyCameraControlSaturation(float saturation)
    {
        MMAL_RATIONAL_T value = { static_cast<int>(saturation * 100), 100 };
        if (mmal_port_parameter_set_rational(m_Camera->control, MMAL_PARAMETER_SATURATION, value) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }

    std::error_code RPICamera::applyCameraControlISO(int ISO)
    {
        if (mmal_port_parameter_set_uint32(m_Camera->control, MMAL_PARAMETER_ISO, ISO) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }
    
    std::error_code RPICamera::applyCameraControlShutterSpeed(Duration shutterSpeed)
    {
        int value = std::chrono::duration_cast<std::chrono::microseconds>(shutterSpeed).count();
        if (mmal_port_parameter_set_uint32(m_Camera->control, MMAL_PARAMETER_SHUTTER_SPEED, value) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }
    
    std::error_code RPICamera::applyCameraControlAWBMode(AWBMode awbMode)
    {
        MMAL_PARAM_AWBMODE_T value = MMAL_PARAM_AWBMODE_T(MMAL_PARAM_AWBMODE_OFF + (static_cast<int>(awbMode) - static_cast<int>(AWBMode::Off)));
        MMAL_PARAMETER_AWBMODE_T param = { { MMAL_PARAMETER_AWB_MODE, sizeof(param) }, value };
        if (mmal_port_parameter_set(m_Camera->control, &param.hdr) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }
    
    std::error_code RPICamera::applyCameraControlExposureCompensation(int exposureCompensation)
    {
        if (mmal_port_parameter_set_int32(m_Camera->control, MMAL_PARAMETER_EXPOSURE_COMP, exposureCompensation) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }
    
    std::error_code RPICamera::applyCameraControlExposureMode(ExposureMode exposureMode)
    {
        MMAL_PARAM_EXPOSUREMODE_T value = MMAL_PARAM_EXPOSUREMODE_T(MMAL_PARAM_EXPOSUREMODE_OFF + (static_cast<int>(exposureMode) - static_cast<int>(ExposureMode::Off)));
        MMAL_PARAMETER_EXPOSUREMODE_T param = { { MMAL_PARAMETER_EXPOSURE_MODE, sizeof(param) }, value };
        if (mmal_port_parameter_set(m_Camera->control, &param.hdr) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }
    
    std::error_code RPICamera::applyCameraControlExposureMeteringMode(ExposureMeteringMode exposureMeteringMode)
    {
        MMAL_PARAM_EXPOSUREMETERINGMODE_T value = MMAL_PARAM_EXPOSUREMETERINGMODE_T(MMAL_PARAM_EXPOSUREMETERINGMODE_AVERAGE + (static_cast<int>(exposureMeteringMode) - static_cast<int>(ExposureMeteringMode::Average)));
        MMAL_PARAMETER_EXPOSUREMETERINGMODE_T param = { { MMAL_PARAMETER_EXP_METERING_MODE, sizeof(param) }, value };
        if (mmal_port_parameter_set(m_Camera->control, &param.hdr) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }
    
    std::error_code RPICamera::applyCameraControlAnalogGain(float gain)
    {
        MMAL_RATIONAL_T value = { static_cast<int>(gain * 65536), 65536 };
        if (mmal_port_parameter_set_rational(m_Camera->control, MMAL_PARAMETER_ANALOG_GAIN, value) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }

    std::error_code RPICamera::applyCameraControlDigitalGain(float gain)
    {
        MMAL_RATIONAL_T value = { static_cast<int>(gain * 65536), 65536 };
        if (mmal_port_parameter_set_rational(m_Camera->control, MMAL_PARAMETER_DIGITAL_GAIN, value) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }

    std::error_code RPICamera::applyCameraControlDRCStrength(DRCStrength drc)
    {
        MMAL_PARAMETER_DRC_STRENGTH_T value = MMAL_PARAMETER_DRC_STRENGTH_T(MMAL_PARAMETER_DRC_STRENGTH_OFF + (static_cast<int>(drc) - static_cast<int>(DRCStrength::Off)));
        MMAL_PARAMETER_DRC_T param = { { MMAL_PARAMETER_DYNAMIC_RANGE_COMPRESSION, sizeof(param) }, value };
        if (mmal_port_parameter_set(m_Camera->control, &param.hdr) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }

    std::error_code RPICamera::applyCameraControlVideoStabilisation(bool videoStabilisation)
    {
        if (mmal_port_parameter_set_boolean(m_Camera->control, MMAL_PARAMETER_VIDEO_STABILISATION, videoStabilisation?MMAL_TRUE:MMAL_FALSE) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
        return std::error_code();
    }
    
    std::error_code RPICamera::applyCameraControlFlickerAvoid(FlickerAvoid flickerAvoid)
    {
        MMAL_PARAM_FLICKERAVOID_T value = MMAL_PARAM_FLICKERAVOID_T(MMAL_PARAM_FLICKERAVOID_OFF + (static_cast<int>(flickerAvoid) - static_cast<int>(FlickerAvoid::Off)));
        MMAL_PARAMETER_FLICKERAVOID_T param = { { MMAL_PARAMETER_FLICKER_AVOID, sizeof(param) }, value };
        if (mmal_port_parameter_set(m_Camera->control, &param.hdr) != MMAL_SUCCESS)
            return std::make_error_code(std::errc::invalid_argument);
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

        if (mmal_component_disable(m_Camera.get()) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::applyVideoSize(): mmal_component_disable() failed");
            return std::make_error_code(std::errc::io_error);
        }

        if (auto accse = applyCameraControlSize(sz, m_SnapshotSize))
        {
            RPI_LOG(WARNING, "Camera::applyVideoSize(): applyCameraControlSize() failed");
            applyCameraControlSize(m_VideoSize, m_SnapshotSize);
            mmal_component_enable(m_Camera.get());
            return accse;
        }

        m_VideoPort->format->es->video.width = VCOS_ALIGN_UP(sz(0), 32);
        m_VideoPort->format->es->video.height = VCOS_ALIGN_UP(sz(1), 16);
        m_VideoPort->format->es->video.crop.x = 0;
        m_VideoPort->format->es->video.crop.y = 0;
        m_VideoPort->format->es->video.crop.width = sz(0);
        m_VideoPort->format->es->video.crop.height = sz(1);

        if (mmal_port_format_commit(m_VideoPort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::applyVideoSize(): mmal_port_format_commit(m_VideoPort) failed");
            applyCameraControlSize(m_VideoSize, m_SnapshotSize);
            mmal_component_enable(m_Camera.get());
            return std::make_error_code(std::errc::io_error);
        }

        mmal_component_enable(m_Camera.get());

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

        if (mmal_component_disable(m_Camera.get()) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::applySnapshotSize(): mmal_component_disable() failed");
            return std::make_error_code(std::errc::io_error);
        }

        if (auto accse = applyCameraControlSize(m_VideoSize, sz))
        {
            RPI_LOG(WARNING, "Camera::applySnapshotSize(): applyCameraControlSize() failed");
            applyCameraControlSize(m_VideoSize, m_SnapshotSize);
            mmal_component_enable(m_Camera.get());
            return accse;
        }

        m_SnapshotPort->format->es->video.width = VCOS_ALIGN_UP(sz(0), 32);
        m_SnapshotPort->format->es->video.height = VCOS_ALIGN_UP(sz(1), 16);
        m_SnapshotPort->format->es->video.crop.x = 0;
        m_SnapshotPort->format->es->video.crop.y = 0;
        m_SnapshotPort->format->es->video.crop.width = sz(0);
        m_SnapshotPort->format->es->video.crop.height = sz(1);

        if (mmal_port_format_commit(m_SnapshotPort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::applySnapshotSize(): mmal_port_format_commit(m_SnapshotPort) failed");
            applyCameraControlSize(m_VideoSize, m_SnapshotSize);
            mmal_component_enable(m_Camera.get());
            return std::make_error_code(std::errc::io_error);
        }

        mmal_component_enable(m_Camera.get());

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

        applyCameraControlBrightness(m_Brightness);
        applyCameraControlContrast(m_Contrast);
        applyCameraControlSharpness(m_Sharpness);
        applyCameraControlSaturation(m_Saturation);
        applyCameraControlISO(m_ISO);
        applyCameraControlShutterSpeed(m_ShutterSpeed);
        applyCameraControlAWBMode(m_AWBMode);
        applyCameraControlExposureCompensation(m_ExposureCompensation);
        applyCameraControlExposureMode(m_ExposureMode);
        applyCameraControlExposureMeteringMode(m_ExposureMeteringMode);
        applyCameraControlAnalogGain(m_AnalogGain);
        applyCameraControlDigitalGain(m_DigitalGain);
        applyCameraControlDRCStrength(m_DRCStrength);
        applyCameraControlVideoStabilisation(m_VideoStabilisation);
        applyCameraControlFlickerAvoid(m_FlickerAvoid);
        
        RPI_LOG(DEBUG, "Camera::initializeCameraControlPort(): camera control port successfully initialized!");

        return std::error_code();
    }

    std::error_code RPICamera::initializePreviewPort()
    {
        RPI_LOG(DEBUG, "Camera::initializeVideoPort(): initializing preview port ...");

        /*
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
        */

        switch(m_VideoFormat)
        {
        case kPixelFormatRGB8:
            m_PreviewPort->format->encoding = MMAL_ENCODING_RGB24;
            m_PreviewPort->format->encoding_variant = 0;
            break;

        case kPixelFormatYUV420:
            m_PreviewPort->format->encoding = MMAL_ENCODING_I420;
            m_PreviewPort->format->encoding_variant = 0;
            break;

        default:
            {
                RPI_LOG(WARNING, "Camera::initializePreviewPort(): unsupported video format %d", m_VideoFormat);
                return std::make_error_code(std::errc::io_error);
            }
            break;
        }

        m_PreviewPort->format->es->video.width = VCOS_ALIGN_UP(m_VideoSize(0), 32);
        m_PreviewPort->format->es->video.height = VCOS_ALIGN_UP(m_VideoSize(1), 16);
        m_PreviewPort->format->es->video.crop.x = 0;
        m_PreviewPort->format->es->video.crop.y = 0;
        m_PreviewPort->format->es->video.crop.width = m_VideoSize(0);
        m_PreviewPort->format->es->video.crop.height = m_VideoSize(1);
        m_PreviewPort->format->es->video.frame_rate.num = 0;//m_VideoFrameRate.numerator;
        m_PreviewPort->format->es->video.frame_rate.den = 1;//m_VideoFrameRate.denominator;

        if (mmal_port_format_commit(m_PreviewPort) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::initializePreviewPort(): mmal_port_format_commit() failed");
            return std::make_error_code(std::errc::io_error);
        }

        //m_PreviewPort->buffer_num = std::min(m_PreviewPort->buffer_num_recommended, uint32_t(VIDEO_OUTPUT_BUFFERS_NUM));
        /*
        if (m_VideoPort->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
              m_VideoPort->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;
        */

        if (mmal_port_parameter_set_boolean(m_PreviewPort, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE) != MMAL_SUCCESS)
        {
            RPI_LOG(WARNING, "Camera::initializePreviewPort(): mmal_port_parameter_set_boolean() failed");
            return std::make_error_code(std::errc::io_error);
        }

        RPI_LOG(DEBUG, "Camera::initializePreviewPort(): video port successfully initialized!");

        return std::error_code();
    }

    std::error_code RPICamera::initializeVideoPort()
    {
        RPI_LOG(DEBUG, "Camera::initializeVideoPort(): initializing video port ...");

        /*
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
        */

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


        //m_VideoPort->buffer_num = std::min(m_VideoPort->buffer_num_recommended, uint32_t(VIDEO_OUTPUT_BUFFERS_NUM));
        //if (m_VideoPort->buffer_num < VIDEO_OUTPUT_BUFFERS_NUM)
        //      m_VideoPort->buffer_num = VIDEO_OUTPUT_BUFFERS_NUM;

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
                    Vec2ui(m_SnapshotPort->format->es->video.width, m_SnapshotPort->format->es->video.height),
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
