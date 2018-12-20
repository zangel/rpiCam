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
    using TimeClock = std::chrono::steady_clock;
    using TimePoint = TimeClock::time_point;
    using Duration = TimeClock::duration;


    using Vec2i = Eigen::Matrix<std::int32_t, 2, 1>;
    using Vec2ui = Eigen::Matrix<std::uint32_t, 2, 1>;
}

