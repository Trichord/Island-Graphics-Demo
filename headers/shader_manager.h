#pragma once

#include "shader_types.h"
#include "shader.h"
#include <unordered_map>

class ShaderManager {
 public:
  ShaderManager();
  ShaderManager(const ShaderManager&) = delete;
  ShaderManager& operator=(const ShaderManager&) = delete;
  ShaderManager(ShaderManager&& other) = delete;
  ShaderManager& operator=(ShaderManager&& other) = delete;
  ~ShaderManager();

 std::unordered_map<ShaderType, Shader>& getShaders();

 private:
  std::unordered_map<ShaderType, Shader> shaders;
  void loadShaders();
  void setupPBRShader(Shader& shader);
  void setupReflectiveShader(Shader& shader);
  void setupLightSourceShader(Shader& shader);
  void setupSkyboxShader(Shader& shader);
};