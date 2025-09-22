#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace ol
{
  using Indices = std::vector<unsigned int>;

  struct Material
  {
    std::string              name;
    std::vector<std::string> diffuseName;
    std::vector<std::string> specularName;
    std::vector<std::string> normalName;
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
}
