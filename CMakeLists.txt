cmake_minimum_required(VERSION 2.8)

project(rpiCam)

add_definitions("-mcpu=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -marm")
if(CMAKE_BUILD_TYPE MATCHES "Debug")
   add_definitions(-rdynamic)
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -U_FORTIFY_SOURCE")
set(CMAKE_ASM_FLAGS "${CMAKE_ASM_FLAGS} -c")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")

include_directories(3rdparty/eigen-3.3.4)
include_directories(/opt/vc/include)
link_directories(/opt/vc/lib)

add_subdirectory(rpiCam)

add_executable(rpiCamExample main.cpp)
target_link_libraries(rpiCamExample rpiCam)

