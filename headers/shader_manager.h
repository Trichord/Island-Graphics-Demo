#pragma once

#include "shader_types.h"
#include "shader.h"
#include <unordered_map>

class ShaderManager {
 public:
  ShaderManager();
  ShaderManager(const ShaderManager&) = delete;
  ShaderManager& operator=(const ShaderManager&) = delete;
  ShaderManager(ShaderManager&& other) noexcept;
  ShaderManager& operator=(ShaderManager&& other) noexcept;
  ~ShaderManager();

  const std::unordered_map<ShaderType, Shader>& getShaders();

 private:
  std::unordered_map<ShaderType, Shader> shaders;
  void loadShaders();
  void setupPBRShader();
  void setupDirectionalPBRShader();
  void setupReflectiveShader();
  void setupLightSourceShader();
  void setupSkyboxShader();
};