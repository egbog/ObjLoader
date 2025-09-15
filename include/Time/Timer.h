#pragma once
#include <chrono>

class Timer {
public:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = Clock::time_point;

    Timer() { Reset(); }

    // Reset the timer to "now"
    void Reset() {
        m_start = Clock::now();
    }

    // Generic elapsed time in any chrono duration type
    template <typename DurationT = std::milli>
    std::chrono::duration<double, DurationT> Elapsed() const {
        return std::chrono::duration<double, DurationT>(Clock::now() - m_start);
    }

private:
    TimePoint m_start;
};