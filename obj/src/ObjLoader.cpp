#include "obj/ObjLoader.hpp"

#include "obj/ObjHelpers.hpp"

/*!
 * @brief Initializes the instance and dispatches an appropriate number of threads pre-emptively, ready to pick up tasks
 * @param t_maxThreads User desired maximum amount of threads to dispatch across the whole instance.
 * \n A small portion of this will be pre-dispatched
 * \n Note if files are small enough, the amount of dispatched threads may not actually reach this limit
 */
ObjLoader::ObjLoader(const size_t t_maxThreads) : m_maxThreadsUser(t_maxThreads), m_threadPool(ThreadPool(m_maxThreadsUser)) {
  //m_logger->DispatchWorkerThread();
}

/*!
 * @brief Loads an obj + mtl file asynchronously
 * @param t_path Relative path to obj file, including file extension
 * @param t_flags 
 * @return std::future<Model> of the created task that loads the file
 */
std::future<obj::Model> ObjLoader::LoadFile(const std::filesystem::path& t_path, obj::Flag t_flags) {
  const Timer      cacheTimer;
  obj::LoaderState state(t_flags);

  std::unordered_map<unsigned int, std::string> mtlBuffers;
  std::unordered_map<unsigned int, std::string> objBuffers;

  state.path = t_path;

  // get file paths of all obj, mtl and lods
  CacheFilePaths(state);

  // read all files to memory on main thread
  for (const auto& [objPath, mtlPath, lodLevel] : state.filePaths) {
    objBuffers[lodLevel] = obj::ReadFileToBuffer(objPath);

    if (mtlPath.empty()) {
      m_logger->Log<Logger::Warning>(std::format("No mtl found for file: {}", objPath.string()));
    }

    mtlBuffers[lodLevel] = obj::ReadFileToBuffer(mtlPath);
  }

  // assign task number before creating task and pass by value
  unsigned int taskNumber = ++m_totalTasks; // atomic increment

  //construct our threaded task
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

obj::Model ObjLoader::ConstructTask(const obj::LoaderState&                              t_state,
                                    const std::unordered_map<unsigned int, std::string>& t_objBuffers,
                                    const std::unordered_map<unsigned int, std::string>& t_mtlBuffers,
                                    const std::chrono::duration<double, std::milli>      t_cacheElapsed,
                                    unsigned int                                         t_taskNumber) {
  std::string        log;
  std::ostringstream id;
  id << std::this_thread::get_id();

  try {
    const Timer processTime;
    const auto parent = t_state.path.parent_path().parent_path().parent_path(); // two levels up
    log = std::format("Started loading task #{} - {} on thread: {}", t_taskNumber, t_state.path.lexically_relative(parent).generic_string(), id.str());
    m_logger->Log<Logger::Debug>(log);

    // since lambda is immutable, and we have to std::move the state,
    // un-const t_state to pass the method for modification
    auto m = LoadFileInternal(const_cast<obj::LoaderState&>(t_state), t_objBuffers, t_mtlBuffers);

    log = std::format("Successfully loaded task #{} in {:L}", t_taskNumber, processTime.Elapsed() + t_cacheElapsed);
    m_logger->Log<Logger::Debug>(log);

    return m;
  }
  catch (const std::exception& e) {
    log = std::format("Error loading model on thread {}: {}", id.str(), e.what());
    m_logger->Log<Logger::Error>(log);
    throw; // still propagate to future
  }
  catch (...) {
    log = std::format("Error loading model on thread {}", id.str());
    m_logger->Log<Logger::Error>(log);
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
obj::Model ObjLoader::LoadFileInternal(obj::LoaderState&                                    t_state,
                                       const std::unordered_map<unsigned int, std::string>& t_objBuffer,
                                       const std::unordered_map<unsigned int, std::string>& t_mtlBuffer) {
  // Load obj
  for (const auto& [objPath, mtlPath, lodLevel] : t_state.filePaths) {
    std::vector<obj::Mesh>& meshes = obj::GetMeshContainer(t_state, lodLevel);

    t_state.tempMeshes.clear();
    obj::ParseMtl(t_state, t_mtlBuffer.at(lodLevel), lodLevel);
    obj::ParseObj(t_state, meshes, t_objBuffer.at(lodLevel), lodLevel);

    obj::ConstructVertices(t_state, meshes);

    if ((t_state.flags & obj::Flag::JoinIdentical) == obj::Flag::JoinIdentical) {
      obj::JoinIdenticalVertices(meshes);
    }

    if ((t_state.flags & obj::Flag::CalculateTangents) == obj::Flag::CalculateTangents) {
      obj::CalcTangentSpace(meshes);
    }

  }

  if ((t_state.flags & obj::Flag::CombineMeshes) == obj::Flag::CombineMeshes) {
    obj::CombineMeshes(t_state);
  }

  return obj::Model(t_state.meshes, t_state.combinedMeshes, t_state.materials, t_state.path);
}
