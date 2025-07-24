#define STB_IMAGE_IMPLEMENTATION

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "stb_image.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "camera.h"
#include "shader.h"
#include "mesh.h"

#define SCREEN_WIDTH 2560
#define SCREEN_HEIGHT 1440

enum LightType
{
    POINT_LIGHT,
    DIRECTIONAL_LIGHT,
    SPOT_LIGHT
};

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xPos, double yPos);
void scrollCallback(GLFWwindow* window, double xOffset, double yOffset);
void processInput(GLFWwindow* window, Shader targetShader);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
unsigned int loadTexture(char const* path);

//Delta Time
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float currentFrame = 0.0f;

//Mouse Positions
float lastX = SCREEN_WIDTH / 2;
float lastY = SCREEN_HEIGHT / 2;

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

bool firstMouseMovement = true;
bool spotLightOn = false;
bool directionalLightOn = false;
bool pointLightsOn = true;

//Textured Cube
float vertices[] = {
    // positions          // normals           // texture coords
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,
     0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  1.0f, 1.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f,  0.0f,  0.0f, -1.0f,  0.0f, 0.0f,

    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   1.0f, 1.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f,  0.0f,  0.0f, 1.0f,   0.0f, 0.0f,

    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
    -0.5f,  0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
    -0.5f, -0.5f, -0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
    -0.5f, -0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
    -0.5f,  0.5f,  0.5f, -1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,
     0.5f,  0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f,

    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,
     0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 1.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
     0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  1.0f, 0.0f,
    -0.5f, -0.5f,  0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 0.0f,
    -0.5f, -0.5f, -0.5f,  0.0f, -1.0f,  0.0f,  0.0f, 1.0f,

    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f,
     0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 1.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
     0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  1.0f, 0.0f,
    -0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 0.0f,
    -0.5f,  0.5f, -0.5f,  0.0f,  1.0f,  0.0f,  0.0f, 1.0f
};

glm::vec3 cubePositions[] = {
    glm::vec3(0.0f,  0.0f,  0.0f),
    glm::vec3(2.0f,  5.0f, -15.0f),
    glm::vec3(-1.5f, -2.2f, -2.5f),
    glm::vec3(-3.8f, -2.0f, -12.3f),
    glm::vec3(2.4f, -0.4f, -3.5f),
    glm::vec3(-1.7f,  3.0f, -7.5f),
    glm::vec3(1.3f, -2.0f, -2.5f),
    glm::vec3(1.5f,  2.0f, -2.5f),
    glm::vec3(1.5f,  0.2f, -1.5f),
    glm::vec3(-1.3f,  1.0f, -1.5f)
};

glm::vec3 pointLightPositions[] = {
    glm::vec3(0.7f,  0.2f,  2.0f),
    glm::vec3(2.3f, -3.3f, -4.0f),
    glm::vec3(-4.0f,  2.0f, -12.0f),
    glm::vec3(0.0f,  0.0f, -3.0f)
};

int main() {

    //Creating the window
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    //Windowed
    //GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OpenGL Demo", NULL, NULL);

    //Fullscreen
    GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OpenGL Demo", glfwGetPrimaryMonitor(), NULL);

    if (window == NULL) {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);

    //Vsync
    glfwSwapInterval(0);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    //Get actual width & height in pixels of the window and pass to openGL
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    //Automatically resize window
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

    //Enable mouse capture and register callback for mouse movements
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);

    //Register mouse scroll-wheel callback function
    glfwSetScrollCallback(window, scrollCallback);

    //glfw key handler to eliminate key-bounce
    glfwSetKeyCallback(window, keyCallback);

    //Compile shader programs
    Shader lightingShader = Shader("./Shaders/lighting.vert", "./Shaders/lighting.frag");
    Shader lightSourceShader = Shader("./Shaders/lightSource.vert", "./Shaders/lightSource.frag");

    //Create vertex array objects
    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    //Generate vertex buffer objects
    unsigned int VBO;
    glGenBuffers(1, &VBO);

    //Bind new buffer object to GL_ARRAY_BUFFER target
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    //Copy vertex data over to the GL_ARRAY_BUFFER
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    //Position vertex attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    //Normal Vector attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    //Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    //Create OpenGL Texture IDs
    lightingShader.use();

    glActiveTexture(GL_TEXTURE0);
    unsigned int diffuseMap = loadTexture("./Textures/stone.png");
    lightingShader.setInt("material.diffuse", 0);
    glBindTexture(GL_TEXTURE_2D, diffuseMap);

    glActiveTexture(GL_TEXTURE1);
    unsigned int specularMap = loadTexture("./Textures/stone_s.png");
    lightingShader.setInt("material.specular", 1);
    glBindTexture(GL_TEXTURE_2D, specularMap);

    //Create VAO for our light
    unsigned int lightVAO;
    glGenVertexArrays(1, &lightVAO);
    glBindVertexArray(lightVAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    //Set Background Color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    glEnable(GL_DEPTH_TEST);

    double totalTime = 0;
    double framesRendered = 0;

    //Render Loop
    while (!glfwWindowShouldClose(window)) {

        //Delta Time
        currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        totalTime += deltaTime;
        framesRendered++;

        //Process user input
        processInput(window, lightingShader);

        //Clear background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        lightingShader.use();
        lightingShader.setVec3("uViewPosition", camera.Position);

        //Define and set projection matrix
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 100.0f);
        lightingShader.setMat4("projectionMatrix", projection);

        //Define and set the view matrix
        glm::mat4 view = camera.GetViewMatrix();
        lightingShader.setMat4("viewMatrix", view);

        //Materials
        lightingShader.setFloat("material.shininess", 32.0f);

        //Light strength and color
        glm::vec3 lightColor(1.0f, 1.0f, 1.0f);
        glm::vec3 ambientColor = lightColor * 0.2f;
        glm::vec3 diffuseColor = lightColor * 0.5f;

        if (directionalLightOn) {
            lightingShader.setBool("directionalLightOn", true);
            //Directional
            lightingShader.setVec3("directionalLight.direction", -0.2f, -1.0f, -0.3f);
            lightingShader.setVec3("directionalLight.ambient", ambientColor);
            lightingShader.setVec3("directionalLight.diffuse", diffuseColor);
            lightingShader.setVec3("directionalLight.specular", 1.0f, 1.0f, 1.0f);
        }
        else
            lightingShader.setBool("directionalLightOn", false);

        if (pointLightsOn) {
            lightingShader.setBool("pointLightOn", true);

            //Point Light 1
            lightingShader.setVec3("pointLights[0].position", pointLightPositions[0]);
            lightingShader.setVec3("pointLights[0].ambient", ambientColor);
            lightingShader.setVec3("pointLights[0].diffuse", diffuseColor);
            lightingShader.setVec3("pointLights[0].specular", 1.0f, 1.0f, 1.0f);
            lightingShader.setFloat("pointLights[0].constant", 1.0f);
            lightingShader.setFloat("pointLights[0].linear", 0.14f);
            lightingShader.setFloat("pointLights[0].quadratic", 0.07f);

            //Point Light 2
            lightingShader.setVec3("pointLights[1].position", pointLightPositions[1]);
            lightingShader.setVec3("pointLights[1].ambient", ambientColor);
            lightingShader.setVec3("pointLights[1].diffuse", diffuseColor);
            lightingShader.setVec3("pointLights[1].specular", 1.0f, 1.0f, 1.0f);
            lightingShader.setFloat("pointLights[1].constant", 1.0f);
            lightingShader.setFloat("pointLights[1].linear", 0.14f);
            lightingShader.setFloat("pointLights[1].quadratic", 0.07f);

            //Point Light 3
            lightingShader.setVec3("pointLights[2].position", pointLightPositions[2]);
            lightingShader.setVec3("pointLights[2].ambient", ambientColor);
            lightingShader.setVec3("pointLights[2].diffuse", diffuseColor);
            lightingShader.setVec3("pointLights[2].specular", 1.0f, 1.0f, 1.0f);
            lightingShader.setFloat("pointLights[2].constant", 1.0f);
            lightingShader.setFloat("pointLights[2].linear", 0.14f);
            lightingShader.setFloat("pointLights[2].quadratic", 0.07f);

            //Point Light 4
            lightingShader.setVec3("pointLights[3].position", pointLightPositions[3]);
            lightingShader.setVec3("pointLights[3].ambient", ambientColor);
            lightingShader.setVec3("pointLights[3].diffuse", diffuseColor);
            lightingShader.setVec3("pointLights[3].specular", 1.0f, 1.0f, 1.0f);
            lightingShader.setFloat("pointLights[3].constant", 1.0f);
            lightingShader.setFloat("pointLights[3].linear", 0.14f);
            lightingShader.setFloat("pointLights[3].quadratic", 0.07f);
        }
        else
            lightingShader.setBool("pointLightOn", false);

        //Spotlight
        if (spotLightOn) {
            lightingShader.setBool("spotLightOn", true);
            lightingShader.setVec3("spotLight.position", camera.Position);
            lightingShader.setVec3("spotLight.direction", camera.Front);
            lightingShader.setFloat("spotLight.cutoff", glm::cos(glm::radians(12.5f)));
            lightingShader.setFloat("spotLight.outerCutoff", glm::cos(glm::radians(17.5f)));
            lightingShader.setVec3("spotLight.ambient", ambientColor);
            lightingShader.setVec3("spotLight.diffuse", diffuseColor);
            lightingShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);
        }
        else
            lightingShader.setBool("spotLightOn", false);

        //Draw our cube
        for (int i = 0; i < 10; i++) {
            float angle = 20.0f * i;
            glm::mat4 model = glm::mat4(1.0f);
            model = glm::translate(model, cubePositions[i]);
            model = glm::rotate(model, glm::radians(angle), glm::vec3(1.0f, 0.3f, 0.5f));
            lightingShader.setMat4("modelMatrix", model);

            glm::mat3 normal = (glm::mat3)(glm::transpose(glm::inverse(model)));
            lightingShader.setMat3("normalMatrix", normal);

            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }

        if (pointLightsOn) {
            lightSourceShader.use();

            for (int i = 0; i < 4; i++) {
                glm::mat4 model = glm::mat4(1.0f);
                model = glm::mat4(1.0f);
                model = glm::translate(model, pointLightPositions[i]);
                model = glm::scale(model, glm::vec3(0.125f));

                lightSourceShader.setMat4("viewMatrix", view);
                lightSourceShader.setMat4("projectionMatrix", projection);
                lightSourceShader.setMat4("modelMatrix", model);

                glBindVertexArray(lightVAO);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &VAO);
    glDeleteVertexArrays(1, &lightVAO);
    glDeleteBuffers(1, &VBO);

    std::cout << "Average FPS: " << framesRendered / totalTime << std::endl;

    glfwTerminate();
    return 0;
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

void mouse_callback(GLFWwindow* window, double xPos, double yPos) {

    if (firstMouseMovement) {
        lastX = (float)xPos;
        lastY = (float)yPos;
        firstMouseMovement = false;
    }

    float xOffset = (float)(xPos - lastX);
    float yOffset = (float)(lastY - yPos);

    lastX = (float)xPos;
    lastY = (float)yPos;

    camera.ProcessMouseMovement(xOffset, yOffset, GL_TRUE);
}

void scrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
    camera.ProcessMouseScroll((float)yOffset);
}

void processInput(GLFWwindow* window, Shader targetShader) {

    float movementSpeed = 3.0f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.ProcessKeyboard(FORWARD, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.ProcessKeyboard(BACKWARD, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.ProcessKeyboard(LEFT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.ProcessKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.ProcessKeyboard(UP, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        camera.ProcessKeyboard(DOWN, deltaTime);
}

//2D Texture from relative path
unsigned int loadTexture(char const* path) {

    unsigned int textureID = 0;
    int width, height, nrChannels;

    unsigned char* image = stbi_load(path, &width, &height, &nrChannels, 0);

    if (image) {
        GLenum imageFormat;

        if (nrChannels == 1)
            imageFormat = GL_RED;
        else if (nrChannels == 3)
            imageFormat = GL_RGB;
        else if (nrChannels == 4)
            imageFormat = GL_RGBA;
        else {
            std::cout << "Invalid number of nrChannels in: " << path << std::endl;
            return 0;
        }

        glGenTextures(1, &textureID);
        glBindTexture(GL_TEXTURE_2D, textureID);

        //Set texture repeat / sampling options
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        //Generate texture and mipmaps
        glTexImage2D(GL_TEXTURE_2D, 0, imageFormat, width, height, 0, imageFormat, GL_UNSIGNED_BYTE, image);
        glGenerateMipmap(GL_TEXTURE_2D);

        stbi_image_free(image);
    }
    else {
        std::cout << "Error loading texture image from: " << path << std::endl;
        stbi_image_free(image);
    }

    return textureID;
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
        pointLightsOn = !pointLightsOn;

    else if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        spotLightOn = !spotLightOn;

    else if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        directionalLightOn = !directionalLightOn;
}