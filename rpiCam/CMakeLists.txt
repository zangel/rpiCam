set(rpiCam_VERSION 0.1.0)

find_package (Eigen3 3.3 REQUIRED NO_MODULE)

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
)

set(rpiCam_headers_private
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
)

set(rpiCam_sources_private
    rpi/RPICamera.cpp
    rpi/RPIPixelSampleBuffer.cpp
)

add_library(rpiCam STATIC
    ${rpiCam_headers} ${rpiCam_headers_private}
    ${rpiCam_sources} ${rpiCam_sources_private}
)

target_link_libraries(rpiCam ${VIDEOCORE_LIBRARIES} Eigen3::Eigen)

install(
    TARGETS rpiCam EXPORT "rpiCamTargets"
    LIBRARY DESTINATION lib${LIB_SUFFIX}
    ARCHIVE DESTINATION lib${LIB_SUFFIX}
    RUNTIME DESTINATION bin
    INCLUDES DESTINATION include
)

install(
    FILES ${rpiCam_headers}
    DESTINATION include/rpiCam
    COMPONENT Devel
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_BINARY_DIR}/${PROJECT_NAME}/rpiCamConfigVersion.cmake"
    VERSION ${rpiCam_VERSION}
    COMPATIBILITY AnyNewerVersion
)

export(EXPORT "rpiCamTargets"
    FILE "${CMAKE_BINARY_DIR}/${PROJECT_NAME}/rpiCamTargets.cmake"
    NAMESPACE "rpiCam::"
)

configure_file("cmake/rpiCamConfig.cmake"
  "${CMAKE_BINARY_DIR}/${PROJECT_NAME}/rpiCamConfig.cmake"
  @ONLY
)

set(ConfigPackageLocation "lib/cmake/${PROJECT_NAME}")

install(
    EXPORT "rpiCamTargets"
    FILE "rpiCamTargets.cmake"
    NAMESPACE "rpiCam::"
    DESTINATION "lib${LIB_SUFFIX}/cmake/${PROJECT_NAME}"
)

install(
    FILES
        "${CMAKE_BINARY_DIR}/${PROJECT_NAME}/rpiCamConfig.cmake"
        "${CMAKE_BINARY_DIR}/${PROJECT_NAME}/rpiCamConfigVersion.cmake"
    DESTINATION "lib${LIB_SUFFIX}/cmake/${PROJECT_NAME}"
    COMPONENT Devel
)
