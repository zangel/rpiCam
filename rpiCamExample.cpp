#include "rpiCam/Camera.hpp"
#include "rpiCam/Logging.hpp"
#include <iostream>
#include <fstream>
#include <condition_variable>
#include <mutex>

#define CLAMP_0_255(X) ((X) > 255 ? 255 : ((X) < 0 ? 0 : (X)))

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
        saveBuffer(buffer, "video.ppm");
    }

    void convertYuv420ToRgb(std::shared_ptr<PixelSampleBuffer> const &buffer, uint8_t* destRgb)
    {
        int width = buffer->planeSize()[0];
        int height = buffer->planeSize()[1];
        int halfWidth = width/2;
        int halfHeight = height/2;

        uint8_t const *srcY = reinterpret_cast<uint8_t const*>(buffer->planeData(0));
        uint8_t const *srcU = reinterpret_cast<uint8_t const*>(buffer->planeData(1));
        uint8_t const *srcV = reinterpret_cast<uint8_t const*>(buffer->planeData(2));

        //int offsetU = width*height;
        //int offsetV = offsetU + halfWidth*halfHeight;


        for (int y = 0; y < halfHeight; y++)
        {
            for (int x = 0; x < halfWidth; x++)
            {
                int offset = (y * halfWidth) + x;
                int U = srcU[offset] - 128;
                int V = srcV[offset] - 128;

                int a1 = 1634 * V;
                int a2 = 832 * V;
                int a3 = 400 * U;
                int a4 = 2066 * U;

                offset = ((y*2 + 0) * width + x*2);
                int offsetRGB = offset*3;

                // Y1
                int Y = srcY[offset];
                int a0 = 1192 * (Y - 16);
                int r = (a0 + a1) >> 10; // R
                int g = (a0 - a2 - a3) >> 10; // G
                int b = (a0 + a4) >> 10; // B
                destRgb[offsetRGB    ] = (uint8_t)CLAMP_0_255(r);
                destRgb[offsetRGB + 1] = (uint8_t)CLAMP_0_255(g);
                destRgb[offsetRGB + 2] = (uint8_t)CLAMP_0_255(b);

                // Y2
                Y = srcY[offset + 1];
                a0 = 1192 * (Y - 16);
                r = (a0 + a1) >> 10; // R
                g = (a0 - a2 - a3) >> 10; // G
                b = (a0 + a4) >> 10; // B
                destRgb[offsetRGB + 3] = (uint8_t)CLAMP_0_255(r);
                destRgb[offsetRGB + 4] = (uint8_t)CLAMP_0_255(g);
                destRgb[offsetRGB + 5] = (uint8_t)CLAMP_0_255(b);

                offset = ((y*2 + 1) * width + x*2);
                offsetRGB = offset + offset + offset;

                // Y3
                Y = srcY[offset];
                a0 = 1192 * (Y - 16);
                r = (a0 + a1) >> 10; // R
                g = (a0 - a2 - a3) >> 10; // G
                b = (a0 + a4) >> 10; // B
                destRgb[offsetRGB    ] = (uint8_t)CLAMP_0_255(r);
                destRgb[offsetRGB + 1] = (uint8_t)CLAMP_0_255(g);
                destRgb[offsetRGB + 2] = (uint8_t)CLAMP_0_255(b);

                // Y4
                Y = srcY[offset + 1];
                a0 = 1192 * (Y - 16);
                r = (a0 + a1) >> 10; // R
                g = (a0 - a2 - a3) >> 10; // G
                b = (a0 + a4) >> 10; // B
                destRgb[offsetRGB + 3] = (uint8_t)CLAMP_0_255(r);
                destRgb[offsetRGB + 4] = (uint8_t)CLAMP_0_255(g);
                destRgb[offsetRGB + 5] = (uint8_t)CLAMP_0_255(b);
            }
        }
    }

    void saveBuffer(std::shared_ptr<PixelSampleBuffer> const &buffer, std::string name)
    {
        if(!buffer->lock())
        {
            char *data;
            std::size_t rowBytes;

            if(buffer->format() == kPixelFormatYUV420)
            {
                data = reinterpret_cast<char *>(std::malloc(buffer->planeSize()[0] * buffer->planeSize()[1] * 3));
                rowBytes = buffer->planeSize()[0] * 3;

                convertYuv420ToRgb(buffer, reinterpret_cast<uint8_t*>(data));
            }
            else
            {
                data = reinterpret_cast<char*>(buffer->planeData());
                rowBytes = buffer->planeRowBytes();
            }
            std::ofstream ppm(name, std::ios::binary);
            ppm << "P6" << std::endl;
            ppm << buffer->planeSize()[0] << " " << buffer->planeSize()[1] << std::endl;
            ppm << "255" << std::endl;

            char *tmp = data;
            for (int y = 0; y < buffer->planeSize()[1]; ++y)
            {
                ppm.write(tmp, 3 * buffer->planeSize()[0]);
                tmp += rowBytes;
            }

            if(buffer->format() == kPixelFormatYUV420)
            {
                std::free(data);
            }
            buffer->unlock();
        }
    }

    void onCameraSnapshotTaken(std::shared_ptr<PixelSampleBuffer> const &buffer) override
    {
        std::cout << "onCameraSnapshotTaken():" << std::endl;
        dumpBufferInfo(buffer);
        saveBuffer(buffer, "snapshot.ppm");
        std::unique_lock<std::mutex> lk(mutex);
        cv.notify_one();
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

    void waitForSnapshot()
    {
        std::unique_lock<std::mutex> lk(mutex);
        cv.wait(lk);
    }

    std::mutex mutex;
    std::condition_variable cv;
};

int main(int argc, char *argv[])
{
    setLogLevel(LOG_DEBUG);
    CameraEvents cameraEvents;

    auto cameras = Device::list<Camera>();

    for(auto cam : cameras)
    {
        cam->deviceEvents() += &cameraEvents;
        cam->cameraEvents() += &cameraEvents;

        if(!cam->open())
        {
            cam->setVideoSize(Vec2ui(2592, 1944));
            cam->setVideoFrameRate(Rational(10, 1));
            if(!cam->startVideo())
            {
                std::this_thread::sleep_for(std::chrono::seconds(4));
                cam->stopVideo();
            }

            cam->setSnapshotSize(Vec2ui(2592, 1944));
            if(!cam->startTakingSnapshots())
            {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                cam->takeSnapshot();
                cameraEvents.waitForSnapshot();
                cam->stopTakingSnapshots();
            }
            cam->close();
        }
    }
    return 0;
}
