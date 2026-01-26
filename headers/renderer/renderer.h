#pragma once

#include <unordered_map>

#include "config_window.h"
#include "gl_state.h"
#include "glad/glad.h"
#include "glm/glm.hpp"
#include "scene.h"
#include "shader.h"
#include "shader_types.h"
#include "texture_manager.h"

class Renderer {
 public:
  Renderer() = delete;
  Renderer(const ConfigWindow::GraphicsSettings& graphicsSettings);
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&& other) noexcept;
  Renderer& operator=(Renderer&& other) = delete;

  void renderScene(Scene& scene);
  void updateUBO(Scene& scene);
  TextureManager& getTextureManager();

 private:
  // Packed UBO data holds everything we want to share with all shaders
  struct PackedUBOData {
    glm::mat4 viewMatrix;
    glm::mat4 viewProjectionMatrix;
    glm::vec4 cameraPosition;
  };

  GLuint UBO{0};
  PackedUBOData uboData;
  std::unordered_map<ShaderType, Shader> shaders;
  const ConfigWindow::GraphicsSettings graphicsSettings;
  TextureManager textureManager;
  GLState glState;

  void setupUBO();
  void loadShaders();
  void setupTextureManager();
  void renderPass(Scene& scene);
  void renderPrePass(Scene& scene);
  void setupShader(Shader& shader);
  void resetVisibility(Scene& scene);
  void renderSkybox(Scene& scene);
  void updateShaderUniforms(Scene& scene);
  void bindTextures(const Material& materials);
  void cullRenderables(Scene& scene, bool prePass);
  void cullInstanced(RenderableObject& renderable, const glm::vec3& cameraPos,
                     const std::array<glm::vec4, 6>& frustumPlanes, bool prePass);
  void cullModel(RenderableObject& renderable,
                 const std::array<glm::vec4, 6>& frustumPlanes, bool prePass);
  void cullTerrain(RenderableObject& renderable,
                        const std::array<glm::vec4, 6>& frustumPlanes);
};
