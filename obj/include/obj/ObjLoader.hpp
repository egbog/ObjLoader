#pragma once

#include "pool/ThreadPool.hpp"

#include <filesystem>

namespace ol
{
  enum class Flag : uint8_t;
  struct Model;
  struct LoaderState;


}

class Logger;

class ObjLoader
{
public:
  //-------------------------------------------------------------------------------------------------------------------
  // Constructors/operators
  explicit ObjLoader(size_t t_maxThreads = 0);
  ~ObjLoader()                              = default;
  ObjLoader& operator=(ObjLoader& t_other)  = delete;
  ObjLoader& operator=(ObjLoader&& t_other) = delete;
  ObjLoader(ObjLoader& t_other)             = delete;
  ObjLoader(ObjLoader&& t_other)            = delete;
  //-------------------------------------------------------------------------------------------------------------------

  std::future<ol::Model> LoadFile(const std::filesystem::path& t_path, ol::Flag t_flags);

  [[nodiscard]] constexpr size_t WorkerCount() const { return m_threadPool.ThreadCount(); }

private:
  size_t                    m_maxThreadsUser = 0; // User-defined maximum number of dispatched threads
  std::atomic<unsigned int> m_totalTasks     = 0; // Global task counter
  Logger                    m_logger;
  ThreadPool                m_threadPool;

  ol::Model ConstructTask(const ol::LoaderState&                               t_state,
                          const std::unordered_map<unsigned int, std::string>& t_objBuffers,
                          const std::unordered_map<unsigned int, std::string>& t_mtlBuffers,
                          std::chrono::duration<double, std::milli>            t_cacheElapsed,
                          unsigned int                                         t_taskNumber);
  static ol::Model LoadFileInternal(ol::LoaderState&                                     t_state,
                                    const std::unordered_map<unsigned int, std::string>& t_objBuffer,
                                    const std::unordered_map<unsigned int, std::string>& t_mtlBuffer);
};
