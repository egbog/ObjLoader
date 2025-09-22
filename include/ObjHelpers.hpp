#pragma once
#include <map>
#include <string>
#include <vector>

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
    std::string  objPath;
    std::string  mtlPath;
    unsigned int lodLevel = 0;
  };

  struct LoaderState
  {
    explicit    LoaderState(const Flag t_flags) : flags(t_flags) {}
    std::string path;
    std::string mtlFileName;
    Flag        flags;

    std::map<unsigned int, File>              lodPaths;
    std::map<unsigned int, std::vector<Mesh>> meshes; // final calculated meshes
    std::map<unsigned int, Mesh>              combinedMeshes;
    std::vector<Material>                     materials;  // final materials
    std::vector<TempMeshes>                   tempMeshes; // interim storage
  };
}

class ObjHelpers
{
public:
  static std::string ReadFileToBuffer(const std::string& t_path);
  static void        CacheFilePaths(ol::LoaderState& t_state);
  static void        ParseObj(ol::LoaderState&       t_state,
                              std::vector<ol::Mesh>& t_meshes,
                              const std::string&     t_buffer,
                              unsigned int           t_lodLevel = 0);
  static const char*                     ParseFloat(const char* t_ptr, const char* t_end, float& t_out);
  static void                            ParseMtl(ol::LoaderState& t_state, const std::string& t_buffer);
  static std::vector<ol::Mesh>&          GetMeshContainer(ol::LoaderState& t_state, unsigned int t_lodLevel = 0);
  static std::pair<glm::vec3, glm::vec3> GetTangentCoords(const ol::Vertex& t_v1, const ol::Vertex& t_v2, const ol::Vertex& t_v3);
  static void                            Triangulate(ol::LoaderState& t_state, std::vector<ol::Mesh>& t_meshes);
  static void                            CalcTangentSpace(std::vector<ol::Mesh>& t_meshes);
  static void                            JoinIdenticalVertices(std::vector<ol::Mesh>& t_meshes);
  static void                            CombineMeshes(ol::LoaderState& t_state);
};
