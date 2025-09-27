#include "../headers/camera.h"
#include "../headers/shader.h"
#include "../headers/model.h"
#include "../headers/thread_pool.h"
#include "../headers/skybox.h"
#include "../headers/space_model.h"
#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

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
void processInput(GLFWwindow* window);
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

//Delta Time
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float currentFrame = 0.0f;

//Mouse Positions
float lastX = SCREEN_WIDTH / 2;
float lastY = SCREEN_HEIGHT / 2;

Camera camera(glm::vec3(0.0f, 3.0f, 30.0f));

bool firstMouseMovement = true;
bool spotLightOn = false;
bool directionalLightOn = true;
bool pointLightsOn = false;

ThreadPool threadPool;

static const glm::vec3 startingLightPos = glm::vec3(0.0f, 3.0f, 23.0f);

float cubeVertices[] = {
    // back face
    -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
     1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
     1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 0.0f, // bottom-right         
     1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 1.0f, 1.0f, // top-right
    -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 0.0f, // bottom-left
    -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, -1.0f, 0.0f, 1.0f, // top-left
    // front face
    -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
     1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 0.0f, // bottom-right
     1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
     1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 1.0f, 1.0f, // top-right
    -1.0f,  1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 1.0f, // top-left
    -1.0f, -1.0f,  1.0f,  0.0f,  0.0f,  1.0f, 0.0f, 0.0f, // bottom-left
    // left face
    -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
    -1.0f,  1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-left
    -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
    -1.0f, -1.0f, -1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-left
    -1.0f, -1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-right
    -1.0f,  1.0f,  1.0f, -1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-right
    // right face
     1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
     1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
     1.0f,  1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 1.0f, // top-right         
     1.0f, -1.0f, -1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 1.0f, // bottom-right
     1.0f,  1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 1.0f, 0.0f, // top-left
     1.0f, -1.0f,  1.0f,  1.0f,  0.0f,  0.0f, 0.0f, 0.0f, // bottom-left     
     // bottom face
     -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
      1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 1.0f, // top-left
      1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
      1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 1.0f, 0.0f, // bottom-left
     -1.0f, -1.0f,  1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 0.0f, // bottom-right
     -1.0f, -1.0f, -1.0f,  0.0f, -1.0f,  0.0f, 0.0f, 1.0f, // top-right
     // top face
     -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
      1.0f,  1.0f , 1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
      1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 1.0f, // top-right     
      1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 1.0f, 0.0f, // bottom-right
     -1.0f,  1.0f, -1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 1.0f, // top-left
     -1.0f,  1.0f,  1.0f,  0.0f,  1.0f,  0.0f, 0.0f, 0.0f  // bottom-left        
};

int main() {
    //Average FPS
    double totalTime = 0;
    double framesRendered = 0;

    //Creating the window
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 8);

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

    //Vsync Off
    glfwSwapInterval(0);

    //Vsync On
    //glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cout << "Failed to initialize GLAD" << std::endl;
        return -1;
    }
    /*
    //Get actual width & height in pixels of the window and pass to openGL.  For selecting resolution before launching
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);
    */

    //OpenGL Options
    glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_FRAMEBUFFER_SRGB);

    //GLFW Settings
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);

    //stbi_set_flip_vertically_on_load(true);

    //Compile shader programs
    Shader pbrShader = Shader("./shaders/point_PBR.vert", "./shaders/point_PBR.frag");
    Shader lightSource = Shader("./shaders/light_source.vert", "./shaders/light_source.frag");
    Shader skyboxShader = Shader("./shaders/skybox.vert", "./shaders/skybox.frag");
    Shader reflectiveSpaceShader = Shader("./shaders/reflective.vert", "./shaders/reflective.frag");

    std::vector<std::string> skyBoxPaths = {
        "./textures/skybox/right.png",
            "./textures/skybox/left.png",
            "./textures/skybox/top.png",
            "./textures/skybox/bottom.png",
            "./textures/skybox/front.png",
            "./textures/skybox/back.png"
    };
    
    //Load all models and stop thread pool
    Model islandModel("./models/island/island.gltf");
    SpaceModel spaceModel("./models/dodecahedron/scene.gltf");
    Skybox skybox(skyBoxPaths);
    threadPool.stopAllThreads();
 
    //Setup matrices
    glm::mat4 modelMatrix = glm::mat4(1.0f);
    glm::mat4 projectionMatrix = glm::perspective(glm::radians(camera.zoom), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 600.0f);
    glm::mat4 viewMatrix = camera.GetViewMatrix();
    glm::mat4 viewNoTransMatrix = glm::mat4(glm::mat3(viewMatrix)); //For skybox
    glm::mat3 normalMVMatrix = glm::transpose(glm::inverse(glm::mat3(viewMatrix * modelMatrix)));
    glm::vec3 lightPosition = startingLightPos;
    glm::mat4 spaceModelMatrix = glm::mat4(1.0f);
    spaceModelMatrix = glm::translate(spaceModelMatrix, glm::vec3(3.0f, 3.0f, 30.0f));
    glm::mat4 spaceNormalMatrix = glm::transpose(glm::inverse(glm::mat3(spaceModelMatrix)));

    //Matrices Uniform Buffer Object
    unsigned int uboMatrices;
    glGenBuffers(1, &uboMatrices);
    glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
    glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboMatrices);
    glm::mat4 packedMatrices[] = { viewMatrix, projectionMatrix * viewMatrix };
    glBufferSubData(GL_UNIFORM_BUFFER, 0, 2 * sizeof(glm::mat4), packedMatrices);

    //light source VAO
    unsigned int lightSourceVBO, lightSourceVAO;
    glGenVertexArrays(1, &lightSourceVAO);
    glGenBuffers(1, &lightSourceVBO);
    glBindVertexArray(lightSourceVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lightSourceVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    //Final shader uniform setup before loop
    pbrShader.use();
    pbrShader.setUniformBlock("matrices", 0);
    pbrShader.setMat4("modelMatrix", modelMatrix);
    pbrShader.setMat3("normalMV", normalMVMatrix);
    pbrShader.setVec3("lightPosition", lightPosition);
    pbrShader.setInt("albedoMap", 0);
    pbrShader.setInt("occlusionRoughMetalMap", 1);
    pbrShader.setInt("normalMap", 2);

    reflectiveSpaceShader.use();
    reflectiveSpaceShader.setUniformBlock("matrices", 0);
    reflectiveSpaceShader.setMat4("modelMatrix", spaceModelMatrix);
    reflectiveSpaceShader.setMat3("normalMatrix", spaceNormalMatrix);
    reflectiveSpaceShader.setVec3("cameraPos", camera.position);
    reflectiveSpaceShader.setInt("cubemap", 0);

    lightSource.use();
    lightSource.setUniformBlock("matrices", 0);
    glm::mat4 lightSourceModel = glm::mat4(1.0f);
    lightSourceModel = glm::translate(lightSourceModel, lightPosition);
    lightSourceModel = glm::scale(lightSourceModel, glm::vec3(0.02f));
    lightSource.setMat4("modelMatrix", lightSourceModel);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    while (true){
        //Clear background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //Delta Time
        currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        totalTime += deltaTime;
        framesRendered++;

        processInput(window);
        glfwPollEvents();
        
        if (glfwWindowShouldClose(window))
            break;

        lightPosition.x = startingLightPos.x + (float)sin((float)glfwGetTime() * 0.25f);
        lightPosition.z = startingLightPos.z + (float)cos((float)glfwGetTime() * 0.25f);

        pbrShader.use();
        pbrShader.setVec3("lightPosition", lightPosition);
        
        if (camera.hasUpdated()) {
            viewMatrix = camera.GetViewMatrix();
            viewNoTransMatrix = glm::mat4(glm::mat3(viewMatrix));
            packedMatrices[0] = viewMatrix;
            packedMatrices[1] = projectionMatrix * viewMatrix;
            glBufferSubData(GL_UNIFORM_BUFFER, 0, 2 * sizeof(glm::mat4), packedMatrices);

            reflectiveSpaceShader.use();
            reflectiveSpaceShader.setVec3("cameraPos", camera.position);

            pbrShader.use();
            pbrShader.setMat3("normalMV", glm::transpose(glm::inverse(glm::mat3(viewMatrix * modelMatrix))));
        }
 
        islandModel.draw(pbrShader);

        reflectiveSpaceShader.use();
        spaceModelMatrix = glm::rotate(spaceModelMatrix, glm::radians(10.0f) * deltaTime, glm::vec3(0.3f, 0.7f, 0.5f));
        reflectiveSpaceShader.setMat4("modelMatrix", spaceModelMatrix);
        reflectiveSpaceShader.setMat3("normalMatrix", glm::transpose(glm::inverse(glm::mat3(spaceModelMatrix))));
        spaceModel.draw(reflectiveSpaceShader);

        lightSource.use();
        lightSourceModel = glm::mat4(1.0f);
        lightSourceModel = glm::translate(lightSourceModel, lightPosition);
        lightSourceModel = glm::scale(lightSourceModel, glm::vec3(0.02f));
        lightSource.setMat4("modelMatrix", lightSourceModel);

        glBindVertexArray(lightSourceVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);

        skyboxShader.use();
        skyboxShader.setMat4("projectionViewNoTranslation", projectionMatrix * viewNoTransMatrix);
        skybox.draw(skyboxShader);

        glfwSwapBuffers(window);
    }

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

    camera.processMouseMovement(xOffset, yOffset, GL_TRUE);
}

void scrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
    camera.processMouseScroll((float)yOffset);
}

void processInput(GLFWwindow* window) {

    float movementSpeed = 3.0f * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera.processKeyboard(FORWARD, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        camera.processKeyboard(BACKWARD, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.processKeyboard(LEFT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.processKeyboard(RIGHT, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera.processKeyboard(UP, deltaTime);

    if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
        camera.processKeyboard(DOWN, deltaTime);
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
     /*
     if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
        pointLightsOn = !pointLightsOn;
     
     if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        spotLightOn = !spotLightOn;
    
    else if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        directionalLightOn = !directionalLightOn;
     */
}

