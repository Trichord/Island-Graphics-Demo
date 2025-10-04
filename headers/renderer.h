#pragma once

#include <map>
#include <unordered_map>
#include <glm/glm.hpp>

#include "scene.h"
#include "shader.h"
#include "shader_types.h"
#include "renderable.h"
#include "shader_manager.h"

class Renderer {
 public:
  Renderer() = delete;
  Renderer(ShaderManager& shaderManager);
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&& other) noexcept;
  Renderer& operator=(Renderer&& other) noexcept;

  void renderScene(Scene& scene);
  void updateUBO();

 private:
  ShaderManager& shaderManager;

  void setupUBOs(const glm::mat4& viewMatrix,
                 const glm::mat4& projectionMatrix);
  void uploadUBOData();
};
