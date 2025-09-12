#pragma once

#include "MaterialTypes.h"

#include <map>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

class Texture;

using Indices = std::vector<unsigned int>;

struct Vertex
{
  glm::vec3 position;
  glm::vec3 normal;
  glm::vec2 texCoords;
  glm::vec3 tangent;
  glm::vec3 biTangent;

  // == operator override for calculateTriangle
  bool operator==(const Vertex& t_other) const {
    return position == t_other.position && normal == t_other.normal && texCoords == t_other.texCoords && tangent ==
      t_other.tangent && biTangent == t_other.biTangent;
  }
};

struct Mesh
{
  //-------------------------------------------------------------------------------------------------------------------
  // Constructors/operators
  Mesh() = default;

  Mesh(const std::vector<Vertex>& t_vertices, Indices t_indices) : vertices(t_vertices),
                                                                   indices(std::move(t_indices)) {}

  ~Mesh()                              = default;
  Mesh(const Mesh& t_other)                  = default;
  Mesh(Mesh&& t_other)                 = default;
  Mesh& operator=(const Mesh& t_other) = default;
  Mesh& operator=(Mesh&& t_other)      = default;
  //-------------------------------------------------------------------------------------------------------------------

  /*Mesh& operator=(Mesh&& t_other) noexcept {
    if (this != &t_other) {
      glDeleteBuffers(1, &m_id);
      m_id         = t_other.m_id;
      m_type       = t_other.m_type;
      t_other.m_id = 0;
    }
    return *this;
  }*/

  std::string name;
  std::string material;
  unsigned    lodLevel   = 0;
  int         meshNumber = -1;

  std::vector<Vertex> vertices; // this is fine as an AoS, we access the whole struct at any given time
  Indices             indices;
};

struct Model
{
  //-------------------------------------------------------------------------------------------------------------------
  // Constructors/operators
  explicit Model(std::vector<Mesh>&                         t_meshes,
                 std::map<unsigned int, std::vector<Mesh>>& t_lods,
                 std::vector<Material>&                     t_materials,
                 std::string&                               t_path) : meshes(std::move(t_meshes)),
                                                                      lods(std::move(t_lods)),
                                                                      materials(std::move(t_materials)),
                                                                      path(std::move(t_path)) {}

  ~Model()                           = default;
  Model(const Model&)                = delete;
  Model(Model&&) noexcept            = default;
  Model& operator=(const Model&)     = delete;
  Model& operator=(Model&&) noexcept = default;
  //-------------------------------------------------------------------------------------------------------------------

  std::vector<Mesh>                         meshes;
  std::map<unsigned int, std::vector<Mesh>> lods;
  std::vector<Material>                     materials;
  std::string                               path;
};

// Hash function for the Vertex struct to use it in unordered_map
template <>
struct std::hash<Vertex>
{
  size_t operator()(const Vertex& t_v) const noexcept {
    const size_t hash1 = std::hash<float>{}(t_v.position.x) ^ std::hash<float>{}(t_v.position.y) ^ std::hash<float>{}(
      t_v.position.z);
    const size_t hash2 = std::hash<float>{}(t_v.texCoords.x) ^ std::hash<float>{}(t_v.texCoords.y);
    const size_t hash3 = std::hash<float>{}(t_v.normal.x) ^ std::hash<float>{}(t_v.normal.y) ^ std::hash<float>{}(
      t_v.normal.z);
    const size_t hash4 = std::hash<float>{}(t_v.tangent.x) ^ std::hash<float>{}(t_v.tangent.y) ^ std::hash<float>{}(
      t_v.tangent.z);
    const size_t hash5 = std::hash<float>{}(t_v.biTangent.x) ^ std::hash<float>{}(t_v.biTangent.y) ^ std::hash<float>{}(
      t_v.biTangent.z);
    return hash1 ^ hash2 ^ hash3 ^ hash4 ^ hash5;
  }
};
