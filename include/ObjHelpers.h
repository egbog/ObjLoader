#pragma once
#include <span>
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
  static void ParseObj(LoaderState& t_state, const std::string& t_buffer, unsigned int t_lodLevel = 0);
  static void ParseMtl(LoaderState& t_state, const std::string& t_buffer);
  static std::vector<Mesh>& GetMeshContainer(LoaderState& t_state, unsigned int t_lodLevel = 0);
  static std::pair<glm::vec3, glm::vec3> GetTangentCoords(const std::span<Vertex>& t_v);
  static void Triangulate(LoaderState& t_state, unsigned int t_lodLevel = 0);
  static void CalcTangentSpace(LoaderState& t_state, unsigned int t_lodLevel = 0);
  static void JoinIdenticalVertices(LoaderState& t_state, unsigned int t_lodLevel = 0);
};
