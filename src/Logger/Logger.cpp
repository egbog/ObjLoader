#include <iostream>

#include <Logger/Logger.h>

Logger::~Logger() {
  Shutdown();
}

void Logger::DispatchWorkerThread() {
  // Dispatch logger worker
  m_thread = std::jthread([this] { WorkerThread(); });
}

void Logger::ThreadSafeLogMessage(std::string t_entry) {
  {
    std::lock_guard lock(m_waitLogMutex);
    m_logQueue.emplace(std::move(t_entry));
  }
  m_cv.notify_one();
}

void Logger::WorkerThread() {
  std::cout << "Logger worker thread dispatched to thread: " << std::this_thread::get_id() << '\n';
  std::string message;
  while (true) {
    {
      std::unique_lock lock(m_waitLogMutex);

      // wait until there's something in the log queue
      m_cv.wait(lock, [this] { return !m_logQueue.empty() || m_shutdown; });

      if (m_shutdown && m_logQueue.empty()) {
        break;
      }

      message = std::move(m_logQueue.front()).message;
      m_logQueue.pop();
    } // release lock

    std::cout << message;
  }
}

void Logger::Shutdown() {
  {
    std::lock_guard<std::mutex> lock(m_waitLogMutex);
    m_shutdown = true;
  }
  m_cv.notify_all(); // wake worker
  if (m_thread.joinable()) {
        m_thread.join(); // wait until worker finishes flushing
    }
}
