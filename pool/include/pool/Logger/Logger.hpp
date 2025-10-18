#pragma once

#include <mutex>
#include <queue>

namespace ol
{
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
    std::string message;
    LogSeverity severity;
  };
}

class Logger
{
public:
  //-------------------------------------------------------------------------------------------------------------------
  // Constructors/operators
  explicit Logger(std::string t_source);
  ~Logger();
  Logger& operator=(Logger& t_other)  = delete;
  Logger& operator=(Logger&& t_other) = delete;
  Logger(Logger& t_other)             = delete;
  Logger(Logger&& t_other)            = delete;
  //-------------------------------------------------------------------------------------------------------------------

  void SetSource(const std::string& t_source);
  void DispatchWorkerThread();
  void LogDebug(const std::string& t_entry);
  void LogInfo(const std::string& t_entry);
  void LogWarning(const std::string& t_entry);
  void LogError(const std::string& t_entry);
  void LogSuccess(const std::string& t_entry);
  void FlushQueue();

private:
  void ThreadSafeLogMessage(std::string t_entry, ol::LogSeverity t_severity);
  void WorkerThread();
  void Shutdown();

  std::jthread             m_thread;       // Worker thread
  std::queue<ol::LogEntry> m_logQueue;     // The message queue
  std::mutex               m_waitLogMutex; // Mutex for inserting and popping into the queue
  std::condition_variable  m_cv;           // Cv to wait thread
  bool                     m_shutdown = false;
  std::string              m_source;
};
