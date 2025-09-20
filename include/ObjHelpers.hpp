#pragma once
#include <string>
#include <vector>

#include <glm/vec3.hpp>

namespace ol
{
  struct Mesh;
  struct LoaderState;
  struct Vertex;
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
  static const char* ParseFloat(const char* t_ptr, const char* t_end, float& t_out);
  static void                            ParseMtl(ol::LoaderState& t_state, const std::string& t_buffer);
  static std::vector<ol::Mesh>&          GetMeshContainer(ol::LoaderState& t_state, unsigned int t_lodLevel = 0);
  static std::pair<glm::vec3, glm::vec3> GetTangentCoords(const ol::Vertex& t_v1,
                                                          const ol::Vertex& t_v2,
                                                          const ol::Vertex& t_v3);
  static void Triangulate(ol::LoaderState& t_state, std::vector<ol::Mesh>& t_meshes);
  static void CalcTangentSpace(std::vector<ol::Mesh>& t_meshes);
  static void JoinIdenticalVertices(std::vector<ol::Mesh>& t_meshes);
};
