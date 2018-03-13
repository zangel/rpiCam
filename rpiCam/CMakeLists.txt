
set(rpiCam_headers
    Config.hpp
    Rational.hpp
    EventsDispatcher.hpp
    PixelFormat.hpp
    Buffer.hpp
    PixelBuffer.hpp
    SampleBuffer.hpp
    PixelSampleBuffer.hpp
    Device.hpp
    Camera.hpp
    rpi/RPICamera.hpp
	rpi/RPIPixelSampleBuffer.hpp
)

set(rpiCam_sources
    Config.cpp
    Rational.cpp
    Buffer.cpp
    PixelBuffer.cpp
    SampleBuffer.cpp
    PixelSampleBuffer.cpp
    Device.cpp
    Camera.cpp
    rpi/RPICamera.cpp
	rpi/RPIPixelSampleBuffer.cpp
)

add_library(rpiCam STATIC
    ${rpiCam_headers}
    ${rpiCam_sources}
)

target_link_libraries(rpiCam mmal_core mmal_util mmal_vc_client)
