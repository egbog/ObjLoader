#pragma once

#include <array>
#include <filesystem>
#include <map>

#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>


namespace obj
{
  using Indices = std::vector<unsigned int>;

  struct Material
  {
    std::string  name;
    std::string  diffuseName;
    std::string  specularName;
    std::string  normalName;
    std::string  heightName;
    bool         isTiled;
    unsigned int index;
  };

  struct Vertex
  {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec4 tangent;

    template <typename T>
    [[nodiscard]] static constexpr bool VecEqual(const T& t_x, const T& t_y) {
      return glm::all(glm::lessThan(glm::abs(t_x - t_y), T(1e-6f)));
    }

    // == operator override for calculateTriangle
    bool operator==(const Vertex& t_other) const {
      return VecEqual(position, t_other.position) && VecEqual(normal, t_other.normal) && VecEqual(texCoords, t_other.texCoords) &&
        VecEqual(tangent, t_other.tangent) && std::abs(tangent.w - t_other.tangent.w) < 1e-6f;
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

  struct VertexHasher
  {
    static constexpr float SCALE = 100000.0f;

    static int Quantize(const float t_v) noexcept {
      return static_cast<int>(std::round(t_v * SCALE));
    }

    size_t operator()(const Vertex& t_v) const noexcept {
      size_t h           = 0;
      auto   hashCombine = [&] (const int t_q)
      {
        h ^= std::hash<int>{}(t_q) + 0x9e3779b9 + (h << 6) + (h >> 2);
      };

      hashCombine(Quantize(t_v.position.x));
      hashCombine(Quantize(t_v.position.y));
      hashCombine(Quantize(t_v.position.z));
      hashCombine(Quantize(t_v.normal.x));
      hashCombine(Quantize(t_v.normal.y));
      hashCombine(Quantize(t_v.normal.z));
      hashCombine(Quantize(t_v.texCoords.x));
      hashCombine(Quantize(t_v.texCoords.y));
      hashCombine(Quantize(t_v.tangent.x));
      hashCombine(Quantize(t_v.tangent.y));
      hashCombine(Quantize(t_v.tangent.z));
      hashCombine(Quantize(t_v.tangent.w));

      return h;
    }
  };

  struct VertexEqual
  {
    bool operator()(const Vertex& t_a, const Vertex& t_b) const noexcept {
      constexpr float eps = 1e-6f;
      return glm::all(glm::lessThan(glm::abs(t_a.position - t_b.position), glm::vec3(eps))) && glm::all(
        glm::lessThan(glm::abs(t_a.normal - t_b.normal), glm::vec3(eps))) && glm::all(
        glm::lessThan(glm::abs(t_a.texCoords - t_b.texCoords), glm::vec2(eps))) && glm::all(
        glm::lessThan(glm::abs(t_a.tangent - t_b.tangent), glm::vec4(eps)));
    }
  };

  struct Mesh
  {
    //-------------------------------------------------------------------------------------------------------------------
    // Constructors/operators
    Mesh() = default;

    Mesh(std::string t_name, const unsigned int t_lodLevel, const int t_meshNumber) : name(std::move(t_name)), material(),
      lodLevel(t_lodLevel), meshNumber(t_meshNumber) {}

    ~Mesh()                              = default;
    Mesh(const Mesh& t_other)            = default;
    Mesh(Mesh&& t_other)                 = default;
    Mesh& operator=(const Mesh& t_other) = default;
    Mesh& operator=(Mesh&& t_other)      = default;
    //-------------------------------------------------------------------------------------------------------------------
    std::string  name;
    Material     material;
    unsigned int lodLevel   = 0;
    int          meshNumber = -1;

    std::vector<Vertex> vertices; // this is fine as an AoS, we access the whole struct at any given time
    Indices             indices;

    size_t baseVertex = 0;
    size_t baseIndex  = 0;
  };

  enum class Flag : uint8_t
  {
    None              = 0,
    CalculateTangents = 1 << 0,
    JoinIdentical     = 1 << 1,
    CombineMeshes     = 1 << 2,
    Lods              = 1 << 3
  };

  // Enable bitwise operations for the enum
  constexpr Flag operator|(Flag t_a, Flag t_b) {
    return static_cast<Flag>(static_cast<uint8_t>(t_a) | static_cast<uint8_t>(t_b));
  }

  constexpr Flag operator&(Flag t_a, Flag t_b) {
    return static_cast<Flag>(static_cast<uint8_t>(t_a) & static_cast<uint8_t>(t_b));
  }

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

    std::vector<File>                               filePaths;      // interim file paths, discarded
    std::map<unsigned int, std::vector<Mesh>>       meshes;         // final calculated meshes, moved
    std::vector<Mesh>                               combinedMeshes; // final combined meshes, moved
    std::map<unsigned int, std::vector<Material>>   materials;      // interim .mtl materials, discarded
    std::map<unsigned int, std::vector<TempMeshes>> tempMeshes;     // interim storage, discarded
  };

  struct Model
  {
    //-------------------------------------------------------------------------------------------------------------------
    // Constructors/operators
    explicit Model(LoaderState& t_state) : meshes(std::move(t_state.meshes)), combinedMeshes(std::move(t_state.combinedMeshes)),
                                           path(std::move(t_state.path)) {}

    ~Model()                           = default;
    Model(const Model&)                = delete;
    Model(Model&&) noexcept            = default;
    Model& operator=(const Model&)     = delete;
    Model& operator=(Model&&) noexcept = default;
    //-------------------------------------------------------------------------------------------------------------------

    std::map<unsigned int, std::vector<Mesh>> meshes;
    std::vector<Mesh>                         combinedMeshes;
    std::filesystem::path                     path;
  };


  std::string                     ReadFileToBuffer(const std::filesystem::path& t_path);
  void                            CacheFilePaths(LoaderState& t_state);
  const char*                     ParseFloat(const char* t_ptr, const char* t_end, float& t_out);
  void                            ParseObj(LoaderState& t_state, const std::string& t_buffer, unsigned int t_lodLevel = 0);
  void                            ParseMtl(LoaderState& t_state, const std::string& t_buffer, const unsigned int& t_lodLevel);
  std::vector<Mesh>&              GetMeshContainer(LoaderState& t_state, unsigned int t_lodLevel = 0);
  std::pair<glm::vec3, glm::vec3> GetTangentCoords(const Vertex& t_v1, const Vertex& t_v2, const Vertex& t_v3);
  void                            ConstructVertices(LoaderState& t_state);
  void                            CalcTangentSpace(LoaderState& t_state);
  void                            JoinIdenticalVertices(LoaderState& t_state);
  void                            CombineMeshes(LoaderState& t_state);
}
