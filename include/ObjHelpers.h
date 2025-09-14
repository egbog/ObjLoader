#pragma once
#include <string>
#include <vector>

#include <glm/vec3.hpp>

struct Mesh;
struct LoaderState;
struct Vertex;

class ObjHelpers
{
public:
  static std::string ReadFileToBuffer(const std::string& t_path);
  static void CacheFilePaths(LoaderState& t_state);
  static void ParseObj(LoaderState& t_state,
                       std::vector<Mesh>& t_meshes,
                       const std::string& t_buffer,
                       unsigned int t_lodLevel = 0);
  static void ParseMtl(LoaderState& t_state, const std::string& t_buffer);
  static std::vector<Mesh>& GetMeshContainer(LoaderState& t_state, unsigned int t_lodLevel = 0);
  static std::pair<glm::vec3, glm::vec3> GetTangentCoords(const Vertex& t_v1, const Vertex& t_v2, const Vertex& t_v3);
  static void Triangulate(LoaderState& t_state, std::vector<Mesh>& t_meshes, unsigned int t_lodLevel = 0);
  static void CalcTangentSpace(std::vector<Mesh>& t_meshes);
  static void JoinIdenticalVertices(std::vector<Mesh>& t_meshes);
};
