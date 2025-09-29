#include "../headers/transform.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp> 

Transform::Transform(glm::vec3 pos, float degrees, glm::vec3 axis, glm::vec3 sc) : position(pos), scale(sc) {
	glm::vec3 normalizedAxis = glm::normalize(axis);
	quaternion = glm::angleAxis(glm::radians(degrees), normalizedAxis);
	rebuildModelMatrix();
	isDirty = false;
};

void Transform::setAbsolutePosition(glm::vec3 absolutePosition) {
	position = absolutePosition;
	isDirty = true;
}

void Transform::setAbsoluteRotation(glm::vec3 absoluteRotation) {
	quaternion = glm::quat(glm::radians(absoluteRotation));
	isDirty = true;
}

void Transform::setAbsoluteScale(glm::vec3 absoluteScale) {
	scale = absoluteScale;
	isDirty = true;
}

void Transform::setRelativePosition(glm::vec3 relativePosition) {
	position += relativePosition;
	isDirty = true;
}

void Transform::setRelativeRotation(glm::vec3 relativeRotation) {
	glm::quat updatedRotation = quaternion * glm::quat(glm::radians(relativeRotation));
	quaternion = glm::normalize(updatedRotation);
	isDirty = true;
}

void Transform::setRelativeScale(glm::vec3 relativeScale) {
	scale *= relativeScale;
	isDirty = true;
}

void Transform::rotateAroundAxis(float degrees, glm::vec3 axis) {
	glm::vec3 normalizedAxis = glm::normalize(axis);
	glm::quat updatedRotation = quaternion * glm::angleAxis(glm::radians(degrees), normalizedAxis);
	quaternion = glm::normalize(updatedRotation);
	isDirty = true;
}

const glm::vec3& Transform::getPosition() const {
	return position;
}

const glm::vec3& Transform::getScale() const {
	return scale;
}

const glm::mat4& Transform::getModelMatrix() {
	if (isDirty) {
		rebuildModelMatrix();
		isDirty = false;
	}

	return modelMatrix;
}

const glm::mat4& Transform::getNormalMatrix() {
	if (isDirty) {
		rebuildModelMatrix();
		isDirty = false;
	}

	return normalMatrix;
}

glm::vec3 Transform::getRotation() const {
	return glm::degrees(glm::eulerAngles(quaternion));
}

void Transform::rebuildModelMatrix() {
	modelMatrix = glm::mat4(1.0f);
	modelMatrix = glm::translate(modelMatrix, position);
	modelMatrix *= glm::mat4_cast(quaternion);
	modelMatrix = glm::scale(modelMatrix, scale);
	normalMatrix = glm::transpose(glm::inverse(glm::mat3(modelMatrix)));
}