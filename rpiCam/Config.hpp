#pragma once

#include <cstdint>
#include <chrono>
#include <memory>
#include <list>
#include <mutex>
#include <thread>
#include <atomic>

#include <Eigen/Eigen>
#include <Eigen/StdVector>
#include <Eigen/Geometry>

namespace rpiCam
{
    typedef Eigen::Matrix<std::int32_t, 2, 1> Vec2i;
    typedef Eigen::Matrix<std::uint32_t, 2, 1> Vec2ui;
}

