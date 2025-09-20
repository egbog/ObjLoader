#pragma once
#include <mutex>
#include <queue>

namespace ol
{
  struct LogEntry
  {
    std::string message;
  };
}

class Logger
{
public:
  //-------------------------------------------------------------------------------------------------------------------
  // Constructors/operators
  Logger() = default;
  ~Logger();
  Logger& operator=(Logger& t_other)  = delete;
  Logger& operator=(Logger&& t_other) = delete;
  Logger(Logger& t_other)             = delete;
  Logger(Logger&& t_other)            = delete;
  //-------------------------------------------------------------------------------------------------------------------

  void DispatchWorkerThread();
  void ThreadSafeLogMessage(std::string t_entry);
  void FlushQueue();

private:
  void WorkerThread();
  void Shutdown();

  std::jthread             m_thread;       // Worker thread
  std::queue<ol::LogEntry> m_logQueue;     // The message queue
  std::mutex               m_waitLogMutex; // Mutex for inserting and popping into the queue
  std::condition_variable  m_cv;           // Cv to wait thread
  bool                     m_shutdown = false;
};
