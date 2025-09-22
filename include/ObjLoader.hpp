#pragma once
#include "Logger/Logger.hpp"

#include "ThreadPool/ThreadPool.hpp"

#include "Types/Types.hpp"

#include <future>
#include <string>

namespace ol
{
  struct LoaderState;

  enum class Flag : uint8_t
  {
    None              = 0,
    Triangulate       = 1 << 0,
    CalculateTangents = 1 << 1,
    JoinIdentical     = 1 << 2,
    CombineMeshes     = 1 << 3,
    Lods              = 1 << 4
  };

  // Enable bitwise operations for the enum
  constexpr Flag operator|(Flag t_a, Flag t_b) {
    return static_cast<Flag>(static_cast<uint8_t>(t_a) | static_cast<uint8_t>(t_b));
  }

  constexpr Flag operator&(Flag t_a, Flag t_b) {
    return static_cast<Flag>(static_cast<uint8_t>(t_a) & static_cast<uint8_t>(t_b));
  }
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

  std::future<ol::Model> LoadFile(const std::string& t_path, ol::Flag t_flags = ol::Flag::Triangulate);

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
