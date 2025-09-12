#include <ObjHelpers.h>
#include <ObjLoader.h>
#include <iostream>
#include <ranges>

#include <Time/Timer.h>

#include <Types/InternalTypes.h>

// Global log sink
inline std::mutex g_logMutex;

ObjLoader::ObjLoader(const size_t t_maxThreads, size_t t_preSpawnThreads) : m_maxThreads(t_maxThreads) {
  // half of physical cores, at least 1
  const size_t safeMinimumThreads = std::max<size_t>(1, t_maxThreads / 2); 

  // pre-spawn a few threads that can be picked up by new tasks before creating more
  if (t_preSpawnThreads == 0) {
    // only spawn as many threads as the cpu has, if its a double core, only spawn one
    t_preSpawnThreads = std::min(t_maxThreads, safeMinimumThreads);
  }

  for (size_t i = 0; i < t_preSpawnThreads; ++i) {
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
  const Timer totalTimer;
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

  //construct our threaded task
  std::packaged_task task(
    [this, t_state = std::move(state), t_objBuffers = std::move(objBuffers), t_mtlBuffers = std::move(mtlBuffers),
      t_totalTimer = totalTimer, t_cacheElapsed = cacheTimer.GetTime(), t_mainThreadId = std::this_thread::get_id()]
    {
      const Timer processTime;
      // since lambda is immutable, and we have to std::move the state, un-const t_state to pass the method for modification
      auto m = LoadFileInternal(const_cast<LoaderState&>(t_state), t_objBuffers, t_mtlBuffers);

      // per-thread log block
      std::ostringstream log;
      log << "Cached all files from model: " << m.path << " in " << t_cacheElapsed << " on thread: " << t_mainThreadId
        << " (main)" << '\n';

      log << "Processed model data: " << m.path << " in " << processTime.GetTime() << " on thread: " <<
        std::this_thread::get_id() << '\n';

      log << "Successfully loaded model: " << m.path << " in " << t_totalTimer.GetTime() << " on thread: " <<
        std::this_thread::get_id() << '\n' << '\n';

      ThreadSafeLog(log.str());

      return m;
    });

  auto fut = task.get_future();

  {
    std::lock_guard lock(m_threadMutex);

    m_tasks.emplace(std::move(task));

    // If all threads are busy and we haven't reached maxThreads, spawn a new one
    if (m_idleThreads == 0 && m_workers.size() < m_maxThreads) {
      m_workers.emplace_back([this] { WorkerLoop(); });
    }
  }
  m_cv.notify_one();

  return fut;
}

void ObjLoader::WorkerLoop() {
  while (true) {
    // we made this std::optional to avoid the overhead of default constructing a packaged_task
    std::optional<std::packaged_task<Model()>> task;

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

    (*task)(); // run job
  }
}

Model ObjLoader::LoadFileInternal(LoaderState&                                         t_state,
                                  const std::unordered_map<unsigned int, std::string>& t_objBuffer,
                                  const std::unordered_map<unsigned int, std::string>& t_mtlBuffer) {
  // Load LOD obj
  for (const auto& [objPath, mtlPath, lodLevel] : t_state.lodPaths | std::views::values) {
    t_state.tempMeshes.clear();
    ObjHelpers::ParseObj(t_state, t_objBuffer.at(lodLevel), lodLevel);
    ObjHelpers::ParseMtl(t_state, t_mtlBuffer.at(lodLevel));
    ObjHelpers::Triangulate(t_state, lodLevel);
    ObjHelpers::CalcTangentSpace(t_state, lodLevel);
    Timer time;
    ObjHelpers::JoinIdenticalVertices(t_state, lodLevel);
    std::cout << "TIME TO PROCESS: " << time.GetTime() << '\n';
  }

  return Model(t_state.meshes, t_state.lodMeshes, t_state.materials, t_state.path);
}

void ObjLoader::ThreadSafeLog(const std::string& t_msg) {
  std::lock_guard lock(g_logMutex);
  std::cout << t_msg;
}
