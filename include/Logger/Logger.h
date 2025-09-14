#pragma once
#include <iosfwd>
#include <mutex>
#include <queue>

#include <Types/InternalTypes.h>

class Logger
{
public:
  Logger()                            = default;
  ~Logger()                           = default;
  Logger& operator=(Logger& t_other)  = delete;
  Logger& operator=(Logger&& t_other) = delete;
  Logger(Logger& t_other)             = delete;
  Logger(Logger&& t_other)            = delete;

  [[noreturn]] void LoggerWorkerThread();
  void              ThreadSafeLogMessage(std::ostringstream& t_entry);

private:
  std::queue<LogEntry> m_logQueue;
  std::mutex           m_waitLogMutex;
};
