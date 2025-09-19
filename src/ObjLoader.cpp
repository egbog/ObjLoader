#include "ObjLoader.h"

#include "ObjHelpers.h"

#include "Types/Types.h"

#include <algorithm>
#include <iostream>
#include <ranges>

/*!
 * @brief Initializes the instance and dispatches an appropriate number of threads pre-emptively, ready to pick up tasks
 * @param t_maxThreads User desired maximum amount of threads to dispatch across the whole instance.
 * \n A small portion of this will be pre-dispatched
 * \n Note if files are small enough, the amount of dispatched threads may not actually reach this limit
 */
ObjLoader::ObjLoader(const size_t t_maxThreads) : m_maxThreadsUser(t_maxThreads),
                                                  m_threadPool(ThreadPool(m_maxThreadsUser, &m_logger)) {
  m_logger.DispatchWorkerThread();
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
  /*std::packaged_task task(
    [this, t_state = std::move(state), t_objBuffers = std::move(objBuffers), t_mtlBuffers = std::move(mtlBuffers),
      t_cacheElapsed = cacheTimer.Elapsed(), t_mainThreadId = std::this_thread::get_id(), t_taskNumber = taskNumber]
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
          processTime.Elapsed() + t_cacheElapsed);

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
    });*/

  //std::future<ol::Model> fut = task.get_future();

  // the time that it was created and the task number it was assigned
  return m_threadPool.Enqueue(
    &ObjLoader::ConstructTask,
    this,
    std::move(state),
    std::move(objBuffers),
    std::move(mtlBuffers),
    cacheTimer.Elapsed(),
    taskNumber);
}

ol::Model ObjLoader::ConstructTask(const ol::LoaderState&                               t_state,
                                   const std::unordered_map<unsigned int, std::string>& t_objBuffers,
                                   const std::unordered_map<unsigned int, std::string>& t_mtlBuffers,
                                   const std::chrono::duration<double, std::milli>      t_cacheElapsed,
                                   unsigned int                                         t_taskNumber) {
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
      processTime.Elapsed() + t_cacheElapsed);

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
