#pragma once

#include "Config.hpp"
#include "Device.hpp"
#include "PixelFormat.hpp"
#include "PixelSampleBuffer.hpp"
#include "EventsDispatcher.hpp"
#include "Rational.hpp"

namespace rpiCam
{
    class Camera
        : public Device
    {
    public:
        class Events
        {
        public:
            virtual void onCameraConfigurationChanged() {}
            virtual void onCameraVideoStarted() {}
            virtual void onCameraVideoStopped() {}
            virtual void onCameraTakingSnapshotsStarted() {}
            virtual void onCameraTakingSnapshotsStopped() {}
            virtual void onCameraRecordingStarted() {}
            virtual void onCameraRecordingStopped() {}
            virtual void onCameraVideoFrame(std::shared_ptr<PixelSampleBuffer> const &buffer) {}
            virtual void onCameraSnapshotTaken(std::shared_ptr<PixelSampleBuffer> const &buffer) {}
            virtual void onCameraRecordingBuffer(std::shared_ptr<SampleBuffer> const &buffer, std::uint32_t flags) {}
        };

        enum class RecordingBufferFlags : std::uint32_t
        {
            FrameEnd              = (1 << 0),
            KeyFrame              = (1 << 1),
            Config                = (1 << 2)
        };

        enum class AWBMode : int
        {
            Off,
            Auto,
            Sunlight,
            Cloudy,
            Shade,
            Tungsten,
            Fluorescent,
            Incandescent,
            Flash,
            Horizon
        };

        enum class ExposureMode : int
        {
            Off,
            Auto,
            Night,
            NightPreview,
            Backlight,
            Spotlight,
            Sports,
            Snow,
            Beach,
            VeryLong,
            FixedFPS,
            AntiShake,
            Fireworks
        };

        enum class ExposureMeteringMode : int
        {
            Average,
            Spot,
            Backlit,
            Matrix
        };

        enum class DRCStrength : int
        {
            Off,
            Low,
            Medium,
            High
        };

        enum class FlickerAvoid : int
        {
            Off,
            Auto,
            At50Hz,
            At60Hz
        };

        using CameraEvents = EventsDispatcher<Events>;

        Camera();
        virtual ~Camera();

        virtual std::error_code beginConfiguration() = 0;
        virtual std::error_code endConfiguration() = 0;

        virtual std::list<ePixelFormat> const& getSupportedVideoFormats() const = 0;
        virtual std::list<Vec2ui> const& getSupportedVideoSizes() const = 0;
        virtual Rational getVideoFrameRateMin() const = 0;
        virtual Rational getVideoFrameRateMax() const = 0;
        virtual ePixelFormat getVideoFormat() const = 0;
        virtual std::error_code setVideoFormat(ePixelFormat fmt) = 0;
        virtual Vec2ui getVideoSize() const = 0;
        virtual std::error_code setVideoSize(Vec2ui const &sz) = 0;
        virtual Rational getVideoFrameRate() const = 0;
        virtual std::error_code setVideoFrameRate(Rational const &rate) = 0;

        virtual std::error_code startVideo() = 0;
        virtual bool isVideoStarted() const = 0;
        virtual std::error_code stopVideo() = 0;

        virtual std::list<ePixelFormat> const& getSupportedSnapshotFormats() const = 0;
        virtual std::list<Vec2ui> const& getSupportedSnapshotSizes() const = 0;
        virtual ePixelFormat getSnapshotFormat() const = 0;
        virtual std::error_code setSnapshotFormat(ePixelFormat fmt) = 0;
        virtual Vec2ui getSnapshotSize() const = 0;
        virtual std::error_code setSnapshotSize(Vec2ui const &sz) = 0;

        virtual std::error_code startTakingSnapshots() = 0;
        virtual bool isTakingSnapshotsStarted() const = 0;
        virtual std::error_code stopTakingSnapshots() = 0;

        virtual std::error_code takeSnapshot() = 0;

        virtual float getBrightness() const = 0;
        virtual std::error_code setBrightness(float brightness) = 0;

        virtual float getContrast() const = 0;
        virtual std::error_code setContrast(float contrast) = 0;

        virtual float getSharpness() const = 0;
        virtual std::error_code setSharpness(float sharpness) = 0;

        virtual float getSaturation() const = 0;
        virtual std::error_code setSaturation(float saturation) = 0;

        virtual int getISO() const = 0;
        virtual std::error_code setISO(int ISO) = 0;

        virtual Duration getShutterSpeed() const = 0;
        virtual std::error_code setShutterSpeed(Duration shutterSpeed) = 0;

        virtual AWBMode getAWBMode() const = 0;
        virtual std::error_code setAWBMode(AWBMode awbMode) = 0;

        virtual int getExposureCompensation() const = 0;
        virtual std::error_code setExposureCompensation(int exposureCompensation) = 0;

        virtual ExposureMode getExposureMode() const = 0;
        virtual std::error_code setExposureMode(ExposureMode exposureMode) = 0;

        virtual ExposureMeteringMode getExposureMeteringMode() const = 0;
        virtual std::error_code setExposureMeteringMode(ExposureMeteringMode exposureMeteringMode) = 0;

        virtual float getAnalogGain() const = 0;
        virtual std::error_code setAnalogGain(float gain) = 0;

        virtual float getDigitalGain() const = 0;
        virtual std::error_code setDigitalGain(float gain) = 0;

        virtual DRCStrength getDRCStrength() const = 0;
        virtual std::error_code setDRCStrength(DRCStrength strength) = 0;

        virtual bool  getVideoStabilisation() const = 0;
        virtual std::error_code setVideoStabilisation(bool videoStabilisation) = 0;

        virtual FlickerAvoid getFlickerAvoid() const = 0;
        virtual std::error_code setFlickerAvoid(FlickerAvoid flickerAvoid) = 0;

        virtual std::error_code enableRecording() = 0;
        virtual bool isRecordingEnabled() const = 0;
        virtual std::error_code disableRecording() = 0;


        inline CameraEvents const& cameraEvents() const { return m_CameraEvents; }

    protected:
        inline void dispatchOnCameraConfigurationChanged()
        {
            m_CameraEvents.dispatch(&Events::onCameraConfigurationChanged);
        }

        inline void dispatchOnCameraVideoStarted()
        {
            m_CameraEvents.dispatch(&Events::onCameraVideoStarted);
        }

        inline void dispatchOnCameraVideoStopped()
        {
            m_CameraEvents.dispatch(&Events::onCameraVideoStopped);
        }

        inline void dispatchOnCameraTakingSnapshotsStarted()
        {
            m_CameraEvents.dispatch(&Events::onCameraTakingSnapshotsStarted);
        }

        inline void dispatchOnCameraTakingSnapshotsStopped()
        {
            m_CameraEvents.dispatch(&Events::onCameraTakingSnapshotsStopped);
        }

        inline void dispatchOnCameraRecordingStarted()
        {
            m_CameraEvents.dispatch(&Events::onCameraRecordingStarted);
        }

        inline void dispatchOnCameraRecordingStopped()
        {
            m_CameraEvents.dispatch(&Events::onCameraRecordingStopped);
        }

        inline void dispatchOnCameraVideoFrame(std::shared_ptr<PixelSampleBuffer> const &buffer)
        {
            m_CameraEvents.dispatch(&Events::onCameraVideoFrame, buffer);
        }

        inline void dispatchOnCameraSnapshotTaken(std::shared_ptr<PixelSampleBuffer> const &buffer)
        {
            m_CameraEvents.dispatch(&Events::onCameraSnapshotTaken, buffer);
        }

        inline void dispatchOnCameraRecordingBuffer(std::shared_ptr<SampleBuffer> const &buffer, std::uint32_t flags)
        {
            m_CameraEvents.dispatch(&Events::onCameraRecordingBuffer, buffer, flags);
        }

    protected:
        CameraEvents m_CameraEvents;
    };
    
    extern std::istream& operator>>(std::istream &s, Camera::AWBMode &v);
    extern std::ostream& operator<<(std::ostream &s, Camera::AWBMode v);
    
    extern std::istream& operator>>(std::istream &s, Camera::ExposureMode &v);
    extern std::ostream& operator<<(std::ostream &s, Camera::ExposureMode v);
    
    extern std::istream& operator>>(std::istream &s, Camera::ExposureMeteringMode &v);
    extern std::ostream& operator<<(std::ostream &s, Camera::ExposureMeteringMode v);
    
    extern std::istream& operator>>(std::istream &s, Camera::DRCStrength &v);
    extern std::ostream& operator<<(std::ostream &s, Camera::DRCStrength v);
    
    extern std::istream& operator>>(std::istream &s, Camera::FlickerAvoid &v);
    extern std::ostream& operator<<(std::ostream &s, Camera::FlickerAvoid v);
    
}

