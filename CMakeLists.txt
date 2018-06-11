cmake_minimum_required(VERSION 2.8)

project(rpiCam)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard")
#set(CMAKE_CXX_STANDARD 11)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")



find_package(VideoCore REQUIRED)
include_directories(${VIDEOCORE_INCLUDE_DIRS})

add_subdirectory(rpiCam)

add_executable(rpiCamExample rpiCamExample.cpp)
target_link_libraries(rpiCamExample rpiCam)

add_executable(estimateMaxVideoFrameRate estimateMaxVideoFrameRate.cpp)
target_link_libraries(estimateMaxVideoFrameRate rpiCam)

