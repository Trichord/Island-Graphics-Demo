#pragma once

#include "shader_types.h"
#include "renderable.h"
#include "camera.h"
#include <glm/glm.hpp>
#include <vector>
#include <map>
#include <functional>
//-------------------------
// It is important to store the models in a class member variable.  RenderableObject holds a reference to the model
// and it's critical to keep the reference alive after any loading functions finish.  Any concrete implementations should
// declare their own class member variables to hold RenderableObjects.
// 
// e.g.:
// 
// private:
// std::vector<Model> models;
// 
// Then references to those models can be passed to sceneObjects
//-------------------------
class Scene {
public:
	Scene() = default;
	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;
	Scene(Scene&& other) noexcept;
	Scene& operator=(Scene&& other) noexcept;

	const std::map<ShaderType, std::vector<RenderableObject>>& getSceneObjects() const;

protected:
	Camera camera;
	glm::vec3 globalLightDirection;
	std::map<ShaderType, std::vector<RenderableObject>> sceneObjects;
	virtual void loadModels() = 0;
};