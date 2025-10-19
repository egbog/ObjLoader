#pragma once

#include <iostream>
#include <mutex>
#include <queue>

class Logger
{
public:
  enum LogSeverity : uint8_t
  {
    None,
    Debug,
    Info,
    Warning,
    Error,
    Success
  };

  struct LogEntry
  {
    LogEntry(std::string t_message, const LogSeverity t_severity) : message(std::move(t_message)), severity(t_severity) {}
    std::string message;
    LogSeverity severity;
  };

  //-------------------------------------------------------------------------------------------------------------------
  // Constructors/operators
  Logger() = default;
  ~Logger();
  Logger& operator=(Logger& t_other)  = delete;
  Logger& operator=(Logger&& t_other) = delete;
  Logger(Logger& t_other)             = delete;
  Logger(Logger&& t_other)            = delete;
  //-------------------------------------------------------------------------------------------------------------------

  static Logger& Instance() {
    static Logger instance;
    return instance;
  }

  void DispatchWorkerThread();

  template <LogSeverity Severity>
  void Log(const std::string& t_entry) {
    ThreadSafeLogMessage(LogEntry(t_entry, Severity));
  }

  void FlushQueue();
  void Shutdown();

private:
  void ThreadSafeLogMessage(LogEntry t_entry);
  void WorkerThread();

  std::jthread            m_thread;       // Worker thread
  std::queue<LogEntry>    m_logQueue;     // The message queue
  std::mutex              m_waitLogMutex; // Mutex for inserting and popping into the queue
  std::condition_variable m_cv;           // Cv to wait thread
  bool                    m_shutdown = false;
};
