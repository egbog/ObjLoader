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

  unsigned int WorkerCount() const { return m_workers.size(); }

private:
  std::mutex                m_threadMutex;                                             // Mutex for inserting tasks
  std::vector<std::jthread> m_workers;                                                 // Container for dispatched worker threads
  std::queue<QueuedTask>    m_tasks;                                                   // Task queue
  std::condition_variable   m_cv;                                                      // Cv to wait threads
  bool                      m_shutdown          = false;
  size_t                    m_idleThreads       = 0;                                   // Amount of dispatched threads that are currently idle
  size_t                    m_maxThreadsUser    = 0;                                   // User-defined maximum number of dispatched threads
  size_t                    m_maxThreadsHw      = std::thread::hardware_concurrency(); // Hardware-defined maximum
  size_t                    m_maxPreSpawnThread = 0;                                   // Calculated amount of threads to dispatch pre-emptively
  std::atomic<unsigned int> m_totalTasks        = 0;                                   // Global task counter
  Logger                    m_logger            = Logger();

  void         WorkerLoop();
  static Model LoadFileInternal(LoaderState&                                         t_state,
                                const std::unordered_map<unsigned int, std::string>& t_objBuffer,
                                const std::unordered_map<unsigned int, std::string>& t_mtlBuffer);
};

