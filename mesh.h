#pragma once

#include <vector>
#include <string>
#include <glm/glm.hpp>
#include <glad/glad.h>
#include "shader.h"

enum TextureType {
	TEXTURE_DIFFUSE,
	TEXTURE_SPECULAR
};

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoords;

	Vertex() = default;
	Vertex(float px, float py, float pz, float nx = 0, float ny = 0, float nz = 0, float u = 0, float v = 0) : position( px, py, pz ), normal( nx, ny, nz ), texCoords( u, v ) {};
};

struct Texture {
	unsigned int id;
	TextureType type;

	Texture() = default;
	Texture(unsigned int i, TextureType t) : id(i), type(t) {};
};

//Mesh owns its VAO / VBO / EBO.  Copying is disabled to avoid double frees or unecessary resource holding
class Mesh {
public:
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<Texture> textures;

	Mesh(const Mesh &) = delete;
	Mesh &operator=(Mesh const &) = delete;

	//Don't forget to call std::move when calling Mesh constructor!  Avoid copies
	Mesh(std::vector<Vertex>&& verts, std::vector<unsigned int>&& inds, std::vector<Texture>&& texts) :
		vertices(std::move(verts)),
		indices(std::move(inds)),
		textures(std::move(texts)) {
		setupMesh();
	}

	Mesh(Mesh &&other) noexcept :
		vertices(std::move(other.vertices)),
		indices(std::move(other.indices)),
		textures(std::move(other.textures)) {
		VAO = other.VAO;
		VBO = other.VBO;
		EBO = other.EBO;

		other.EBO = 0;
		other.VBO = 0;
		other.VAO = 0;
	}

	Mesh &operator=(Mesh &&other) noexcept {
		if (this != &other) {
			glDeleteBuffers(1, &EBO);
			glDeleteBuffers(1, &VBO);
			glDeleteVertexArrays(1, &VAO);

			vertices = std::move(other.vertices);
			indices = std::move(other.indices);
			textures = std::move(other.textures);
			VAO = other.VAO;
			VBO = other.VBO;
			EBO = other.EBO;

			other.EBO = 0;
			other.VBO = 0;
			other.VAO = 0;
		}
		return *this;
	}

	//Free GPU resources on mesh destruction
	~Mesh(){
		glDeleteBuffers(1, &EBO);
		glDeleteBuffers(1, &VBO);
		glDeleteVertexArrays(1, &VAO);
	}

	//Reset to default VAO & active texture handled by model.h
	void draw(Shader& shader) const{
		if (VAO == 0)
			return;
		
		unsigned int diffuseNr = 1;
		unsigned int specularNr = 1;

		shader.use();

		for (unsigned int i = 0; i < textures.size(); i++) {
			glActiveTexture(GL_TEXTURE0 + i);
			std::string name;
			TextureType type = textures[i].type;

			if (type == TEXTURE_DIFFUSE)
				name = "texture_diffuse" + std::to_string(diffuseNr++);
			else if (type == TEXTURE_SPECULAR)
				name = "texture_specular" + std::to_string(specularNr++);

			shader.setInt("material." + name, i);
			glBindTexture(GL_TEXTURE_2D, textures[i].id);

		}
		glActiveTexture(GL_TEXTURE0);

		//Draw
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
	}

private:
	unsigned int VAO = 0, VBO = 0, EBO = 0;
	
	void setupMesh() {
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

		//textures
		glEnableVertexAttribArray(2);
		glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));

		//reset VAO
		glBindVertexArray(0);
	}
};