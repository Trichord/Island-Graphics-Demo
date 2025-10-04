#include "../headers/shader_manager.h"
#include "../headers/shader.h"

ShaderManager::ShaderManager() { 
	loadShaders(); 
setupPBRShader();

}
ShaderManager::ShaderManager(ShaderManager&& other) noexcept {}
ShaderManager& ShaderManager::operator=(ShaderManager&& other) noexcept {}
ShaderManager::~ShaderManager() {}

const std::unordered_map<ShaderType, Shader>& ShaderManager::getShaders() {
  return shaders;
}

void ShaderManager::loadShaders() {}
void ShaderManager::setupPBRShader() {}
void ShaderManager::setupDirectionalPBRShader() {}
void ShaderManager::setupReflectiveShader() {}
void ShaderManager::setupLightSourceShader() {}
void ShaderManager::setupSkyboxShader() {}