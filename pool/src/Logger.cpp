#include "pool/Logger/Logger.hpp"

#include "pool/ThreadPool.hpp"

#include <format>
#include <iostream>
#include <windows.h>

Logger::~Logger() {
  Shutdown();
}

/*!
 * @brief Creates a jthread in a private member of this instance
 */
void Logger::DispatchWorkerThread() {
  m_thread = std::jthread([this] { WorkerThread(); });
}

void Logger::LogInfo(const std::string& t_entry) {
  ThreadSafeLogMessage(t_entry, ol::Info);
}

void Logger::LogWarning(const std::string& t_entry) {
  ThreadSafeLogMessage(t_entry, ol::Warning);
}

void Logger::LogError(const std::string& t_entry) {
  ThreadSafeLogMessage(t_entry, ol::Error);
}

void Logger::LogSuccess(const std::string& t_entry) {
  ThreadSafeLogMessage(t_entry, ol::Success);
}

void Logger::FlushQueue() {
  HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);

  std::queue<ol::LogEntry> local;
  {
    std::lock_guard lock(m_waitLogMutex);
    std::swap(local, m_logQueue); // grab everything fast
  }

  while (!local.empty()) {
    auto [message, severity] = local.front();
    switch (severity) {
      case ol::Info: {
        SetConsoleTextAttribute(hConsole, 7);
        message = std::format("[ThreadPool] - Info: {}\n", message);
        break;
      }
      case ol::Warning: {
        SetConsoleTextAttribute(hConsole, 6);
        message = std::format("[ThreadPool] - Warning: {}\n", message);
        break;
      }
      case ol::Error: {
        SetConsoleTextAttribute(hConsole, 4);
        message = std::format("[ThreadPool] - Error: {}\n", message);
        break;
      }
      case ol::Success: {
        SetConsoleTextAttribute(hConsole, 2);
        message = std::format("[ThreadPool] - Success: {}\n", message);
        break;
      }
      case ol::None: {
        SetConsoleTextAttribute(hConsole, 7);
        break;
      }
    }
    std::cout << message;
    SetConsoleTextAttribute(hConsole, 7);
    local.pop();
  }
}

/*!
 * @brief Inserts a log message into a queue in a thread-safe manner
 * @param t_entry The message
 * @param t_severity Severity of the message
 */
void Logger::ThreadSafeLogMessage(std::string t_entry, ol::LogSeverity t_severity) {
  {
    std::lock_guard lock(m_waitLogMutex);
    m_logQueue.emplace(std::move(t_entry), t_severity);
  }
  m_cv.notify_one();
}

/*!
 * @brief A worker intended to be dispatched to a separate thread that will automatically detect messages that are inserted into the queue and will wait if the queue is empty.
 */
void Logger::WorkerThread() {
  LogInfo(std::format("Logger worker dispatched to thread: {}", ol::QueuedTask::ThreadIdString(std::this_thread::get_id())));
  while (true) {
    {
      std::unique_lock lock(m_waitLogMutex);

      // wait until there's something in the log queue
      m_cv.wait(lock, [this] { return !m_logQueue.empty() || m_shutdown; });

      if (m_shutdown && m_logQueue.empty()) {
        break;
      }
    } // release lock

    // print logs
    FlushQueue();
  }
}

/*!
 * @brief Signals all threads to wake up and finish printing any outstanding messages.
 */
void Logger::Shutdown() {
  {
    std::lock_guard lock(m_waitLogMutex);
    m_shutdown = true;
  }

  m_cv.notify_all(); // wake worker

  if (m_thread.joinable()) {
    m_thread.join(); // wait until worker finishes flushing
  }

  // final flush (main thread case)
  FlushQueue();
}
