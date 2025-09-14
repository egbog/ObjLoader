#include <Logger/Logger.h>

#include <iostream>

void Logger::LoggerWorkerThread() {
  while (true) {
    std::unique_lock lock(m_waitLogMutex);
    if (!m_logQueue.empty()) {
      while (!m_logQueue.empty()) {
        auto& [message] = m_logQueue.front();
        std::cout << message.str();
        m_logQueue.pop();
      }

      lock.unlock();
    }
    else {
      lock.unlock();
      std::this_thread::sleep_for(std::chrono::nanoseconds(10));
    }
  }
}

void Logger::ThreadSafeLogMessage(std::ostringstream& t_entry) {
  std::lock_guard lock(m_waitLogMutex);
  m_logQueue.emplace(std::move(t_entry));
}
