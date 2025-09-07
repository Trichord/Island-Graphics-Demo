#define STB_IMAGE_IMPLEMENTATION

#include <iostream>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "camera.h"
#include "shader.h"
#include "model.h"
#include "thread_pool.h"
#include "skybox.h"
#include "space_icosahedron.h"

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

Camera camera(glm::vec3(0.0f, 0.0f, 3.0f));

bool firstMouseMovement = true;
bool spotLightOn = false;
bool directionalLightOn = true;
bool pointLightsOn = false;

ThreadPool threadPool;

int main() {
    //Average FPS
    double totalTime = 0;
    double framesRendered = 0;

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

    //GLFW Settings
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetCursorPosCallback(window, mouse_callback);
    glfwSetScrollCallback(window, scrollCallback);
    glfwSetKeyCallback(window, keyCallback);

    //Compile shader programs
    Shader lightingShader = Shader("./Shaders/lighting.vert", "./Shaders/lighting.frag");
    Shader skyboxShader = Shader("./Shaders/skybox.vert", "./Shaders/skybox.frag");
    Shader reflectiveShader = Shader("./Shaders/reflective.vert", "./Shaders/reflective.frag");
    Shader explodingGeoShader = Shader("./Shaders/geo.vert", "./Shaders/geo.frag", "./Shaders/geo.geom");
    
    //Set matrices UBO location
    lightingShader.setUniformBlock("matrices", 0);
    skyboxShader.setUniformBlock("matrices", 0);
    reflectiveShader.setUniformBlock("matrices", 0);
    explodingGeoShader.setUniformBlock("matrices", 0);
    
    //Load all models and stop thread pool
    Model backpack("./Models/Backpack/backpack.assbin"); 
    Skybox skybox;
    SpaceIcosaheron spaceIcosaheron;
    threadPool.stopAllThreads();

    //Setup lighting shader
    lightingShader.use();
    lightingShader.setBool("pointLightOn", false);
    lightingShader.setFloat("material.shininess", 32.0f);

    lightingShader.setVec3("directionalLight.direction", -0.2f, -1.0f, -0.3f);
    lightingShader.setVec3("directionalLight.ambient", 0.1f, 0.1f, 0.1f);
    lightingShader.setVec3("directionalLight.diffuse", 1.0f, 1.0f, 1.0f);
    lightingShader.setVec3("directionalLight.specular", 1.0f, 1.0f, 1.0f);
    
    lightingShader.setVec3("spotLight.ambient", 0.05f, 0.05f, 0.05f);
    lightingShader.setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
    lightingShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);

    lightingShader.setVec3("spotLight.position", camera.position);
    lightingShader.setVec3("spotLight.direction", camera.front);
  
    lightingShader.setFloat("spotLight.constant", 1.0f);
    lightingShader.setFloat("spotLight.linear", 0.045f);
    lightingShader.setFloat("spotLight.quadratic", 0.0075f);
    
    lightingShader.setFloat("spotLight.cutoff", glm::cos(glm::radians(12.5f)));
    lightingShader.setFloat("spotLight.outerCutoff", glm::cos(glm::radians(17.5f)));
    
    //Setup matrices
    glm::mat4 model = glm::mat4(1.0f);
    glm::mat4 projection = glm::perspective(glm::radians(camera.zoom), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 100.0f);
    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 viewNoTrans = glm::mat4(glm::mat3(view));
    glm::mat3 normal = (glm::mat3)(glm::transpose(glm::inverse(model)));
    glm::mat4 spaceIcoModel = glm::mat4(1.0f);
    spaceIcoModel = glm::translate(spaceIcoModel, glm::vec3(-5.0f, 0.0f, 0.0f));
    glm::mat3 spaceIcoNormalMat = (glm::mat3)(glm::transpose(glm::inverse(spaceIcoModel)));

    //Matrices Uniform Buffer Object
    unsigned int uboMatrices;
    glGenBuffers(1, &uboMatrices);
    glBindBuffer(GL_UNIFORM_BUFFER, uboMatrices);
    glBufferData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, uboMatrices);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view));
    glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(projection));

    //Final shader uniform setup before loop
    lightingShader.setMat4("modelMatrix", model);
    lightingShader.setMat3("normalMatrix", normal);
    lightingShader.setVec3("viewPosition", camera.position);

    skyboxShader.use();
    skyboxShader.setInt("skybox", 0);

    reflectiveShader.use();
    reflectiveShader.setInt("skybox", 0);

    explodingGeoShader.use();
    explodingGeoShader.setMat4("modelMatrix", glm::translate(glm::mat4(1.0f), glm::vec3(7.0f, 0.0f, 0.0f)));
    explodingGeoShader.setFloat("time", (float)glfwGetTime());

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

        lightingShader.use();

        if (camera.hasUpdated()) {
            lightingShader.setVec3("viewPosition", camera.position);
            view = camera.GetViewMatrix();
            viewNoTrans = glm::mat4(glm::mat3(view));
            glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(view));
        }

        if (directionalLightOn) 
            lightingShader.setBool("directionalLightOn", true);
        else
            lightingShader.setBool("directionalLightOn", false);

        if (spotLightOn) {
            lightingShader.setBool("spotLightOn", true);
            lightingShader.setVec3("spotLight.position", camera.position);
            lightingShader.setVec3("spotLight.direction", camera.front);
        }
        else
            lightingShader.setBool("spotLightOn", false);

        //Draw
        backpack.draw(lightingShader);

        explodingGeoShader.use();
        explodingGeoShader.setFloat("time", (float)glfwGetTime());
        glDisable(GL_CULL_FACE);
        backpack.draw(explodingGeoShader);
        glEnable(GL_CULL_FACE);

        reflectiveShader.use();
        spaceIcoModel = glm::rotate(spaceIcoModel, (float)glm::radians(deltaTime * 10), glm::vec3(.885, .543, .75));
        reflectiveShader.setMat4("modelMatrix", spaceIcoModel);
        reflectiveShader.setMat3("normalMatrix", (glm::mat3)(glm::transpose(glm::inverse(spaceIcoModel))));
        reflectiveShader.setVec3("cameraPos", camera.position);
        spaceIcosaheron.draw();

        skyboxShader.use();
        skyboxShader.setMat4("viewNoTrans", viewNoTrans);
        skybox.draw();

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
     */
     if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        spotLightOn = !spotLightOn;

    else if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        directionalLightOn = !directionalLightOn;
}

