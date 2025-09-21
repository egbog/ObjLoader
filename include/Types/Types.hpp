#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace ol
{
  struct Material;
  struct Mesh;
  using Indices = std::vector<unsigned int>;

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

  enum class Flag : uint8_t
  {
    None              = 0,
    Triangulate       = 1 << 0,
    CalculateTangents = 1 << 1,
    JoinIdentical     = 1 << 2,
    CombineMeshes     = 1 << 3
  };

  // Enable bitwise operations for the enum
  constexpr Flag operator|(Flag t_a, Flag t_b) {
    return static_cast<Flag>(static_cast<uint8_t>(t_a) | static_cast<uint8_t>(t_b));
  }

  constexpr Flag operator&(Flag t_a, Flag t_b) {
    return static_cast<Flag>(static_cast<uint8_t>(t_a) & static_cast<uint8_t>(t_b));
  }

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

  struct Vertex
  {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec3 tangent;
    glm::vec3 biTangent;

    // == operator override for calculateTriangle
    constexpr bool operator==(const Vertex& t_other) const {
      return position == t_other.position && normal == t_other.normal && texCoords == t_other.texCoords;
    }

    constexpr bool operator!=(const Vertex& t_other) const {
      return !(*this == t_other);
    }

    [[nodiscard]] constexpr auto AsArray() const noexcept {
      return std::array{position.x, position.y, position.z, normal.x, normal.y, normal.z, texCoords.x, texCoords.y};
    }


    constexpr bool operator<(const Vertex& t_other) const noexcept {
      return AsArray() < t_other.AsArray();
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
    Mesh(const Mesh& t_other)            = default;
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

    std::string  name;
    std::string  material;
    unsigned int lodLevel   = 0;
    int          meshNumber = -1;

    std::vector<Vertex> vertices; // this is fine as an AoS, we access the whole struct at any given time
    Indices             indices;
  };

  struct Model
  {
    //-------------------------------------------------------------------------------------------------------------------
    // Constructors/operators
    explicit Model(std::map<unsigned int, std::vector<Mesh>>& t_meshes,
                   std::map<unsigned int, Mesh>               t_combinedMeshes,
                   std::vector<Material>&                     t_materials,
                   std::string&                               t_path) : meshes(std::move(t_meshes)),
                                                                        combinedMeshes(std::move(t_combinedMeshes)),
                                                                        materials(std::move(t_materials)),
                                                                        path(std::move(t_path)) {}

    ~Model()                           = default;
    Model(const Model&)                = delete;
    Model(Model&&) noexcept            = default;
    Model& operator=(const Model&)     = delete;
    Model& operator=(Model&&) noexcept = default;
    //-------------------------------------------------------------------------------------------------------------------

    std::map<unsigned int, std::vector<Mesh>> meshes;
    std::map<unsigned int, Mesh>              combinedMeshes;
    std::vector<Material>                     materials;
    std::string                               path;
  };

  struct Material
  {
    std::string              name;
    std::vector<std::string> diffuseName;
    std::vector<std::string> specularName;
    std::vector<std::string> normalName;
  };
}
