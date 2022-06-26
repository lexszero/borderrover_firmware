#pragma once

#include <chrono>

using namespace std::literals;
using std::chrono::milliseconds;
using Clock = std::chrono::system_clock;
using duration = milliseconds;
using time_point = std::chrono::time_point<Clock, milliseconds>;

static inline time_point time_now() {
	return std::chrono::time_point_cast<duration>(Clock::now());
}
