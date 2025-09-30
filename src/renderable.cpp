#include "../headers/renderable.h"

const Model& Renderable::getModel() const {
	return model;
}

const Transform& Renderable::getTransform() const {
	return transform;
}