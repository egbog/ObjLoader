#pragma once
#include <mutex>
#include <queue>

#include <Types/InternalTypes.h>

class Logger
{
public:
  Logger() = default;
  ~Logger();
  Logger& operator=(Logger& t_other)  = delete;
  Logger& operator=(Logger&& t_other) = delete;
  Logger(Logger& t_other)             = delete;
  Logger(Logger&& t_other)            = delete;

  void DispatchWorkerThread();
  void ThreadSafeLogMessage(std::string t_entry);

private:
  void WorkerThread();
  void Shutdown();

  std::jthread            m_thread;
  std::queue<LogEntry>    m_logQueue;
  std::queue<LogEntry>    m_resultQueue;
  std::mutex              m_waitLogMutex;
  std::condition_variable m_cv;
  bool                    m_shutdown = false;
};
