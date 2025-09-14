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

    std::string message;

    // Scope for the lock just to pop from the queue
    {
      message = std::move(m_logQueue.front()).message;
      m_logQueue.pop();
    }

    lock.unlock(); // unlock before expensive I/O
    std::cout << message;
  }
}

void Logger::ThreadSafeLogMessage(const std::string& t_entry) {
  std::lock_guard lock(m_waitLogMutex);
  m_logQueue.emplace(t_entry);
  m_cv.notify_one();
}

void Logger::Shutdown() {
  m_shutdown = true;
}
