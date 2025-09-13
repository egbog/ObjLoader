#include "ObjHelpers.h"

#include "Types/InternalTypes.h"
#include "Types/SceneTypes.h"

#include <fast_float.h>
#include <filesystem>
#include <fstream>
#include <numeric>

#include <glm/geometric.hpp>

std::string ObjHelpers::ReadFileToBuffer(const std::string& t_path) {
  std::ifstream in(t_path, std::ios::binary | std::ios::ate);
  if (!in.is_open()) {
    throw std::runtime_error("Failed to open file: " + t_path);
  }

  const auto  size = in.tellg();
  std::string buffer(size, '\0');
  in.seekg(0, std::ios::beg);
  in.read(buffer.data(), size);

  return buffer;
}

// finds all lod files and corresponding mtl files and stores them
void ObjHelpers::CacheFilePaths(LoaderState& t_state) {
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

  //store base mesh at lod0
  t_state.lodPaths[0] = File{.objPath = t_state.path, .mtlPath = mtlPath.string(), .lodLevel = 0};

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
    int         lodIndex;
    try {
      lodIndex = std::stoi(lodNumStr);
    }
    catch (...) {
      continue; // invalid suffix, skip file
    }

    // Get reference to lod entry in map (created if not exists)
    t_state.lodPaths[lodIndex].lodLevel = lodIndex;

    // Assign paths depending on file type
    if (entryExtension == ".obj") {
      t_state.lodPaths[lodIndex].objPath = entry.path().string();
    }
    else if (entryExtension == ".mtl") {
      t_state.lodPaths[lodIndex].mtlPath = entry.path().string();
    }
  }
}

//void ObjHelpers::ParseObj(LoaderState& t_state, const std::string& t_buffer, unsigned int t_lodLevel) {
//  std::istringstream inFile(t_buffer);
//
//  if (!inFile.good()) {
//    throw std::runtime_error("Failed to open OBJ file");
//  }
//
//  //-------------------------------------------------------------------------------------------------------------------
//  // use file size as estimate for vector reserves
//  //std::filesystem::path p(t_path);
//  //auto                  fileSize = file_size(p); // size in bytes
//
//  // average each line is ~34 bytes (plus lines like #comments and s 1, mtllib, o etc.)
//  // filesize / 34 = ~linecount
//  //const auto linecount = fileSize / 34;
//  // file consists of pos, tex, norm and faces so divide linecount by 4
//  //m_vertices.reserve(linecount / 4); // each line is ~30 bytes
//  //m_texCoords.reserve(linecount / 4); // ~20 bytes
//  //m_normals.reserve(linecount / 4);   // ~25 bytes
//  //-------------------------------------------------------------------------------------------------------------------
//
//  std::string        line;
//  std::string        meshName;
//  std::istringstream ss;
//
//  std::vector<Mesh>& meshes = GetMeshContainer(t_state, t_lodLevel);
//
//  int meshCount = -1;
//
//  // temp vectors
//  float x, y, z;
//
//  while (std::getline(inFile, line)) {
//    if (line.empty() || line[0] == '#') // Skip empty lines and comments
//    {
//      continue;
//    }
//
//    ss.clear();
//    ss.str(line);
//
//    std::string prefix;
//    ss >> prefix;
//
//    if (prefix == "mtllib") {
//      ss >> t_state.mtlFileName;
//    }
//    // name of model
//    else if (prefix == "o") {
//      ss >> meshName;
//      meshCount++;
//
//      // temp storage
//      t_state.tempMeshes.emplace_back();
//      //t_state.tempMeshes[meshCount].vertices.reserve(linecount / 4);
//      //t_state.tempMeshes[meshCount].texCoords.reserve(linecount / 4);
//      //t_state.tempMeshes[meshCount].faceIndices.reserve(linecount);
//
//      meshes.emplace_back();
//      //meshes[meshCount].vertices.reserve(linecount);
//      //meshes[meshCount].indices.reserve(linecount);
//      meshes[meshCount].name       = meshName;
//      meshes[meshCount].meshNumber = meshCount;
//      meshes[meshCount].lodLevel   = t_lodLevel;
//    }
//    else if (prefix == "v") {
//      ss >> x >> y >> z;
//
//      t_state.tempMeshes[meshCount].vertices.emplace_back(x, y, z);
//    }
//    else if (prefix == "vt") {
//      ss >> x >> y;
//
//      // flip uv y axis
//      //if(tvec2.y < 0) // _________________________________________________________________________________________________________________
//      //tvec2.y = 1.0f - tvec2.y;
//
//      t_state.tempMeshes[meshCount].texCoords.emplace_back(x, y);
//    }
//    else if (prefix == "vn") {
//      ss >> x >> y >> z;
//      //tvec3 = 1.0f - tvec3; //invert normals - not necessary
//      t_state.tempMeshes[meshCount].normals.emplace_back(x, y, z);
//    }
//    else if (prefix == "usemtl") {
//      ss >> meshes[meshCount].material;
//    }
//    else if (prefix == "f") {
//      // each face is an index of a tri. we can use these tris to calc tangent
//      for (int i = 0; i < 3; i++) {
//        char       c;
//        glm::uvec3 u;
//        ss >> u.x >> c >> u.y >> c >> u.z;
//
//        --u; // indices start at 1 in obj files. opengl starts at 0 so we subtract 1
//
//        t_state.tempMeshes[meshCount].faceIndices.emplace_back(u);
//      }
//    }
//  }
//  //inFile.close();
//}

void ObjHelpers::ParseObj(LoaderState& t_state, std::vector<Mesh>& t_meshes, const std::string& t_buffer, const unsigned int t_lodLevel) {
  int                meshCount = -1;

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
      indexOffset = maxIndexSeen;  // carry forward for next mesh

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
      const char* ptr = line.data() + 2;
      auto        r1  = fast_float::from_chars(ptr, line.data() + line.size(), x);
      auto        r2  = fast_float::from_chars(r1.ptr, line.data() + line.size(), y);
      auto        r3  = fast_float::from_chars(r2.ptr, line.data() + line.size(), z);
      t_state.tempMeshes[meshCount].vertices.emplace_back(x, y, z);
    }
    else if (line.starts_with("vt")) {
      const char* ptr = line.data() + 3;
      auto        r1  = fast_float::from_chars(ptr, line.data() + line.size(), x);
      auto        r2  = fast_float::from_chars(r1.ptr, line.data() + line.size(), y);
      t_state.tempMeshes[meshCount].texCoords.emplace_back(x, y);
    }
    else if (line.starts_with("vn")) {
      const char* ptr = line.data() + 3;
      auto        r1  = fast_float::from_chars(ptr, line.data() + line.size(), x);
      auto        r2  = fast_float::from_chars(r1.ptr, line.data() + line.size(), y);
      auto        r3  = fast_float::from_chars(r2.ptr, line.data() + line.size(), z);
      t_state.tempMeshes[meshCount].normals.emplace_back(x, y, z);
    }
    else if (line.starts_with("usemtl")) {
      t_meshes[meshCount].material = std::string(line.substr(7));
    }
    else if (line.starts_with("mtllib")) {
      t_state.mtlFileName = std::string(line.substr(7));
    }
    else if (line.starts_with("f ")) {
      const char* ptr = line.data() + 2;
      for (int i = 0; i < 3; i++) {
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

        --u;

        // Rebase against offset
        u -= indexOffset;

        t_state.tempMeshes[meshCount].faceIndices.emplace_back(u);

        while (*ptr == ' ') {
          ++ptr;
        }
      }
    }
  }
}

//void ObjHelpers::ParseMtl(LoaderState& t_state, const std::string& t_buffer) {
//  // load mtl
//  std::istringstream inFileMtl(t_buffer);
//
//  if (!inFileMtl.good()) {
//    throw std::runtime_error("Failed to open MTL file");
//  }
//
//  int                mtlCount = -1;
//  std::string        line;
//  std::istringstream ss;
//  std::string        prefix;
//
//  while (std::getline(inFileMtl, line)) {
//    ss.clear();
//    ss.str(line);
//    prefix = "";
//    ss >> prefix;
//
//    if (prefix == "newmtl") {
//      std::string mtlname;
//      ss >> mtlname;
//      // create new material
//      t_state.materials.emplace_back(mtlname);
//      mtlCount = static_cast<int>(t_state.materials.size() - 1);
//    }
//    else if (prefix == "map_Kd" && mtlCount >= 0) // diffuse
//    {
//      std::string t;
//      ss >> t;
//      t_state.materials[mtlCount].diffuseName.emplace_back(t);
//    }
//    else if ((prefix == "map_Ks" || prefix == "map_Ns") && mtlCount >= 0)
//    // map_Ns specular strength, map_Ks specular color
//    {
//      std::string t;
//      ss >> t;
//      t_state.materials[mtlCount].specularName.emplace_back(t);
//    }
//    else if (prefix == "map_Bump" && mtlCount >= 0) // normal
//    {
//      std::string t;
//      ss >> t;
//      t_state.materials[mtlCount].normalName.emplace_back(t);
//    }
//  }
//
//  //inFileMtl.close();
//}

void ObjHelpers::ParseMtl(LoaderState& t_state, const std::string& t_buffer) {
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
  t_state.materials.reserve(t_state.materials.size() + materialCount);

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
      t_state.materials.emplace_back(std::string(value));
      mtlCount = static_cast<int>(t_state.materials.size() - 1);
    }
    else if (mtlCount >= 0) {
      if (prefix == "map_Kd") {
        t_state.materials[mtlCount].diffuseName.emplace_back(value);
      }
      else if (prefix == "map_Ks" || prefix == "map_Ns") {
        t_state.materials[mtlCount].specularName.emplace_back(value);
      }
      else if (prefix == "map_Bump") {
        t_state.materials[mtlCount].normalName.emplace_back(value);
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

std::vector<Mesh>& ObjHelpers::GetMeshContainer(LoaderState& t_state, const unsigned int t_lodLevel) {
  if (t_lodLevel == 0) {
    return t_state.meshes;
  }

  return t_state.lodMeshes[t_lodLevel];
}

std::pair<glm::vec3, glm::vec3> ObjHelpers::GetTangentCoords(const std::span<Vertex>& t_v) {
  glm::vec3 tangent1;
  // flat-shaded tangent
  const glm::vec3 normal = normalize(t_v[0].normal);

  //clockwise
  const glm::vec3 edge1    = t_v[1].position - t_v[0].position;
  const glm::vec3 edge2    = t_v[2].position - t_v[0].position;
  const glm::vec2 deltaUv1 = t_v[1].texCoords - t_v[0].texCoords;
  const glm::vec2 deltaUv2 = t_v[2].texCoords - t_v[0].texCoords;

  const float f = 1.0f / (deltaUv1.x * deltaUv2.y - deltaUv2.x * deltaUv1.y);
  tangent1.x    = f * (deltaUv2.y * edge1.x - deltaUv1.y * edge2.x);
  tangent1.y    = f * (deltaUv2.y * edge1.y - deltaUv1.y * edge2.y);
  tangent1.z    = f * (deltaUv2.y * edge1.z - deltaUv1.y * edge2.z);

  // We should be storing the tangent on all three vertices of the face
  tangent1             = normalize(tangent1);
  glm::vec3 bitangent1 = cross(normal, tangent1);

  return {tangent1, bitangent1};
}

void ObjHelpers::Triangulate(LoaderState& t_state, std::vector<Mesh>& t_meshes, const unsigned int t_lodLevel) {
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

void ObjHelpers::CalcTangentSpace(std::vector<Mesh>& t_meshes) {
  for (auto& mesh : t_meshes) {
    for (size_t i = 0; i < mesh.vertices.size(); i += 3) {
      auto         triSpan              = std::span(mesh.vertices.begin() + i, 3);
      const auto&& [tangent, bitangent] = GetTangentCoords(triSpan);

      // store tangent in each of the three vertices associated with the triangle
      for (size_t j = 0; j < 3; ++j) {
        mesh.vertices[i + j].tangent   = tangent;
        mesh.vertices[i + j].biTangent = bitangent;
      }
    }
  }
}

void ObjHelpers::JoinIdenticalVertices(std::vector<Mesh>& t_meshes) {
  for (auto& mesh : t_meshes) {
    if (mesh.vertices.empty()) {
      continue;
    }

    const size_t              n = mesh.vertices.size();
    std::vector<unsigned int> indexMap(n);
    std::iota(indexMap.begin(), indexMap.end(), 0);

    // Sort indices by vertex value
    std::ranges::sort(
      indexMap,
      [&](const unsigned int t_a, const unsigned int t_b)
      {
        return mesh.vertices[t_a] < mesh.vertices[t_b]; // requires operator<
      });

    std::vector<Vertex>       newVertices(n);
    std::vector<unsigned int> remap(n);

    // Deduplicate vertices while tracking new index mapping
    unsigned int nextIndex = 0;
    newVertices.emplace_back(mesh.vertices[indexMap[0]]);
    remap[indexMap[0]] = nextIndex;

    for (size_t i = 1; i < n; ++i) {
      const Vertex& curr = mesh.vertices[indexMap[i]];
      const Vertex& prev = mesh.vertices[indexMap[i - 1]];

      if (curr != prev) {
        ++nextIndex;
        newVertices[nextIndex] = curr; // assign instead of emplace_back
      }
      remap[indexMap[i]] = nextIndex;
    }

    // Resize newVertices to actual number of unique vertices
    newVertices.resize(nextIndex + 1);

    // Remap mesh.indices to the deduplicated set
    mesh.indices.swap(remap);
    mesh.vertices.swap(newVertices);
  }
}
