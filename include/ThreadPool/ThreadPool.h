#pragma once
#include "Time/Timer.h"

#include <future>

class Logger;

namespace ol
{
  struct LoaderState;
  struct Model;

  struct QueuedTask
  {
    QueuedTask() = delete;

    QueuedTask(std::packaged_task<void()> t_task, const unsigned int t_taskNumber) : task(std::move(t_task)),
      timer(Timer()),
      taskNumber(t_taskNumber) {}

    [[nodiscard]] std::string ThreadIdString() const {
      std::ostringstream s;
      s << threadId;
      return s.str();
    }

    std::packaged_task<void()> task;
    Timer                      timer;
    unsigned int               taskNumber;
    std::thread::id            threadId;
  };
}

class ThreadPool
{
public:
  explicit ThreadPool(const size_t t_threadCount, Logger* t_logger) : m_maxThreadsUser(t_threadCount),
                                                                      m_logger(t_logger) {
    // if we are not able to get the amount of max concurrent threads
    if (m_maxThreadsUser == 0 || m_maxThreadsHw == 0) {
      // only run on the main thread
      return;
    }

    // make sure user did not request more threads than hw is capable of
    m_maxThreadsUser = std::min(m_maxThreadsUser, m_maxThreadsHw);

    // half of physical cores, at least 1
    const size_t safeMinimumThreads = std::max<size_t>(1, m_maxThreadsUser / 2);

    // pre-spawn a few threads that can be picked up by new tasks before creating more
    // only spawn as many threads as the cpu has, if its a double core, only spawn one
    m_maxPreSpawnThread = std::min(m_maxThreadsUser, safeMinimumThreads);

    for (size_t i = 0; i < m_maxPreSpawnThread; ++i) {
      AddThread([this] { WorkerLoop(); });
    }
  }

  ~ThreadPool() {
    {
      std::lock_guard lock(m_mutex);
      m_shutdown = true;
    }
    m_cv.notify_all();
    // No need to manually join m_workers, jthreads will join automatically
  }

  ThreadPool& operator=(ThreadPool& t_other)  = delete;
  ThreadPool& operator=(ThreadPool&& t_other) = default;
  ThreadPool(ThreadPool& t_other)             = delete;
  ThreadPool(ThreadPool&& t_other) noexcept   = default;

  template <typename F>
  void AddThread(F&& t_f) {
    m_workerPool.emplace_back(std::forward<F>(t_f));
  }

  template <typename F, typename... Args>
  std::future<std::invoke_result_t<F, Args...>> Enqueue(F&& t_f, Args&&... t_args) {
    // the return type of the function being passed
    using ReturnT = std::invoke_result_t<F, Args...>;
    // Wrap the function and its arguments into a packaged_task
    //auto task = std::packaged_task<ReturnT()>(std::bind(std::forward<F>(t_f), std::forward<Args>(t_args)...));

    auto task = std::packaged_task<ReturnT()>(
      [f = std::forward<F>(t_f), ...args = std::forward<Args>(t_args)]() mutable
      {
        return std::invoke(std::move(f), std::move(args)...);
      });

    // get future of task with the proper return type
    auto fut = task.get_future();

    // run on main thread only
    if (m_maxThreadsUser == 0) {
      task();
      return fut;
    }

    {
      std::lock_guard lock(m_mutex);
      unsigned int    taskNumber = ++m_totalTasks;
      // lambda wrap to a void() task to insert into queue
      m_queue.emplace(std::packaged_task<void()>([t = std::move(task)]() mutable { t(); }), taskNumber);

      // If all threads are busy, and we haven't reached maxThreads, spawn a new one
      if (m_idleThreads == 0 && ThreadCount() < m_maxThreadsUser) {
        AddThread([this] { WorkerLoop(); });
      }
    }

    m_cv.notify_one();
    return fut;
  }

  [[nodiscard]] size_t ThreadCount() const { return m_workerPool.size(); }

private:
  /*!
   * @brief A worker intended to be dispatched on a thread that will automatically pick up tasks that are inserted into the queue and will wait if the queue is empty.
   */
  void WorkerLoop() {
    while (true) {
      // we made this std::optional to avoid the overhead of default constructing a QueuedTask
      std::optional<ol::QueuedTask> optTask;

      {
        std::unique_lock lock(m_mutex);
        m_idleThreads++; // thread is now idle
        // make the thread wait until shutdown, or we insert a task
        m_cv.wait(lock, [this] { return m_shutdown || !m_queue.empty(); });
        m_idleThreads--; // thread is waking up

        if (m_shutdown && m_queue.empty()) {
          break;
        }

        // move the next element in the queue to a temp var to run
        optTask = std::move(m_queue.front());
        m_queue.pop();
      }

      // Measure how long this job waited in the queue
      const auto waitTime = optTask->timer.Elapsed();
      // assign threadId once the task gets picked up
      optTask->threadId = std::this_thread::get_id();

      std::string log;

      if (optTask->taskNumber > m_maxPreSpawnThread && optTask->taskNumber <= m_maxThreadsUser) {
        log = std::format(
          "Task #{} waited {:L} before starting on new thread: {}\n",
          optTask->taskNumber,
          waitTime,
          optTask->ThreadIdString());
      }
      else if (optTask->taskNumber > m_maxPreSpawnThread) {
        log = std::format(
          "Task #{} waited {:L} in queue before starting on thread: {}\n",
          optTask->taskNumber,
          waitTime,
          optTask->ThreadIdString());
      }
      else {
        log = std::format(
          "Task #{} assigned to already running thread: {}\n",
          optTask->taskNumber,
          optTask->ThreadIdString());
      }

      if (!log.empty()) {
        m_logger->ThreadSafeLogMessage(log);
      }

      optTask->task(); // run job
    }
  }

  std::mutex                 m_mutex; // Mutex for inserting tasks
  std::condition_variable    m_cv; // Cv to wait threads
  std::queue<ol::QueuedTask> m_queue; // Task queue
  std::vector<std::jthread>  m_workerPool; // Container for dispatched worker threads
  size_t                     m_maxThreadsUser    = 0; // User-defined maximum number of dispatched threads
  size_t                     m_maxThreadsHw      = std::thread::hardware_concurrency(); // Hardware-defined maximum
  size_t                     m_maxPreSpawnThread = 0; // Calculated amount of threads to dispatch pre-emptively
  size_t                     m_idleThreads       = 0; // Amount of dispatched threads that are currently idle
  bool                       m_shutdown          = false;
  std::atomic<unsigned int>  m_totalTasks        = 0; // Global task counter
  Logger*                    m_logger;
};
