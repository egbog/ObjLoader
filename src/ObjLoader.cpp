#include <ObjHelpers.h>
#include <ObjLoader.h>
#include <iostream>
#include <ranges>

#include <Time/Timer.h>

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

std::future<Model> ObjLoader::LoadFile(const std::string& t_path) {
  const Timer cacheTimer;
  LoaderState state;

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
      t_cacheElapsed = cacheTimer.GetTime(), t_mainThreadId = std::this_thread::get_id(), t_taskNumber = taskNumber]
    {
      const Timer        processTime;
      std::string        log;
      std::ostringstream id;
      id << std::this_thread::get_id();

      try {
        // since lambda is immutable, and we have to std::move the state,
        // un-const t_state to pass the method for modification
        auto m = LoadFileInternal(const_cast<LoaderState&>(t_state), t_objBuffers, t_mtlBuffers);

        log = std::format(
          "\nStarted loading task #{} - {} on thread: {}\nSuccessfully loaded task #{} in {:L}\n",
          t_taskNumber,
          m.path,
          id.str(),
          t_taskNumber,
          processTime.GetTime() + t_cacheElapsed);

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

  std::future<Model> fut = task.get_future();

  // if concurrency is supported
  if (m_maxThreadsUser != 0) {
    std::lock_guard lock(m_threadMutex);

    // the time that it was created and the task number it was assigned
    m_tasks.emplace(std::move(task), std::chrono::high_resolution_clock::now(), m_totalTasks);

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

void ObjLoader::WorkerLoop() {
  while (true) {
    // we made this std::optional to avoid the overhead of default constructing a packaged_task
    std::optional<QueuedTask> task;

    {
      std::unique_lock lock(m_threadMutex);

      m_idleThreads++; // thread is now idle

      // make the thread wait until shutdown, or we insert a task
      m_cv.wait(lock, [this] { return m_shutdown || !m_tasks.empty(); });

      m_idleThreads--; // thread is waking up

      if (m_shutdown && m_tasks.empty()) {
        return;
      }

      // move the next element in the queue to a temp var to run
      task = std::move(m_tasks.front());
      m_tasks.pop();
    }

    // Measure how long this job waited in the queue
    auto       now      = std::chrono::high_resolution_clock::now();
    const auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - task->enqueueTime);

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

Model ObjLoader::LoadFileInternal(LoaderState&                                         t_state,
                                  const std::unordered_map<unsigned int, std::string>& t_objBuffer,
                                  const std::unordered_map<unsigned int, std::string>& t_mtlBuffer) {
  // Load obj
  for (const auto& [objPath, mtlPath, lodLevel] : t_state.lodPaths | std::views::values) {
    std::vector<Mesh>& meshes = ObjHelpers::GetMeshContainer(t_state, lodLevel);

    t_state.tempMeshes.clear();
    ObjHelpers::ParseObj(t_state, meshes, t_objBuffer.at(lodLevel), lodLevel);
    ObjHelpers::ParseMtl(t_state, t_mtlBuffer.at(lodLevel));
    ObjHelpers::Triangulate(t_state, meshes, lodLevel);
    ObjHelpers::CalcTangentSpace(meshes);
    ObjHelpers::JoinIdenticalVertices(meshes);
  }

  return Model(t_state.meshes, t_state.lodMeshes, t_state.materials, t_state.path);
}
