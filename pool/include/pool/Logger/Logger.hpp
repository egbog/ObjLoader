#pragma once

#define NOMINMAX
#include <iostream>
#include <mutex>
#include <queue>
#include <windows.h>

class Logger
{
public:
  enum LogSeverity : uint8_t
  {
    Debug,
    Info,
    Warning,
    Error,
    Success,
    None
  };

  struct LogEntry
  {
    LogEntry(std::string t_message, const LogSeverity t_severity) : message(std::move(t_message)), severity(t_severity) {}
    std::string message;
    LogSeverity severity;
  };

  struct ConsoleColor
  {
    HANDLE h;
    WORD   original;

    ConsoleColor(const HANDLE t_h, const WORD t_c) : h(t_h) {
      CONSOLE_SCREEN_BUFFER_INFO info;
      GetConsoleScreenBufferInfo(t_h, &info);
      original = info.wAttributes;
      SetConsoleTextAttribute(t_h, t_c);
    }

    ~ConsoleColor() { SetConsoleTextAttribute(h, original); }
    ConsoleColor& operator=(ConsoleColor& t_other)  = delete;
    ConsoleColor& operator=(ConsoleColor&& t_other) = delete;
    ConsoleColor(ConsoleColor& t_other)             = delete;
    ConsoleColor(ConsoleColor&& t_other)            = delete;
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

  LogSeverity currentLogLevel = Debug; // The severity level of log messages to print
private:
  [[nodiscard]] bool    IsLogLevelEnabled(LogSeverity t_logLevel) const;
  constexpr static WORD GetSeverityColor(LogSeverity t_logLevel);
  void                  ThreadSafeLogMessage(LogEntry t_entry);
  void                  WorkerThread();

  std::jthread            m_thread;         // Worker thread
  std::queue<LogEntry>    m_logQueue;       // The message queue
  std::mutex              m_waitLogMutex;   // Mutex for inserting and popping into the queue
  std::condition_variable m_cv;             // Cv to wait thread
  std::thread::id         m_workerThreadId; // The thread id of the dispatched worker
  bool                    m_shutdown = false;
};
