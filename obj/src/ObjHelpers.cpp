#include "obj/ObjHelpers.hpp"

#include "obj/ObjLoader.hpp"

#include <fstream>
#include <ranges>

#include <fast_float/fast_float.h>

#include <glm/geometric.hpp>

namespace obj
{
  /*!
   * @brief Reads specified file path to a std::string buffer
   * @param t_path Path to file including file extension
   * @return String buffer
   */
  std::string ReadFileToBuffer(const std::filesystem::path& t_path) {
    std::ifstream in(t_path, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
      throw std::runtime_error("Failed to open file: " + t_path.string());
    }

    const auto  size = in.tellg();
    std::string buffer(size, '\0');
    in.seekg(0, std::ios::beg);
    in.read(buffer.data(), size);

    return buffer;
  }

  /*!
   * @brief Finds all lod files if any and their corresponding mtl files and stores them
   * @param t_state Internal state data to store the file paths in
   */
  void CacheFilePaths(LoaderState& t_state) {
    // whole file path
    const std::filesystem::path basePath = t_state.path;
    // root directory of file
    const std::filesystem::path dir = basePath.parent_path().empty()
                                        ? "." // current directory if none
                                        : basePath.parent_path();
    // just the filename minus the extension
    const std::filesystem::path fileName = basePath.stem();

    // file path to corresponding mtl file
    const std::filesystem::path mtlPath = (dir / fileName).string() + ".mtl";

    t_state.filePaths.emplace_back();

    //store base mesh at lod0
    t_state.filePaths[0] = File{.objPath = t_state.path, .mtlPath = mtlPath.string(), .lodLevel = 0};

    if ((t_state.flags & Flag::Lods) == Flag::Lods) {
      // find lods
      for (auto& entry : std::filesystem::directory_iterator(dir)) {
        // skip if it is not a normal file
        if (!entry.is_regular_file()) {
          continue;
        }

        const auto entryExtension = entry.path().extension(); // ".obj" or ".mtl"
        const auto entryFileName  = entry.path().stem();      // filename without extension

        // Look for "_lod" substring inside filename (e.g., "rock_lod1")
        const auto pos = entryFileName.string().find(fileName.string() + "_lod");
        if (pos == std::string::npos) {
          continue; // skip non-lod files
        }

        // Extract numeric part after "_lod"
        std::string lodNumStr = entryFileName.string().substr(pos + fileName.string().length() + 4);
        try {
          unsigned int lodIndex = std::stoi(lodNumStr);

          // create element if it doesn't exist
          if (lodIndex >= t_state.filePaths.size()) {
            t_state.filePaths.emplace_back("", "", lodIndex);
          }

          // Assign paths depending on file type
          if (entryExtension == ".obj") {
            t_state.filePaths[lodIndex].objPath = entry.path().string();
          }
          else if (entryExtension == ".mtl") {
            t_state.filePaths[lodIndex].mtlPath = entry.path().string();
          }
        }
        catch (...) {
          break; // invalid suffix, skip file
        }
      }
    }
  }

  const char* ParseFloat(const char* t_ptr, const char* t_end, float& t_out) {
    auto [ptr, ec] = fast_float::from_chars(t_ptr, t_end, t_out, fast_float::chars_format::skip_white_space);
    if (ec != std::errc{}) {
      throw std::runtime_error("OBJ parse error: invalid float");
    }
    return ptr;
  }

  /*!
   * @brief Pointer walks through the obj file and stores vertex data in LoaderState
   * @param t_state Internal state data to store vertex data
   * @param t_meshes List of meshes created by parsing
   * @param t_buffer String buffer of current file
   * @param t_lodLevel Specified lod level, if any
   */
  void ParseObj(LoaderState& t_state, std::vector<Mesh>& t_meshes, const std::string& t_buffer, const unsigned int t_lodLevel) {
    int meshCount = -1;

    const char* data = t_buffer.data();
    const char* end  = data + t_buffer.size();

    // --- First pass: count meshes and estimate sizes ---
    size_t              meshEstimate = 0;
    std::vector<size_t> verticesPerMesh;
    std::vector<size_t> texPerMesh;
    std::vector<size_t> normalsPerMesh;
    std::vector<size_t> facesPerMesh;

    while (data < end) {
      const char* lineStart = data;
      while (data < end && *data != '\n' && *data != '\r') {
        ++data;
      }
      std::string_view line(lineStart, data - lineStart);
      while (data < end && (*data == '\n' || *data == '\r')) {
        ++data;
      }

      if (line.empty() || line[0] == '#') {
        continue;
      }

      if (line.starts_with("o ")) {
        meshEstimate++;
        verticesPerMesh.push_back(0);
        texPerMesh.push_back(0);
        normalsPerMesh.push_back(0);
        facesPerMesh.push_back(0);
      }
      else if (line.starts_with("v ")) {
        if (!verticesPerMesh.empty()) {
          verticesPerMesh.back()++;
        }
      }
      else if (line.starts_with("vt")) {
        if (!texPerMesh.empty()) {
          texPerMesh.back()++;
        }
      }
      else if (line.starts_with("vn")) {
        if (!normalsPerMesh.empty()) {
          normalsPerMesh.back()++;
        }
      }
      else if (line.starts_with("f ")) {
        if (!facesPerMesh.empty()) {
          facesPerMesh.back()++;
        }
      }
    }

    // Pre-reserve tempMeshes and meshes vectors
    t_state.tempMeshes.reserve(meshEstimate);
    t_meshes.reserve(meshEstimate);

    // --- Second pass: actual parsing ---
    data = t_buffer.data();
    std::string meshName;
    float       x, y, z;

    glm::uvec3 indexOffset{0};
    glm::uvec3 maxIndexSeen{0};

    glm::vec2 uvMin(FLT_MAX);
    glm::vec2 uvMax(-FLT_MAX);

    unsigned int mtlCount = 0;

    while (data < end) {
      const char* lineStart = data;
      while (data < end && *data != '\n' && *data != '\r') {
        ++data;
      }
      std::string_view line(lineStart, data - lineStart);
      while (data < end && (*data == '\n' || *data == '\r')) {
        ++data;
      }

      if (line.empty() || line[0] == '#') {
        continue;
      }

      if (line.starts_with("o ")) {
        meshName = std::string(line.substr(2));
        meshCount++;

        t_state.tempMeshes.emplace_back();
        t_meshes.emplace_back();
        indexOffset = maxIndexSeen; // carry forward for next mesh

        // Pre-reserve per-mesh vectors based on first-pass counts
        if (int size = static_cast<int>(verticesPerMesh.size()); meshCount < size) {
          t_state.tempMeshes[meshCount].vertices.reserve(verticesPerMesh[meshCount]);
          t_state.tempMeshes[meshCount].texCoords.reserve(texPerMesh[meshCount]);
          t_state.tempMeshes[meshCount].normals.reserve(normalsPerMesh[meshCount]);
          t_state.tempMeshes[meshCount].faceIndices.reserve(facesPerMesh[meshCount] * 3); // 3 indices per face
        }

        t_meshes[meshCount].name       = meshName;
        t_meshes[meshCount].meshNumber = meshCount;
        t_meshes[meshCount].lodLevel   = t_lodLevel;
      }
      else if (line.starts_with("v ")) {
        const char* ptr    = line.data() + 2;
        const char* ptrEnd = line.data() + line.size();

        ptr = ParseFloat(ptr, ptrEnd, x);
        ptr = ParseFloat(ptr, ptrEnd, y);
        ptr = ParseFloat(ptr, ptrEnd, z);
        t_state.tempMeshes[meshCount].vertices.emplace_back(x, y, z);
      }
      else if (line.starts_with("vt")) {
        const char* ptr    = line.data() + 2;
        const char* ptrEnd = line.data() + line.size();

        ptr = ParseFloat(ptr, ptrEnd, x);
        ptr = ParseFloat(ptr, ptrEnd, y);
        t_state.tempMeshes[meshCount].texCoords.emplace_back(x, 1.0 - y);

        uvMin = glm::min(uvMin, x);
        uvMax = glm::max(uvMax, 1.0f - y);
      }
      else if (line.starts_with("vn")) {
        const char* ptr    = line.data() + 2;
        const char* ptrEnd = line.data() + line.size();

        ptr = ParseFloat(ptr, ptrEnd, x);
        ptr = ParseFloat(ptr, ptrEnd, y);
        ptr = ParseFloat(ptr, ptrEnd, z);
        t_state.tempMeshes[meshCount].normals.emplace_back(x, y, z);
      }
      else if (line.starts_with("usemtl")) {
        t_meshes[meshCount].material = std::string(line.substr(7));

        glm::vec2 uvRange = uvMax - uvMin;
        bool      isTiled = (uvRange.x > 1.0f || uvRange.y > 1.0f);

        t_state.materials[t_lodLevel][mtlCount].isTiled = isTiled;
        // TODO: dont assume materials are used in the same order as definitions

        // reset uv count
        uvMax = glm::vec2(FLT_MAX);
        uvMin = glm::vec2(FLT_MIN);

        mtlCount++;
      }
      else if (line.starts_with("mtllib")) {
        t_state.mtlFileName = std::string(line.substr(7));
      }
      else if (line.starts_with("f ")) {
        const char* ptr    = line.data() + 2;
        const char* ptrEnd = line.data() + line.size();

        unsigned int              faceSize = 0;
        std::array<glm::uvec3, 4> face;

        while (ptr < ptrEnd && faceSize < 4) {
          glm::uvec3 u{};
          u.x = std::strtoul(ptr, const_cast<char**>(&ptr), 10);
          if (*ptr == '/') {
            ++ptr;
            u.y = std::strtoul(ptr, const_cast<char**>(&ptr), 10);
          }
          if (*ptr == '/') {
            ++ptr;
            u.z = std::strtoul(ptr, const_cast<char**>(&ptr), 10);
          }

          // Track max values for this mesh
          maxIndexSeen = glm::max(maxIndexSeen, u);

          // Decrement for 0-based indices
          --u;
          u -= indexOffset;

          face[faceSize++] = u;

          // Skip spaces
          while (ptr < ptrEnd && *ptr == ' ') {
            ++ptr;
          }
        }

        if (faceSize == 3) {
          t_state.tempMeshes[meshCount].faceIndices.insert(
            t_state.tempMeshes[meshCount].faceIndices.end(),
            face.begin(),
            face.begin() + 3);
        }
        // triangulate
        else if (faceSize == 4) {
          // Split along v0 → v2 diagonal
          t_state.tempMeshes[meshCount].faceIndices.push_back(face[0]);
          t_state.tempMeshes[meshCount].faceIndices.push_back(face[1]);
          t_state.tempMeshes[meshCount].faceIndices.push_back(face[2]);

          t_state.tempMeshes[meshCount].faceIndices.push_back(face[0]);
          t_state.tempMeshes[meshCount].faceIndices.push_back(face[2]);
          t_state.tempMeshes[meshCount].faceIndices.push_back(face[3]);
        }
      }
    }
  }

  /*!
   * @brief Pointer walks through the mtl file and stores texture info in LoaderState
   * @param t_state Internal state data to store texture info
   * @param t_buffer String buffer of current file
   * @param t_lodLevel
   */
  void ParseMtl(LoaderState& t_state, const std::string& t_buffer, const unsigned int& t_lodLevel) {
    // --- First pass: estimate number of materials ---
    size_t      materialCount = 0;
    const char* ptr           = t_buffer.data();
    const char* end           = ptr + t_buffer.size();

    while (ptr < end) {
      // skip leading whitespace
      while (ptr < end && (*ptr == ' ' || *ptr == '\t')) {
        ++ptr;
      }
      const char* lineStart = ptr;
      while (ptr < end && *ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r') {
        ++ptr;
      }
      std::string_view prefix(lineStart, ptr - lineStart);

      if (prefix == "newmtl") {
        materialCount++;
      }

      // skip rest of line
      while (ptr < end && *ptr != '\n') {
        ++ptr;
      }
      if (ptr < end && *ptr == '\n') {
        ++ptr;
      }
    }

    // pre-allocate vector
    t_state.materials[t_lodLevel].reserve(t_state.materials.size() + materialCount);

    // --- Second pass: actual parsing ---
    ptr          = t_buffer.data();
    int mtlCount = -1;

    while (ptr < end) {
      // Skip leading whitespace
      while (ptr < end && (*ptr == ' ' || *ptr == '\t')) {
        ++ptr;
      }

      const char* lineStart = ptr;
      while (ptr < end && *ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r') {
        ++ptr;
      }
      std::string_view prefix(lineStart, ptr - lineStart);

      if (prefix.empty() || prefix[0] == '#') {
        while (ptr < end && *ptr != '\n') {
          ++ptr;
        }
        if (ptr < end && *ptr == '\n') {
          ++ptr;
        }
        continue;
      }

      while (ptr < end && (*ptr == ' ' || *ptr == '\t')) {
        ++ptr;
      }
      const char* valueStart = ptr;
      while (ptr < end && *ptr != ' ' && *ptr != '\t' && *ptr != '\n' && *ptr != '\r') {
        ++ptr;
      }
      std::string_view value(valueStart, ptr - valueStart);

      if (prefix == "newmtl") {
        t_state.materials[t_lodLevel].emplace_back(std::string(value));
        mtlCount = static_cast<int>(t_state.materials[t_lodLevel].size() - 1);
      }
      else if (mtlCount >= 0) {
        if (prefix == "map_Kd") {
          t_state.materials[t_lodLevel][mtlCount].diffuseName.emplace_back(value);
        }
        else if (prefix == "map_Ks" || prefix == "map_Ns") {
          t_state.materials[t_lodLevel][mtlCount].specularName.emplace_back(value);
        }
        else if (prefix == "map_Bump" || prefix == "bump") {
          t_state.materials[t_lodLevel][mtlCount].normalName.emplace_back(value);
        }
        else if (prefix == "disp") {
          t_state.materials[t_lodLevel][mtlCount].heightName.emplace_back(value);
        }
      }

      while (ptr < end && *ptr != '\n') {
        ++ptr;
      }
      if (ptr < end && *ptr == '\n') {
        ++ptr;
      }
    }
  }

  /*!
   * @brief Returns the proper mesh container given a lod level or not
   * @param t_state Internal state data to grab mesh container from
   * @param t_lodLevel Specified lod level, if any
   * @return Reference to the appropriate container
   */
  std::vector<Mesh>& GetMeshContainer(LoaderState& t_state, const unsigned int t_lodLevel) {
    return t_state.meshes[t_lodLevel];
  }

  /*!
   * @brief Computes the tangent and bitangent vectors for a triangle face using its vertex positions, texture coordinates, and normal.
   * @param t_v1 First vertex of the triangle.
   * @param t_v2 Second vertex of the triangle.
   * @param t_v3 Third vertex of the triangle.
   * @return Pair of normalized tangent and bitangent vectors.
   */
  std::pair<glm::vec3, glm::vec3> GetTangentCoords(const Vertex& t_v1, const Vertex& t_v2, const Vertex& t_v3) {
    // flat-shaded tangent
    //clockwise
    const glm::vec3 edge1    = t_v2.position - t_v1.position;
    const glm::vec3 edge2    = t_v3.position - t_v1.position;
    const glm::vec2 deltaUv1 = t_v2.texCoords - t_v1.texCoords;
    const glm::vec2 deltaUv2 = t_v3.texCoords - t_v1.texCoords;

    const float f = 1.0f / (deltaUv1.x * deltaUv2.y - deltaUv2.x * deltaUv1.y);

    glm::vec3 tangent   = f * (edge1 * deltaUv2.y - edge2 * deltaUv1.y);
    glm::vec3 bitangent = f * (edge2 * deltaUv1.x - edge1 * deltaUv2.x);

    // We should be storing the tangent on all three vertices of the face

    return {tangent, bitangent};
  }

  /*!
   * @brief Converts polygonal face data from the temporary loader state into fully defined triangles, populating each mesh with vertices and indices.
   * @param t_state Internal state data used to grab vertex, normal, and texture coordinate data from temporary containers.
   * @param t_meshes List of meshes to populate with triangulated vertex and index data.
   */
  void ConstructVertices(LoaderState& t_state, std::vector<Mesh>& t_meshes) {
    for (unsigned int a = 0; a < t_meshes.size(); ++a) {
      for (unsigned int i = 0; i < t_state.tempMeshes[a].faceIndices.size(); ++i) {
        // fetch each triangle from our face indices
        t_meshes[a].vertices.emplace_back(
          t_state.tempMeshes[a].vertices[t_state.tempMeshes[a].faceIndices[i].x],
          t_state.tempMeshes[a].normals[t_state.tempMeshes[a].faceIndices[i].z],
          t_state.tempMeshes[a].texCoords[t_state.tempMeshes[a].faceIndices[i].y]);
        // store the indice of each triangle we create
        t_meshes[a].indices.emplace_back(i);
      }
    }
  }

  /*!
   * @brief Calculates per-vertex tangent and bitangent vectors for all meshes, used in tangent-space normal mapping. Accumulates contributions from each face and normalizes the results.
   * @param t_meshes List of meshes to process and update with tangent space data.
   */
  void CalcTangentSpace(std::vector<Mesh>& t_meshes) {
    for (auto& mesh : t_meshes) {
      std::vector bitangents(mesh.vertices.size(), glm::vec3(0.0f));

      // accumulate
      for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        Vertex& v0 = mesh.vertices[mesh.indices[i]];
        Vertex& v1 = mesh.vertices[mesh.indices[i + 1]];
        Vertex& v2 = mesh.vertices[mesh.indices[i + 2]];

        const auto&& [tangent, bitangent] = GetTangentCoords(v0, v1, v2);

        const float lenT = glm::length(tangent);
        const float lenB = glm::length(bitangent);

        // skip degenerate tri
        if (!std::isfinite(lenT) || lenT < 1e-10f || !std::isfinite(lenB) || lenB < 1e-10f) {
          continue;
        }

        const float area = glm::length(glm::cross(v1.position - v0.position, v2.position - v0.position)) * 0.5f;

        v0.tangent += glm::vec4(tangent, 0.0f) * area;
        bitangents[mesh.indices[i]] += bitangent * area;
        v1.tangent += glm::vec4(tangent, 0.0f) * area;
        bitangents[mesh.indices[i + 1]] += bitangent * area;
        v2.tangent += glm::vec4(tangent, 0.0f) * area;
        bitangents[mesh.indices[i + 2]] += bitangent * area;
      }

      for (size_t i = 0; i < mesh.vertices.size(); ++i) {
        auto& v = mesh.vertices[i];

        glm::vec3 t(1, 0, 0);

        if (glm::length(v.tangent) > 1e-10f) {
          // Gram-Schmidt orthogonalize
          t = glm::normalize(glm::vec3(v.tangent) - v.normal * glm::dot(v.normal, glm::vec3(v.tangent)));
        }

        // Handedness from unnormalized bitangent
        const float handedness = (glm::dot(glm::cross(v.normal, t), bitangents[i]) < 0.0f) ? -1.0f : 1.0f;

        v.tangent = glm::vec4(t, handedness);
      }
    }
  }

  /*!
   * @brief Deduplicates vertices with identical pos, uv and normal data
   * @param t_meshes List of meshes to deduplicate
   */
  void JoinIdenticalVertices(std::vector<Mesh>& t_meshes) {
    for (auto& mesh : t_meshes) {
      if (mesh.vertices.empty()) {
        continue;
      }

      std::unordered_map<Vertex, unsigned int, VertexHasher, VertexEqual> uniqueVertices;
      uniqueVertices.reserve(mesh.vertices.size());

      std::vector<unsigned int> newIndices;
      newIndices.reserve(mesh.indices.size());

      std::vector<Vertex> newVertices;
      newVertices.reserve(mesh.indices.size());

      for (const auto idx : mesh.indices) {
        const Vertex& v  = mesh.vertices[idx];
        auto          it = uniqueVertices.find(v);

        if (it == uniqueVertices.end()) {
          const unsigned int newIndex = static_cast<unsigned int>(newVertices.size());
          uniqueVertices.emplace(v, newIndex);
          newVertices.push_back(v);
          newIndices.push_back(newIndex);
        }
        else {
          newIndices.push_back(it->second);
        }
      }

      mesh.indices.swap(newIndices);
      mesh.vertices.swap(newVertices);

      //const size_t              n = mesh.vertices.size();
      //std::vector<unsigned int> indexMap(n);
      //std::iota(indexMap.begin(), indexMap.end(), 0);
      //
      //// Sort indices by vertex value
      //std::ranges::sort(
      //  indexMap,
      //  [&] (const unsigned int t_a, const unsigned int t_b)
      //  {
      //    return mesh.vertices[t_a] < mesh.vertices[t_b]; // requires operator<
      //  });
      //
      //std::vector<Vertex> newVertices;
      //newVertices.reserve(n);
      //std::vector<unsigned int> remap(n);
      //
      //// Deduplicate vertices while tracking new index mapping
      //unsigned int nextIndex = 0;
      //newVertices.emplace_back(mesh.vertices[indexMap[0]]);
      //remap[indexMap[0]] = nextIndex;
      //
      //for (size_t i = 1; i < n; ++i) {
      //  const Vertex& curr = mesh.vertices[indexMap[i]];
      //  const Vertex& prev = mesh.vertices[indexMap[i - 1]];
      //
      //  if (curr != prev) {
      //    ++nextIndex;
      //    newVertices.emplace_back(curr);
      //  }
      //  remap[indexMap[i]] = nextIndex;
      //}
      //
      //for (auto& idx : mesh.indices) {
      //  idx = remap[idx];
      //}
      //
      //// Resize newVertices to actual number of unique vertices
      //newVertices.resize(nextIndex + 1);
      //
      //// Remap mesh.indices to the deduplicated set
      //mesh.vertices.swap(newVertices);
    }
  }

  void CombineMeshes(LoaderState& t_state) {
    auto initFrom = [&] (const Mesh& t_src, Mesh& t_dst)
    {
      t_dst.name       = t_src.name;
      t_dst.material   = t_src.material;
      t_dst.meshNumber = t_src.meshNumber;
      t_dst.lodLevel   = t_src.lodLevel;
    };

    for (auto& lod : t_state.meshes | std::views::values) {
      unsigned int lodLevel   = lod[0].lodLevel;
      size_t       totalVerts = 0, totalIndices = 0;
      unsigned int baseVertex = 0;

      t_state.combinedMeshes.emplace_back();

      initFrom(lod[0], t_state.combinedMeshes[lodLevel]);

      // check size for reserve
      for (auto& mesh : lod) {
        totalVerts += mesh.vertices.size();
        totalIndices += mesh.indices.size();
      }

      t_state.combinedMeshes[lodLevel].vertices.reserve(totalVerts);
      t_state.combinedMeshes[lodLevel].indices.reserve(totalIndices);

      // combine meshes
      for (auto& mesh : lod) {
        // append indices with offset
        for (const auto idx : mesh.indices) {
          t_state.combinedMeshes[lodLevel].indices.push_back(idx + baseVertex);
        }

        t_state.combinedMeshes[lodLevel].vertices.insert(
          t_state.combinedMeshes[lodLevel].vertices.end(),
          mesh.vertices.begin(),
          mesh.vertices.end());

        baseVertex += static_cast<unsigned int>(mesh.vertices.size());
      }
    }
  }
}
