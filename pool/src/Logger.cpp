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

void Logger::FlushQueue() {
  const HANDLE console = GetStdHandle(STD_OUTPUT_HANDLE);

  std::queue<LogEntry> local;
  {
    std::lock_guard lock(m_waitLogMutex);
    std::swap(local, m_logQueue); // grab everything fast
  }

  while (!local.empty()) {
    auto [message, severity] = local.front();

    ConsoleColor color(console, GetSeverityColor(severity));
    std::cout << message << '\n';

    local.pop();
  }
}

/*!
 * @brief Signals all threads to wake up and finish printing any outstanding messages.
 */
void Logger::Shutdown() {
  Log<Debug>(std::format("Logger worker closed on thread: {}", obj::QueuedTask::ThreadIdString(m_workerThreadId)));
  {
    std::lock_guard lock(m_waitLogMutex);
    m_shutdown = true;
  }

  m_cv.notify_all(); // wake worker

  if (m_thread.joinable()) {
    m_thread.join(); // wait until worker finishes flushing
  }
}

bool Logger::IsLogLevelEnabled(const LogSeverity t_logLevel) const {
  return t_logLevel >= currentLogLevel;
}

constexpr WORD Logger::GetSeverityColor(const LogSeverity t_logLevel) {
  switch (t_logLevel) {
    case Debug: return 8;
    case Info: return 7;
    case Warning: return 6;
    case Error: return 4;
    case Success: return 2;
    default: return 7;
  }
}

/*!
 * @brief Inserts a log message into a queue in a thread-safe manner
 * @param t_entry The log entry
 */
void Logger::ThreadSafeLogMessage(LogEntry t_entry) {
  // if the severity is lower than our current set log level, skip it
  if (!IsLogLevelEnabled(t_entry.severity)) {
    return;
  }

  {
    std::lock_guard lock(m_waitLogMutex);
    m_logQueue.emplace(std::move(t_entry));
  }

  // notify thread
  m_cv.notify_one();
}

/*!
 * @brief A worker intended to be dispatched to a separate thread that will automatically detect messages that are inserted into the queue and will wait if the queue is empty.
 */
void Logger::WorkerThread() {
  m_workerThreadId = std::this_thread::get_id();
  Log<Debug>(std::format("Logger worker dispatched to thread: {}", obj::QueuedTask::ThreadIdString(m_workerThreadId)));
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
