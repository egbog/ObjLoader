#include <iostream>

#include <Logger/Logger.h>

Logger::~Logger() {
  Shutdown();
}

/*!
 * @brief Creates a jthread in a private member of this instance
 */
void Logger::DispatchWorkerThread() {
  m_thread = std::jthread([this] { WorkerThread(); });
}

/*!
 * @brief Inserts a log message into a queue in a thread-safe manner
 * @param t_entry The message
 */
void Logger::ThreadSafeLogMessage(std::string t_entry) {
  {
    std::lock_guard lock(m_waitLogMutex);
    m_logQueue.emplace(std::move(t_entry));
  }
  m_cv.notify_one();
}

/*!
 * @brief A worker intended to be dispatched to a separate thread that will automatically detect messages that are inserted into the queue and will wait if the queue is empty.
 */
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

/*!
 * @brief Signals all threads to wake up and finish printing any outstanding messages. Joins worker thread
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
}
