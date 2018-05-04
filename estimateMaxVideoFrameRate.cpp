#include "rpiCam/Camera.hpp"
#include <iostream>

class CameraEvents
    : public Device::Events
    , public Camera::Events
{
public:
    using Clock = std::chrono::high_resolution_clock;

    void onCameraVideoStarted() override
    {
        m_LastFrameTime = Clock::time_point::max();
        m_NumFrames = 0;
        m_FramesDuration = Clock::duration(0);
    }

    void onCameraVideoStopped() override
    {
    }

    void onCameraVideoFrame(std::shared_ptr<PixelSampleBuffer> const &buffer) override
    {
        auto now = Clock::now();
        if (m_LastFrameTime != Clock::time_point::max())
        {
            m_FramesDuration += now - m_LastFrameTime;
            m_NumFrames++;
        }
        m_LastFrameTime = now;
    }

    double estimatedVideoFrameRate() const
    {
        return 1.0e+6 * double(m_NumFrames) / std::chrono::duration_cast<std::chrono::microseconds>(m_FramesDuration).count();
    }

private:
    Clock::time_point m_LastFrameTime;
    std::size_t m_NumFrames;
    Clock::duration m_FramesDuration;

};

int main(int argc, char *argv[])
{
    std::vector<Vec2ui> videoSizes =
    {
        {640, 480},
        {1280, 720},
        {1296, 730},
        {1296, 972},
        {1640, 922},
        {1640, 1232},
        {1920, 1080},
        {2592, 1944},
        {3280, 2464}
    };

    CameraEvents cameraEvents;

    auto cameras = Device::list<Camera>();

    for(auto cam : cameras)
    {
        cam->deviceEvents() += &cameraEvents;
        cam->cameraEvents() += &cameraEvents;

        if(!cam->open())
        {
            std::cout << "camera: " << cam->name() << std::endl;
            cam->setVideoFrameRate(Rational(200, 1));
            for(auto vs : videoSizes)
            {
                std::cout << "	estVideoFrameRate(" << vs(0) << "x" << vs(1) <<"): ";

                cam->setVideoSize(vs);
                if(!cam->startVideo())
                {
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    std::cout << cameraEvents.estimatedVideoFrameRate() << std::endl;
                    cam->stopVideo();
                }
                else
                {
                    std::cout << "NA" << std::endl;
                }
            }
            cam->close();
        }
    }
    return 0;
}
