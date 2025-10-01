#pragma once

#include "shader_types.h"
#include "renderable.h"
#include <vector>
#include <map>

class Scene {
public:
	Scene() = default;
	Scene(Scene&& other) noexcept;
	Scene& operator=(Scene&& other) noexcept;
	Scene(const Scene&) = delete;
	Scene& operator=(const Scene&) = delete;

	const std::map<ShaderType, std::vector<RenderableObject>>& getSceneObjects() const;
private:
	virtual void loadModels() = 0;
};