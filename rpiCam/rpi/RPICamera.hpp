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

        std::error_code takeSnapshot() override;

    private:
        std::error_code applyVideoFormat(ePixelFormat fmt);
        std::error_code applyVideoSize(Vec2ui const &sz);
        std::error_code applyVideoFrameRate(Rational const &rate);

        std::error_code applySnapshotFormat(ePixelFormat fmt);
        std::error_code applySnapshotSize(Vec2ui const &sz);

        std::error_code initializeCameraControlPort();
        std::error_code initializeVideoPort();
        std::error_code initializeSnapshotPort();

        std::error_code enableVideoPort();
        std::error_code enableSnapshotPort();

        void disableVideoPort();
        void disableSnapshotPort();

        static void _mmalCameraControlCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
        void mmalCameraControlCallback(MMAL_BUFFER_HEADER_T *buffer);

        static void _mmalCameraVideoBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
        void mmalCameraVideoBufferCallback(MMAL_BUFFER_HEADER_T *buffer);

        static void _mmalCameraSnapshotBufferCallback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer);
        void mmalCameraSnapshotBufferCallback(MMAL_BUFFER_HEADER_T *buffer);

    private:
        std::shared_ptr<MMAL_COMPONENT_T> m_Camera;
        MMAL_PORT_T *m_VideoPort;
        MMAL_PORT_T *m_SnapshotPort;
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
        bool m_bConfiguring;
        bool m_bConfigurationChanged;
        bool m_bTakingSnapshot;

        MMAL_POOL_T *m_VideoBufferPool;
        MMAL_POOL_T *m_SnapshotBufferPool;
    };
}
