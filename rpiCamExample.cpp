#include "rpiCam/Camera.hpp"
#include "rpiCam/Logging.hpp"
#include <iostream>

using namespace rpiCam;

class CameraEvents
    : public Device::Events
    , public Camera::Events
{
public:
    void onDeviceOpened() override
    {
        std::cout << "onDeviceOpened()" << std::endl;
    }

    void onDeviceClosed()
    {
        std::cout << "onDeviceClosed()" << std::endl;
    }

    void onCameraConfigurationChanged() override
    {
        std::cout << "onCameraConfigurationChanged()" << std::endl;
    }

    void onCameraVideoStarted() override
    {
        std::cout << "onCameraVideoStarted()" << std::endl;
    }

    void onCameraVideoStopped() override
    {
        std::cout << "onCameraVideoStopped()" << std::endl;
    }

    void onCameraVideoFrame(std::shared_ptr<PixelSampleBuffer> const &buffer) override
    {
        std::cout << "onCameraVideoFrame():" << std::endl;

        if(!buffer->lock())
        {
            std::cout
                << "\t" << "format: " << buffer->format() << std::endl
                << "\t" << "size: " << buffer->planeSize(0)[0] << "x" << buffer->planeSize(0)[1] << std::endl
                << "\t" << "planeCount: " << buffer->planeCount() << std::endl;

            for (std::uint32_t pi = 0; pi < buffer->planeCount(); ++pi)
            {
                std::cout
                    << "\t" << "plane[" << pi << "]:" << std::endl
                    << "\t\t" << "size: " << buffer->planeSize(pi)[0] << "x" << buffer->planeSize(pi)[1] << std::endl
                    << "\t\t" << "data: " << buffer->planeData(pi) << std::endl
                    << "\t\t" << "stride: " << buffer->planeRowBytes(pi) << std::endl;
            }

            buffer->unlock();
            std::cout.flush();
        }
    }
};

int main(int argc, char *argv[])
{
    setLogLevel(LOG_SILENT);
    CameraEvents cameraEvents;

    auto cameras = Device::list<Camera>();

    for(auto cam : cameras)
    {
        cam->deviceEvents() += &cameraEvents;
        cam->cameraEvents() += &cameraEvents;

        if(!cam->open())
        {
            if(!cam->startVideo())
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                cam->stopVideo();
            }
            cam->close();
        }
    }
    return 0;
}
