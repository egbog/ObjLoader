#include <iostream>

#include <Logger/Logger.h>

Logger::~Logger() {
  Shutdown();
}

void Logger::LoggerWorkerThread() {
  std::cout << "Logger worker thread dispatched to thread: " << std::this_thread::get_id() << '\n';
  while (true) {
    std::unique_lock lock(m_waitLogMutex);

    // wait until there's something in the log queue
    m_cv.wait(lock, [this] { return !m_logQueue.empty() || m_shutdown; });

    if (m_shutdown && m_logQueue.empty()) {
      break;
    }

    while (!m_logQueue.empty()) {
      auto [message] = std::move(m_logQueue.front());
      m_logQueue.pop();

      lock.unlock();
      std::cout << message.str();
      lock.lock();
    }
  }
}

void Logger::ThreadSafeLogMessage(std::ostringstream t_entry) {
  std::lock_guard lock(m_waitLogMutex);
  m_logQueue.emplace(std::move(t_entry));
  m_cv.notify_one();
}

void Logger::Shutdown() {
  m_shutdown = true;
}
