#pragma once

#include "SceneTypes.h"

#include "Time/Timer.h"

#include <chrono>
#include <future>
#include <string>
#include <vector>

struct Material;

struct TempMeshes
{
  std::vector<glm::vec3>    vertices;
  std::vector<glm::vec2>    texCoords;
  std::vector<glm::vec3>    normals;
  std::vector<glm::uvec3>   faceIndices;
  std::vector<unsigned int> indices;
};

struct File
{
  std::string  objPath;
  std::string  mtlPath;
  unsigned int lodLevel = 0;
};

struct LoaderState
{
  std::string path;
  std::string mtlFileName;

  std::map<unsigned int, File>              lodPaths;
  std::vector<Mesh>                         meshes; // final calculated meshes
  std::map<unsigned int, std::vector<Mesh>> lodMeshes;
  std::vector<Material>                     materials;  // final materials
  std::vector<TempMeshes>                   tempMeshes; // interim storage
};

struct QueuedTask
{
  QueuedTask() = delete;
  QueuedTask(std::packaged_task<Model()> t_task, const unsigned int t_taskNumber) : task(std::move(t_task)), timer(Timer()), taskNumber(t_taskNumber) {}

  [[nodiscard]] std::string ThreadIdString() const {
    std::ostringstream s;
    s << threadId;
    return s.str();
  }

  std::packaged_task<Model()>                    task;
  Timer                                          timer;
  unsigned int                                   taskNumber;
  std::thread::id                                threadId;
};

struct LogEntry
{
  std::string message;
};
