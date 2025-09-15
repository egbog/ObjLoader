
#include "ObjLoader.h"

#include "Types/Types.h"
#include "ObjHelpers.h"
#include <iostream>
#include <ranges>

/*!
 * @brief Initializes the instance and dispatches an appropriate number of threads pre-emptively, ready to pick up tasks
 * @param t_maxThreads User desired maximum amount of threads to dispatch across the whole instance.
 * \n A small portion of this will be pre-dispatched
 * \n Note if files are small enough, the amount of dispatched threads may not actually reach this limit
 */
ObjLoader::ObjLoader(const size_t t_maxThreads) : m_maxThreadsUser(t_maxThreads) {
  m_logger.DispatchWorkerThread();

  // if we are not able to get the amount of max concurrent threads
  if (m_maxThreadsUser == 0 || m_maxThreadsHw == 0) {
    // only run on the main thread
    return;
  }

  // half of physical cores, at least 1
  const size_t safeMinimumThreads = std::max<size_t>(1, m_maxThreadsHw / 2);

  // pre-spawn a few threads that can be picked up by new tasks before creating more
  // only spawn as many threads as the cpu has, if its a double core, only spawn one
  m_maxPreSpawnThread = std::min(m_maxThreadsUser, safeMinimumThreads);

  for (size_t i = 0; i < m_maxPreSpawnThread; ++i) {
    m_workers.emplace_back([this] { WorkerLoop(); });
  }
}

ObjLoader::~ObjLoader() {
  {
    std::lock_guard lock(m_threadMutex);
    m_shutdown = true;
  }
  m_cv.notify_all();
  // No need to manually join m_workers, jthreads will join automatically
}

/*!
 * @brief Loads an obj + mtl file asynchronously
 * @param t_path Relative path to obj file, including file extension
 * @return std::future<Model> of the created task that loads the file
 */
std::future<ol::Model> ObjLoader::LoadFile(const std::string& t_path) {
  const Timer     cacheTimer;
  ol::LoaderState state;

  std::unordered_map<unsigned int, std::string> mtlBuffers;
  std::unordered_map<unsigned int, std::string> objBuffers;

  state.path = t_path;

  // get file paths of all obj, mtl and lods
  ObjHelpers::CacheFilePaths(state);

  // read all files to memory on main thread
  for (const auto& [objPath, mtlPath, lodLevel] : state.lodPaths | std::views::values) {
    objBuffers[lodLevel] = ObjHelpers::ReadFileToBuffer(objPath);

    if (!mtlPath.empty()) {
      mtlBuffers[lodLevel] = ObjHelpers::ReadFileToBuffer(mtlPath);
    }
  }

  // assign task number before creating task and pass by value
  unsigned int taskNumber = ++m_totalTasks; // atomic increment

  //construct our threaded task
  std::packaged_task task(
    [this, t_state = std::move(state), t_objBuffers = std::move(objBuffers), t_mtlBuffers = std::move(mtlBuffers),
      t_cacheElapsed = cacheTimer.Elapsed<std::milli>(), t_mainThreadId = std::this_thread::get_id(), t_taskNumber =
      taskNumber]
    {
      std::string        log;
      std::ostringstream id;
      id << std::this_thread::get_id();

      try {
        const Timer processTime;

        // since lambda is immutable, and we have to std::move the state,
        // un-const t_state to pass the method for modification
        auto m = LoadFileInternal(const_cast<ol::LoaderState&>(t_state), t_objBuffers, t_mtlBuffers);

        log = std::format(
          "\nStarted loading task #{} - {} on thread: {}\nSuccessfully loaded task #{} in {:L}\n",
          t_taskNumber,
          m.path,
          id.str(),
          t_taskNumber,
          processTime.Elapsed<std::milli>() + t_cacheElapsed);

        m_logger.ThreadSafeLogMessage(log);

        return m;
      }
      catch (const std::exception& e) {
        log = std::format("\nError loading model on thread {}: {}", id.str(), e.what());
        m_logger.ThreadSafeLogMessage(log);
        throw; // still propagate to future
      }
      catch (...) {
        log = std::format("\nError loading model on thread {}", id.str());
        m_logger.ThreadSafeLogMessage(log);
        throw; // still propagate to future
      }
    });

  std::future<ol::Model> fut = task.get_future();

  // if concurrency is supported
  if (m_maxThreadsUser != 0) {
    std::lock_guard lock(m_threadMutex);

    // the time that it was created and the task number it was assigned
    m_taskQueue.emplace(std::move(task), m_totalTasks);

    // If all threads are busy, and we haven't reached maxThreads, spawn a new one
    if (m_idleThreads == 0 && m_workers.size() < m_maxThreadsUser) {
      m_workers.emplace_back([this] { WorkerLoop(); });
    }
  }
  // if not just run the task right away on main thread
  else {
    task();
  }

  m_cv.notify_one();

  return fut;
}

/*!
 * @brief A worker intended to be dispatched on a thread that will automatically pick up tasks that are inserted into the queue and will wait if the queue is empty.
 */
void ObjLoader::WorkerLoop() {
  while (true) {
    // we made this std::optional to avoid the overhead of default constructing a packaged_task
    std::optional<ol::QueuedTask> task;

    {
      std::unique_lock lock(m_threadMutex);

      m_idleThreads++; // thread is now idle

      // make the thread wait until shutdown, or we insert a task
      m_cv.wait(lock, [this] { return m_shutdown || !m_taskQueue.empty(); });

      m_idleThreads--; // thread is waking up

      if (m_shutdown && m_taskQueue.empty()) {
        break;
      }

      // move the next element in the queue to a temp var to run
      task = std::move(m_taskQueue.front());
      m_taskQueue.pop();
    }

    // Measure how long this job waited in the queue
    const auto waitTime = task->timer.Elapsed<std::milli>();

    // assign threadId once the task gets picked up
    task->threadId = std::this_thread::get_id();

    std::string log;

    if (task->taskNumber > m_maxPreSpawnThread && task->taskNumber <= m_maxThreadsUser) {
      log = std::format(
        "Task #{} waited {:L} before starting on new thread: {}\n",
        task->taskNumber,
        waitTime,
        task->ThreadIdString());
    }
    else if (task->taskNumber > m_maxPreSpawnThread) {
      log = std::format(
        "Task #{} waited {:L} in queue before starting on thread: {}\n",
        task->taskNumber,
        waitTime,
        task->ThreadIdString());
    }
    else {
      log = std::format("Task #{} assigned to already running thread: {}\n", task->taskNumber, task->ThreadIdString());
    }

    if (!log.empty()) {
      m_logger.ThreadSafeLogMessage(log);
    }

    task->task(); // run job
  }
}

/*!
 * @brief Parses and processes every file associated with the specified t_path given to LoadFile()
 * @param t_state The instance-thread specific state data that houses temporary processing containers
 * @param t_objBuffer Map of every detected obj loaded into memory as a std::string
 * @param t_mtlBuffer Map of every detected mtl loaded into memory as a std::string
 * @return Rvalue Model constructed with the processed data
 */
ol::Model ObjLoader::LoadFileInternal(ol::LoaderState&                                     t_state,
                                      const std::unordered_map<unsigned int, std::string>& t_objBuffer,
                                      const std::unordered_map<unsigned int, std::string>& t_mtlBuffer) {
  // Load obj
  for (const auto& [objPath, mtlPath, lodLevel] : t_state.lodPaths | std::views::values) {
    std::vector<ol::Mesh>& meshes = ObjHelpers::GetMeshContainer(t_state, lodLevel);

    t_state.tempMeshes.clear();
    ObjHelpers::ParseObj(t_state, meshes, t_objBuffer.at(lodLevel), lodLevel);
    ObjHelpers::ParseMtl(t_state, t_mtlBuffer.at(lodLevel));
    ObjHelpers::Triangulate(t_state, meshes);
    ObjHelpers::CalcTangentSpace(meshes);
    ObjHelpers::JoinIdenticalVertices(meshes);
  }

  return ol::Model(t_state.meshes, t_state.lodMeshes, t_state.materials, t_state.path);
}
