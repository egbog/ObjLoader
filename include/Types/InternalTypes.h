#pragma once

#include <map>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

struct Material;
struct Mesh;
struct Vertex;

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

struct LoaderState
{
  std::string path;
  std::string mtlFileName;

  std::map<unsigned int, File>              lodPaths;
  std::vector<Mesh>                         meshes; // final calculated meshes
  std::map<unsigned int, std::vector<Mesh>> lodMeshes;
  std::vector<Material>                     materials;  // final materials
  std::vector<TempMeshes>                   tempMeshes; // interim storage
};
