#pragma once

#include <filesystem>
#include <map>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace ol
{
  enum class Flag : uint8_t;
  struct File;
  struct Material;
  struct Mesh;
  struct LoaderState;
  struct Vertex;

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
    std::filesystem::path objPath;
    std::filesystem::path mtlPath;
    unsigned int          lodLevel = 0;
  };

  struct LoaderState
  {
    explicit              LoaderState(const Flag t_flags) : flags(t_flags) {}
    std::filesystem::path path;
    std::string           mtlFileName;
    Flag                  flags;

    std::vector<File>                             filePaths;
    std::map<unsigned int, std::vector<Mesh>>     meshes; // final calculated meshes
    std::vector<Mesh>                             combinedMeshes;
    std::map<unsigned int, std::vector<Material>> materials;  // final materials
    std::vector<TempMeshes>                       tempMeshes; // interim storage
  };

  std::string ReadFileToBuffer(const std::filesystem::path& t_path);
  void CacheFilePaths(LoaderState& t_state);
  const char* ParseFloat(const char* t_ptr, const char* t_end, float& t_out);
  void ParseObj(LoaderState& t_state, std::vector<Mesh>& t_meshes, const std::string& t_buffer, unsigned int t_lodLevel = 0);
  void ParseMtl(LoaderState& t_state, const std::string& t_buffer, const unsigned int& t_lodLevel);
  std::vector<Mesh>& GetMeshContainer(LoaderState& t_state, unsigned int t_lodLevel = 0);
  std::pair<glm::vec3, glm::vec3> GetTangentCoords(const Vertex& t_v1, const Vertex& t_v2, const Vertex& t_v3);
  void Triangulate(LoaderState& t_state, std::vector<Mesh>& t_meshes);
  void CalcTangentSpace(std::vector<Mesh>& t_meshes);
  void JoinIdenticalVertices(std::vector<Mesh>& t_meshes);
  void CombineMeshes(LoaderState& t_state);
}
