#pragma once
#include "Logger/Logger.h"

#include "Types/InternalTypes.h"
#include "Types/SceneTypes.h"

#include <future>
#include <queue>
#include <string>

struct Model;
struct LoaderState;

class ObjLoader
{
public:
  explicit ObjLoader(size_t t_maxThreads = 0);
  ~ObjLoader();
  ObjLoader& operator=(ObjLoader& t_other)  = delete;
  ObjLoader& operator=(ObjLoader&& t_other) = delete;
  ObjLoader(ObjLoader& t_other)             = delete;
  ObjLoader(ObjLoader&& t_other)            = delete;

  std::future<Model> LoadFile(const std::string& t_path);

private:
  std::mutex                m_threadMutex;
  std::vector<std::jthread> m_workers;
  std::queue<QueuedTask>    m_tasks;
  std::condition_variable   m_cv;
  bool                      m_shutdown          = false;
  size_t                    m_idleThreads       = 0;
  size_t                    m_maxThreadsUser    = 0;
  size_t                    m_maxThreadsHw      = std::thread::hardware_concurrency();
  size_t                    m_maxPreSpawnThread = 0;
  std::atomic<unsigned int> m_totalTasks        = 0;
  Logger                    m_logger            = Logger();

  void         WorkerLoop();
  static Model LoadFileInternal(LoaderState&                                         t_state,
                                const std::unordered_map<unsigned int, std::string>& t_objBuffer,
                                const std::unordered_map<unsigned int, std::string>& t_mtlBuffer);
};
