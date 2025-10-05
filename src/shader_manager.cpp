#include "../headers/shader_manager.h"
#include "../headers/shader.h"
#include <iostream>

ShaderManager::ShaderManager() {
  // Load shaders
  std::cout << "Loading Shaders...";
  loadShaders();
  std::cout << " Done" << std::endl;

  // Setup shaders
  setupPBRShader(shaders.at(ShaderType::POINT_PBR));
  setupPBRShader(shaders.at(ShaderType::DIRECTIONAL_PBR));
  setupReflectiveShader(shaders.at(ShaderType::REFLECTIVE));
  setupLightSourceShader(shaders.at(ShaderType::LIGHT_SOURCE));
  setupSkyboxShader(shaders.at(ShaderType::SKYBOX));
}

ShaderManager::ShaderManager(ShaderManager&& other) noexcept {
  shaders = std::move(other.shaders);
}

ShaderManager& ShaderManager::operator=(ShaderManager&& other) noexcept {
  if (this != &other) {
    shaders.clear();
    shaders = std::move(other.shaders);
  }
  return *this;
}

ShaderManager::~ShaderManager() { shaders.clear(); }

std::unordered_map<ShaderType, Shader>& ShaderManager::getShaders(){
  return shaders;
}

void ShaderManager::loadShaders() {
  shaders.emplace(ShaderType::POINT_PBR, "./shaders/point_PBR.vert",
                  "./shaders/point_PBR.frag");
  shaders.emplace(ShaderType::DIRECTIONAL_PBR, "./shaders/directional_PBR.vert",
                  "./shaders/directional_PBR.frag");
  shaders.emplace(ShaderType::REFLECTIVE, "./shaders/reflective.vert",
                  "./shaders/reflective.frag");
  shaders.emplace(ShaderType::LIGHT_SOURCE, "./shaders/light_source.vert",
                  "./shaders/light_source.frag");
  shaders.emplace(ShaderType::SKYBOX, "./shaders/skybox.vert",
                  "./shaders/skybox.frag");
}

void ShaderManager::setupPBRShader(Shader& shader) {
  shader.use();
  shader.setUniformBlock("matrices", 0);
  shader.setInt("albedoMap", 0);
  shader.setInt("occlusionRoughMetalMap", 1);
  shader.setInt("normalMap", 2);
}

void ShaderManager::setupReflectiveShader(Shader& shader) {
  shader.use();
  shader.setUniformBlock("matrices", 0);
  shader.setInt("cubemap", 0);
}

void ShaderManager::setupLightSourceShader(Shader& shader) {
  shader.use();
  shader.setUniformBlock("matrices", 0);
}
void ShaderManager::setupSkyboxShader(Shader& shader) {
  shader.use();
  shader.setInt("skybox", 0);
}