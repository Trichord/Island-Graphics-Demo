#ifndef MESH_H
#define MESH_H

#include <glm/glm.hpp>

enum TextureType {
	TEXTURE_DIFFUSE,
	TEXTURE_SPECULAR
};

struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoords;
};

struct Texture {
	unsigned int id;
	TextureType type;
};

class Mesh {
public:
	std::vector<Vertex> vertices;
	std::vector<unsigned int> indices;
	std::vector<Texture> textures;

	//Don't forget to call std::move when calling Mesh constructor!  Avoid copies
	Mesh(std::vector<Vertex>&& vertices, std::vector<unsigned int>&& indices, std::vector<Texture>&& textures) {
		this->vertices = std::move(vertices);
		this->indices = std::move(indices);
		this->textures = std::move(textures);
		
		setupMesh();
	}

	//Disable all copying of a mesh for now.  If a mesh copy goes out of scope, the other copy could have its GPU side memory freed by the destructor?
	Mesh(const Mesh&) = delete;
	Mesh& operator=(Mesh const&) = delete;

	~Mesh(){
		glDeleteBuffers(1, &EBO);
		glDeleteBuffers(1, &VBO);
		glDeleteVertexArrays(1, &VAO);
	}

private:
	unsigned int VAO, VBO, EBO;
	
	void setupMesh() {
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

	void draw(Shader &shader) {
		unsigned int diffuseNr = 1;
		unsigned int specularNr = 1;

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

		//draw
		glBindVertexArray(VAO);
		glDrawElements(GL_TRIANGLES, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
		glBindVertexArray(0);
	}
};


#endif	