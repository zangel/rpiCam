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
            virtual void onCameraVideoFrame(std::shared_ptr<PixelSampleBuffer> const &buffer) {}
            virtual void onCameraSnapshotTaken(std::shared_ptr<PixelSampleBuffer> const &buffer) {}

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

        inline void dispatchOnCameraVideoFrame(std::shared_ptr<PixelSampleBuffer> const &buffer)
        {
            m_CameraEvents.dispatch(&Events::onCameraVideoFrame, buffer);
        }

        inline void dispatchOnCameraSnapshotTaken(std::shared_ptr<PixelSampleBuffer> const &buffer)
        {
            m_CameraEvents.dispatch(&Events::onCameraSnapshotTaken, buffer);
        }

    protected:
        CameraEvents m_CameraEvents;
    };
}

