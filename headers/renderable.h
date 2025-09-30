#pragma once

#include "transform.h"
#include "shader.h"
#include <utility>

class Renderable {
public:
	virtual void render(Shader& shader) = 0;
	virtual ~Renderable() = default;
};

struct RenderableObject {
	std::reference_wrapper<Renderable> renderable;
	Transform transform;
};