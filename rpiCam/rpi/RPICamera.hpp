#pragma once

#include "../Camera.hpp"

#include <interface/mmal/mmal.h>
#include <interface/mmal/mmal_logging.h>
#include <interface/mmal/mmal_buffer.h>
#include <interface/mmal/util/mmal_util.h>
#include <interface/mmal/util/mmal_util_params.h>
#include <interface/mmal/util/mmal_default_components.h>
#include <interface/mmal/util/mmal_connection.h>

namespace rpiCam
{
    class RPICamera
        : public Camera
    {
    public:
        RPICamera(std::shared_ptr<MMAL_COMPONENT_T> camera, std::string const &name);
        ~RPICamera();

        // Device overrides
        std::string const& name() const override;

        bool isOpen() const override;
        std::error_code open() override;
        std::error_code close() override;

        // Camera overrides
        std::error_code beginConfiguration() override;
        std::error_code endConfiguration() override;

        std::list<ePixelFormat> const& getSupportedVideoFormats() const override;
        std::list<Vec2ui> const& getSupportedVideoSizes() const override;
        Rational getVideoFrameRateMin() const override;
        Rational getVideoFrameRateMax() const override;
        ePixelFormat getVideoFormat() const override;
        std::error_code setVideoFormat(ePixelFormat fmt) override;
        Vec2ui getVideoSize() const override;
        std::error_code setVideoSize(Vec2ui const &sz) override;
        Rational getVideoFrameRate() const override;
        std::error_code setVideoFrameRate(Rational const &rate) override;

        std::error_code startVideo() override;
        bool isVideoStarted() const override;
        std::error_code stopVideo() override;

        std::list<ePixelFormat> const& getSupportedSnapshotFormats() const override;
        std::list<Vec2ui> const& getSupportedSnapshotSizes() const override;
        ePixelFormat getSnapshotFormat() const override;
        std::error_code setSnapshotFormat(ePixelFormat fmt) override;
        Vec2ui getSnapshotSize() const override;
        std::error_code setSnapshotSize(Vec2ui const &sz) override;

        std::error_code startTakingSnapshots() override;
        bool isTakingSnapshotsStarted() const override;
        std::error_code stopTakingSnapshots() override;

        std::error_code takeSnapshot() override;

        float getBrightness() const override;
        std::error_code setBrightness(float brightness) override;

        float getContrast() const override;
        std::error_code setContrast(float contrast) override;

        float getSharpness() const override;
        std::error_code setSharpness(float sharpness) override;

        float getSaturation() const override;
        std::error_code setSaturation(float saturation) override;

        int getISO() const override;
        std::error_code setISO(int ISO) override;

        Duration getShutterSpeed() const override;
        std::error_code setShutterSpeed(Duration shutterSpeed) override;

        AWBMode getAWBMode() const override;
        std::error_code setAWBMode(AWBMode awbMode) override;

        int getExposureCompensation() const override;
        std::error_code setExposureCompensation(int exposureCompensation) override;

        ExposureMode getExposureMode() const override;
        std::error_code setExposureMode(ExposureMode exposureMode) override;

        ExposureMeteringMode getExposureMeteringMode() const override;
        std::error_code setExposureMeteringMode(ExposureMeteringMode exposureMeteringMode) override;

        float getAnalogGain() const override;
        std::error_code setAnalogGain(float gain) override;

        float getDigitalGain() const override;
        std::error_code setDigitalGain(float gain) override;

        DRCStrength getDRCStrength() const override;
        std::error_code setDRCStrength(DRCStrength strength) override;

        bool  getVideoStabilisation() const override;
        std::error_code setVideoStabilisation(bool videoStabilisation) override;

        FlickerAvoid getFlickerAvoid() const override;
        std::error_code setFlickerAvoid(FlickerAvoid flickerAvoid) override;

        std::error_code enableRecording() override;
        bool isRecordingEnabled() const override;
        std::error_code disableRecording() override;

    private:
        std::error_code applyCameraControlSize(Vec2ui const &vsz, Vec2ui const &ssz);
        std::error_code applyCameraControlBrightness(float brightness);
        std::error_code applyCameraControlContrast(float contrast);
        std::error_code applyCameraControlSharpness(float sharpness);
        std::error_code applyCameraControlSaturation(float saturation);
        std::error_code applyCameraControlISO(int ISO);
        std::error_code applyCameraControlShutterSpeed(Duration shutterSpeed);
        std::error_code applyCameraControlAWBMode(AWBMode awbMode);
        std::error_code applyCameraControlExposureCompensation(int exposureCompensation);
        std::error_code applyCameraControlExposureMode(ExposureMode exposureMode);
        std::error_code applyCameraControlExposureMeteringMode(ExposureMeteringMode exposureMeteringMode);
        std::error_code applyCameraControlAnalogGain(float gain);
        std::error_code applyCameraControlDigitalGain(float gain);
        std::error_code applyCameraControlDRCStrength(DRCStrength drc);
        std::error_code applyCameraControlVideoStabilisation(bool videoStabilisation);
        std::error_code applyCameraControlFlickerAvoid(FlickerAvoid flickerAvoid);
        
        std::error_code applyVideoFormat(ePixelFormat fmt);
        std::error_code applyVideoSize(Vec2ui const &sz);
        std::error_code applyVideoFrameRate(Rational const &rate);

        std::error_code applySnapshotFormat(ePixelFormat fmt);
        std::error_code applySnapshotSize(Vec2ui const &sz);

        std::error_code initializeCameraControlPort();
        std::error_code initializePreviewPort();
        std::error_code initializeVideoPort();
        std::error_code initializeSnapshotPort();

        std::error_code enableVideoPort();
        std::error_code enableSnapshotPort();

        void disableVideoPort();
        void disableSnapshotPort();

        std::error_code createEncoder();
        std::error_code enableEncoder();
        void disableEncoder();
        void destroyEncoder();

        static void _mmalCameraControlCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
        void mmalCameraControlCallback(MMAL_BUFFER_HEADER_T *buffer);

        static void _mmalCameraVideoBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
        void mmalCameraVideoBufferCallback(MMAL_BUFFER_HEADER_T *buffer);

        static void _mmalCameraSnapshotBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
        void mmalCameraSnapshotBufferCallback(MMAL_BUFFER_HEADER_T *buffer);

        static void _mmalEncoderBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
        void mmalEncoderBufferCallback(MMAL_BUFFER_HEADER_T *buffer);

    private:
        std::shared_ptr<MMAL_COMPONENT_T> m_Camera;
        MMAL_PORT_T *m_PreviewPort;
        MMAL_PORT_T *m_VideoPort;
        MMAL_PORT_T *m_SnapshotPort;
        std::shared_ptr<MMAL_COMPONENT_T> m_Encoder;
        MMAL_PORT_T *m_EncoderInputPort;
        MMAL_PORT_T *m_EncoderOutputPort;
        MMAL_CONNECTION_T *m_EncoderInputConnection;
        std::string m_Name;
        std::list<ePixelFormat> m_SupportedVideoFormats;
        std::list<Vec2ui> m_SupportedVideoSizes;
        Rational m_VideoFrameRateMin;
        Rational m_VideoFrameRateMax;
        ePixelFormat m_VideoFormat;
        Vec2ui m_VideoSize;
        Rational m_VideoFrameRate;
        std::list<ePixelFormat> m_SupportedSnapshotFormats;
        std::list<Vec2ui> m_SupportedSnapshotSizes;
        ePixelFormat m_SnapshotFormat;
        Vec2ui m_SnapshotSize;
        float m_Brightness;
        float m_Contrast;
        float m_Sharpness;
        float m_Saturation;
        int m_ISO;
        Duration m_ShutterSpeed;
        AWBMode m_AWBMode;
        int m_ExposureCompensation;
        ExposureMode m_ExposureMode;
        ExposureMeteringMode m_ExposureMeteringMode;
        float m_AnalogGain;
        float m_DigitalGain;
        DRCStrength m_DRCStrength;
        bool m_VideoStabilisation;
        FlickerAvoid m_FlickerAvoid;

        bool m_RecordingEnabled;
        Vec2ui m_RecordingSize;
                
        bool m_bConfiguring;
        bool m_bConfigurationChanged;
        bool m_bTakingSnapshot;

        MMAL_POOL_T *m_VideoBufferPool;
        MMAL_POOL_T *m_SnapshotBufferPool;
        MMAL_POOL_T *m_EncoderBufferPool;
    };
}
