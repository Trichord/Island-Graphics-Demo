#include <glad/glad.h>
#include "../headers/renderer.h"
#include "../headers/renderable.h"
#include "../headers/scene.h"
#include "../headers/shader_types.h"
#include "../headers/shader.h"
#include <unordered_map>
#include <map>
#include <glm/glm.hpp>

void Renderer::renderScene(Scene& scene){}
void updateUBO() {}

void setupUBOs(const glm::mat4& viewMatrix, const glm::mat4& projectionMatrix){}
void uploadUBOData(){}