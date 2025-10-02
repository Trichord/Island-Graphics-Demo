#pragma once

#include "shader_types.h"
#include "renderable.h"
#include "camera.h"
#include <vector>
#include <map>

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
	std::map<ShaderType, std::vector<RenderableObject>> sceneObjects;
	virtual void loadModels() = 0;
};