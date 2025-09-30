#pragma once

#include "model.h"
#include "transform.h"
#include "shader.h"
#include <utility>

//Ensure model is always an rvalue, it is a non copyable resource
class Renderable {
public:
	Renderable(Model&& m, Transform&& t) : model(std::move(m)), transform(std::move(t)) {};

	const Model& getModel() const;
	const Transform& getTransform() const;

	virtual void render(Shader& shader) = 0;

protected:
	Model model;
	Transform transform;
};
