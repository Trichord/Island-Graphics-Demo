#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cstddef>
#include <iostream>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "shader.h"

//Primitive owns its VAO / VBO / EBO.  Copying is disabled to avoid double frees or unecessary resource holding
class Primitive{
public:
	struct Vertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec4 tangent;
		glm::vec2 texCoords;

		Vertex() = default;
		Vertex(float px, float py, float pz, float nx, float ny, float nz, float tx, float ty, float tz, float tw, float u = 0, float v = 0) :
			position(px, py, pz), normal(nx, ny, nz), tangent(tx, ty, tz, tw), texCoords(u, v) {
		};
	};

	std::vector<Vertex> vertices;
	std::vector<std::uint32_t> indices;

	//Don't forget to call std::move when calling Primitive constructor!  Avoid copies
	Primitive(std::vector<Vertex>&& verts, std::vector<std::uint32_t>&& inds) :
		vertices(std::move(verts)),
		indices(std::move(inds)) {
		setupPrimitive();
	}

	Primitive(Primitive&&other) noexcept :
		vertices(std::move(other.vertices)),
		indices(std::move(other.indices)) {
		VAO = other.VAO;
		VBO = other.VBO;
		EBO = other.EBO;

		other.EBO = 0;
		other.VBO = 0;
		other.VAO = 0;
	}

	Primitive&operator=(Primitive&&other) noexcept {
		if (this != &other) {
			glDeleteBuffers(1, &EBO);
			glDeleteBuffers(1, &VBO);
			glDeleteVertexArrays(1, &VAO);

			vertices = std::move(other.vertices);
			indices = std::move(other.indices);
			VAO = other.VAO;
			VBO = other.VBO;
			EBO = other.EBO;

			other.EBO = 0;
			other.VBO = 0;
			other.VAO = 0;
		}
		return *this;
	}

	Primitive(const Primitive&) = delete;
	Primitive& operator=(Primitive const&) = delete;

	//Free GPU resources on mesh destruction
	~Primitive(){
		glDeleteBuffers(1, &EBO);
		glDeleteBuffers(1, &VBO);
		glDeleteVertexArrays(1, &VAO);
	}

	//Reset to default VAO & active texture handled by model.h
	void draw() const{
		if (VAO == 0)
			return;

		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
	}

private:
	unsigned int VAO = 0, VBO = 0, EBO = 0;
	
	void setupPrimitive() {
		if (vertices.size() == 0 || indices.size() == 0) {
			std::cout << "Error: no vertices or indices in mesh" << std::endl;
			return;
		}

		glGenVertexArrays(1, &VAO);
		glBindVertexArray(VAO);

		glGenBuffers(1, &VBO);
		glBindBuffer(GL_ARRAY_BUFFER, VBO);
		glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), &vertices[0], GL_STATIC_DRAW);

		glGenBuffers(1, &EBO);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);

		//vertices
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);

		//normals
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));

		//tangent
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));

		//textures
		glEnableVertexAttribArray(3);
		glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

		//reset VAO
		glBindVertexArray(0);
	}
};