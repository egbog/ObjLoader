#pragma once

template <typename DurationT = std::milli>
[[nodiscard]] std::chrono::duration<double, DurationT> Timer::Elapsed() const {
  return std::chrono::duration<double, DurationT>(Clock::now() - m_start);
}
