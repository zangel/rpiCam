#pragma once

#include "Config.hpp"
#include "Device.hpp"
#include "PixelFormat.hpp"
#include "PixelSampleBuffer.hpp"
#include "EventsDispatcher.hpp"
#include "Rational.hpp"

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
        virtual void onCameraVideoFrame(std::shared_ptr<PixelSampleBuffer> const &buffer) {}

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

    inline void dispatchOnCameraVideoFrame(std::shared_ptr<PixelSampleBuffer> const &buffer)
    {
        m_CameraEvents.dispatch(&Events::onCameraVideoFrame, buffer);
    }

protected:
    CameraEvents m_CameraEvents;
};

