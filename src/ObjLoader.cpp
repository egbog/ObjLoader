#include <ObjHelpers.h>
#include <ObjLoader.h>
#include <iostream>
#include <ranges>

#include <Time/Timer.h>

// Global log sink
inline std::mutex g_logMutex;

ObjLoader::ObjLoader(const size_t t_maxThreads) : m_maxThreads(t_maxThreads) {
  // if we are not able to get the amount of max concurrent threads
  if (m_maxThreads == 0 || m_maxThreadsHw == 0) {
    // only run on the main thread
    return;
  }

  // half of physical cores, at least 1
  const size_t safeMinimumThreads = std::max<size_t>(1, m_maxThreadsHw / 2);

  // pre-spawn a few threads that can be picked up by new tasks before creating more
  // only spawn as many threads as the cpu has, if its a double core, only spawn one
  const size_t preSpawnThreads = std::min(m_maxThreads, safeMinimumThreads);

  for (size_t i = 0; i < preSpawnThreads; ++i) {
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
      const Timer processTime;

      try {
        // since lambda is immutable, and we have to std::move the state,
        // un-const t_state to pass the method for modification
        auto m = LoadFileInternal(const_cast<LoaderState&>(t_state), t_objBuffers, t_mtlBuffers);

        // per-thread log block
        std::ostringstream log;
        log << "\nStarted loading task #" << t_taskNumber << " - model: " << m.path << " on thread: " << std::this_thread::get_id() << '\n';
        //log << "Cached all files in " << t_cacheElapsed << " on thread: " << t_mainThreadId << " (main)\n";
        //log << "Processed in " << processTime.GetTime() << " on thread: " << std::this_thread::get_id() << '\n';
        log << "Successfully loaded task #" << t_taskNumber << " in " << processTime.GetTime() + t_cacheElapsed << "\n";

        ThreadSafeLog(log.str());

        return m;
      }
      catch (const std::exception& e) {
        std::ostringstream log;
        log << "Error loading model on thread " << std::this_thread::get_id() << ": " << e.what() << '\n';
        ThreadSafeLog(log.str());
        throw; // still propagate to future
      }
      catch (...) {
        std::ostringstream log;
        log << "Unknown error loading model on thread " << std::this_thread::get_id() << '\n';
        ThreadSafeLog(log.str());
        throw; // still propagate to future
      }
    });

  std::future<Model> fut = task.get_future();

  // if concurrency is supported
  if (m_maxThreads != 0) {
    std::lock_guard lock(m_threadMutex);

    // the time that it was created and the task number it was assigned
    m_tasks.emplace(std::move(task), std::chrono::high_resolution_clock::now(), m_totalTasks);

    // If all threads are busy, and we haven't reached maxThreads, spawn a new one
    if (m_idleThreads == 0 && m_workers.size() < m_maxThreads) {
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

    task->threadId = std::this_thread::get_id();

    // Measure how long this job waited in the queue
    auto now      = std::chrono::high_resolution_clock::now();
    auto waitTime = std::chrono::duration_cast<std::chrono::milliseconds>(now - task->enqueueTime);

    std::ostringstream ss;
    ss << "Task #" << task->taskNumber << " waited " << waitTime << " in the queue before starting on thread: " << task
      ->threadId << '\n';
    ThreadSafeLog(ss.str());

    task->task(); // run job
  }
}

Model ObjLoader::LoadFileInternal(LoaderState&                                         t_state,
                                  const std::unordered_map<unsigned int, std::string>& t_objBuffer,
                                  const std::unordered_map<unsigned int, std::string>& t_mtlBuffer) {
  // Load LOD obj
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

void ObjLoader::ThreadSafeLog(const std::string& t_msg) {
  std::lock_guard lock(g_logMutex);
  std::cout << t_msg;
}
