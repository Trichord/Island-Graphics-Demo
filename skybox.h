#pragma once

#include "image_loader.h"
#include <glad/glad.h>
#include <vector>
#include <unordered_map>
#include <string>

//Skybox owns its texure ID's, VAO, VBO, and EBO.  Therefore copying is disabled
class Skybox {
public:
    Skybox(const Skybox&) = delete;
    Skybox& operator=(Skybox const&) = delete;

    Skybox() {
        setupBuffers();
        loadTextures();
    }

    ~Skybox() {
        glDeleteTextures(1, &skyboxTextureID);
        glDeleteBuffers(1, &skyboxEBO);
        glDeleteBuffers(1, &skyboxVBO);
        glDeleteVertexArrays(1, &skyboxVAO); 
    }

    void draw() {
        GLint previousVAO = 0, previousTexUnit = 0, previousTex = 0;

        //Save
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &previousVAO);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &previousTexUnit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTex);

        //Draw
        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTextureID);
        glDepthFunc(GL_LEQUAL);
        glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
        glDepthFunc(GL_LESS);

        //Restore
        glBindVertexArray(previousVAO);
        glActiveTexture(previousTexUnit);
        glBindTexture(GL_TEXTURE_2D, previousTex);
    }
private:
	unsigned int skyboxVAO, skyboxVBO, skyboxEBO, skyboxTextureID;

     inline static const float skyboxVertices[] = {
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f 
    };

     inline static const unsigned int skyboxIndices[] = {
        2, 1, 0,
        0, 3, 2,
        6, 5, 4,
        4, 7, 6,
        7, 3, 0,
        0, 4, 7,
        6, 2, 1,
        1, 5, 6,
        5, 1, 0,
        0, 4, 5,
        6, 7, 3,
        3, 2, 6
    };

    std::vector<std::string> skyboxPaths = {
        "./Textures/skybox/right.jpg",
        "./Textures/skybox/left.jpg",
        "./Textures/skybox/top.jpg",
        "./Textures/skybox/bottom.jpg",
        "./Textures/skybox/front.jpg",
        "./Textures/skybox/back.jpg"
    };

    void setupBuffers() {
        glGenVertexArrays(1, &skyboxVAO);
        glBindVertexArray(skyboxVAO);

        glGenBuffers(1, &skyboxVBO);
        glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);

        glGenBuffers(1, &skyboxEBO);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, skyboxEBO);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(skyboxIndices), skyboxIndices, GL_STATIC_DRAW);

        glBindVertexArray(0);
    }

    void loadTextures() {
        ImageLoader imageLoader(skyboxPaths);
        std::unordered_map<std::string, ImageData> loadedImages = imageLoader.getImageData();

        //Save state
        GLint previousTexUnit = 0, previousTex = 0, previousTex0 = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &previousTexUnit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTex);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTex0);

        glGenTextures(1, &skyboxTextureID);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTextureID);

        //Cubemap ID
        for (auto& imagePair : loadedImages) {
            int nrChannels = imagePair.second.nrChannels;
            unsigned char* data = imagePair.second.data;

            if (data) {
                GLenum imageFormat;

                //stb_image only ever returns a 1, 2, 3, or 4 for a valid image
                if (nrChannels == 1)
                    imageFormat = GL_RED;
                else if (nrChannels == 2)
                    imageFormat = GL_RG;
                else if (nrChannels == 3)
                    imageFormat = GL_RGB;
                else if (nrChannels == 4)
                    imageFormat = GL_RGBA;

                std::string path = imagePair.first;
                std::string face = path.substr(path.find_last_of('/') + 1);
                face = face.substr(0, face.find_last_of('.'));

                unsigned int cubeTargetOffset = 0;

                if (face == "left")
                    cubeTargetOffset = 1;
                else if (face == "top")
                    cubeTargetOffset = 2;
                else if (face == "bottom")
                    cubeTargetOffset = 3;
                else if (face == "back")
                    cubeTargetOffset = 4;
                else if (face == "front")
                    cubeTargetOffset = 5;

                glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + cubeTargetOffset,
                    0, GL_RGB, imagePair.second.width, imagePair.second.height, 0, GL_RGB, GL_UNSIGNED_BYTE, data
                );
            }
            else {
                std::cout << "Cube texture failed to load: " << imagePair.first << std::endl;
                skyboxTextureID = 0;
                return;
            }
        }

        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

        //Restore state
        glBindTexture(GL_TEXTURE_2D, previousTex0);
        glActiveTexture(previousTexUnit);
        glBindTexture(GL_TEXTURE_2D, previousTex);
    }
};