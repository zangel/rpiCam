#include <iostream>
#include "rpiCam/Camera.hpp"

using namespace std;

int main(int argc, char *argv[])
{
    auto cameras = Device::list<Camera>();

    for(auto cam : cameras)
    {
        if(!cam->open())
        {
            cam->close();
        }
    }
    return 0;
}
