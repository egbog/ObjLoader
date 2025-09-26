#pragma once
#include <array>
#include <map>
#include <string>
#include <vector>

#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

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
    glm::vec4 tangent;

    [[nodiscard]] static bool VecEqual(const glm::vec3& t_x, const glm::vec3& t_y) {
      return glm::all(glm::lessThan(glm::abs(t_x - t_y), glm::vec3(1e-6f)));
    }

    [[nodiscard]] static bool Vec2Equal(const glm::vec2& t_x, const glm::vec2& t_y) {
      return glm::all(glm::lessThan(glm::abs(t_x - t_y), glm::vec2(1e-6f)));
    }

    // == operator override for calculateTriangle
    bool operator==(const Vertex& t_other) const {
      return VecEqual(position, t_other.position) && VecEqual(normal, t_other.normal) && Vec2Equal(texCoords, t_other.texCoords)
        && VecEqual(tangent, t_other.tangent) && std::abs(tangent.w - t_other.tangent.w) < 1e-6f;
    }

    bool operator!=(const Vertex& t_other) const {
      return !(*this == t_other);
    }

    [[nodiscard]] static int Quantize(const float t_v, const float t_scale = 100000) {
      return static_cast<int>(std::round(t_v * t_scale));
    }

    [[nodiscard]] auto AsArrayQuantized() const noexcept {
      return std::array{Quantize(position.x), Quantize(position.y), Quantize(position.z), Quantize(normal.x), Quantize(normal.y),
                        Quantize(normal.z), Quantize(texCoords.x), Quantize(texCoords.y), Quantize(tangent.x),
                        Quantize(tangent.y), Quantize(tangent.z), Quantize(tangent.w)};
    }

    constexpr bool operator<(const Vertex& t_other) const noexcept {
      return AsArrayQuantized() < t_other.AsArrayQuantized();
    }
  };

  struct Mesh
  {
    //-------------------------------------------------------------------------------------------------------------------
    // Constructors/operators
    Mesh() = default;

    Mesh(const std::vector<Vertex>& t_vertices, Indices t_indices) : vertices(t_vertices), indices(std::move(t_indices)) {}

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
                   std::vector<Mesh>& t_combinedMeshes,
                   std::map<unsigned int, std::vector<Material>>& t_materials,
                   std::string& t_path) : meshes(std::move(t_meshes)), materials(std::move(t_materials)),
                                          combinedMeshes(std::move(t_combinedMeshes)), path(std::move(t_path)) {}

    ~Model()                           = default;
    Model(const Model&)                = delete;
    Model(Model&&) noexcept            = default;
    Model& operator=(const Model&)     = delete;
    Model& operator=(Model&&) noexcept = default;
    //-------------------------------------------------------------------------------------------------------------------

    std::map<unsigned int, std::vector<Mesh>>     meshes;
    std::map<unsigned int, std::vector<Material>> materials;
    std::vector<Mesh>                             combinedMeshes;
    std::string                                   path;
  };
}
