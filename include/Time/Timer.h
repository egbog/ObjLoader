#pragma once
#include <chrono>
#include <mutex>

class Timer
{
public:
  Timer() {
    m_start = std::chrono::high_resolution_clock::now();
  }

  std::chrono::duration<double, std::milli> GetTime() const {
    elapsed = std::chrono::high_resolution_clock::now() - m_start;
    return elapsed;
  }

  mutable std::chrono::duration<double, std::milli> elapsed;

private:
  std::chrono::time_point<std::chrono::steady_clock> m_start;
};
