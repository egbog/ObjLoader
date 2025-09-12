#pragma once

#include <string>
#include <vector>

struct Material
{
  std::string name;
  std::vector<std::string> diffuseName;
  std::vector<std::string> specularName;
  std::vector<std::string> normalName;
};
