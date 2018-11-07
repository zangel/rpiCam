#include "rpiCam/Camera.hpp"
#include "rpiCam/Logging.hpp"
#include <iostream>
#include <fstream>

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
        //std::cout << "onCameraVideoFrame():" << std::endl;
        //dumpBufferInfo(buffer);
        //saveBuffer(buffer, "video.ppm");
    }

    void saveBuffer(std::shared_ptr<PixelSampleBuffer> const &buffer, std::string name)
    {
        if(!buffer->lock())
        {
            std::ofstream ppm(name, std::ios::binary);
            ppm << "P6" << std::endl;
            ppm << buffer->planeSize()[0] << " " << buffer->planeSize()[1] << std::endl;
            ppm << "255" << std::endl;

            char *data = reinterpret_cast<char*>(buffer->planeData());
            std::size_t rowBytes = buffer->planeRowBytes();

            for (int y = 0; y < buffer->planeSize()[1]; ++y)
            {
                ppm.write(data, 3 * buffer->planeSize()[0]);
                data += rowBytes;
            }
            buffer->unlock();
        }
    }

    void onCameraSnapshotTaken(std::shared_ptr<PixelSampleBuffer> const &buffer) override
    {
        std::cout << "onCameraSnapshotTaken():" << std::endl;
        //dumpBufferInfo(buffer);
        //saveBuffer(buffer, "snapshot.ppm");
    }

    void dumpBufferInfo(std::shared_ptr<PixelSampleBuffer> const &buffer)
    {
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
            cam->setVideoSize(Vec2ui(1920, 1080));
            cam->setSnapshotSize(Vec2ui(2592, 1944));
            cam->setVideoFrameRate(Rational(30, 1));
            if(!cam->startVideo())
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                cam->takeSnapshot();
                std::this_thread::sleep_for(std::chrono::seconds(5));
                cam->stopVideo();
            }
            cam->close();
        }
    }
    return 0;
}
