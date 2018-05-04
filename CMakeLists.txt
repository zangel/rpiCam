cmake_minimum_required(VERSION 2.8)

project(rpiCam)

include_directories(3rdparty/eigen-3.3.4)
include_directories(/opt/vc/include)
link_directories(/opt/vc/lib)

add_subdirectory(rpiCam)

add_executable(rpiCamExample main.cpp)
target_link_libraries(rpiCamExample rpiCam)

