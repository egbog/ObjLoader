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
    switch (severity) {
      case Debug: {
        SetConsoleTextAttribute(console, 8);
        break;
      }
      case Info: {
        SetConsoleTextAttribute(console, 7);
        break;
      }
      case Warning: {
        SetConsoleTextAttribute(console, 6);
        break;
      }
      case Error: {
        SetConsoleTextAttribute(console, 4);
        break;
      }
      case Success: {
        SetConsoleTextAttribute(console, 2);
        break;
      }
      case None: {
        SetConsoleTextAttribute(console, 7);
        break;
      }
    }
    std::cout << message << '\n';
    SetConsoleTextAttribute(console, 7);
    local.pop();
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

  Log<Debug>(std::format("Logger worker closed on thread: {}", obj::QueuedTask::ThreadIdString(m_workerThreadId)));

  m_cv.notify_all(); // wake worker

  if (m_thread.joinable()) {
    m_thread.join(); // wait until worker finishes flushing
  }
}

/*!
 * @brief Inserts a log message into a queue in a thread-safe manner
 * @param t_entry The log entry
 */
void Logger::ThreadSafeLogMessage(LogEntry t_entry) {
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
