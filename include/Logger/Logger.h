#pragma once
#include <iosfwd>
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

  void LoggerWorkerThread();
  void ThreadSafeLogMessage(std::ostringstream t_entry);

private:
  void                    Shutdown();
  std::queue<LogEntry>    m_logQueue;
  std::mutex              m_waitLogMutex;
  std::condition_variable m_cv;
  bool                    m_shutdown = false;
};
