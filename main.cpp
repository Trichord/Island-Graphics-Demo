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
//bool pointLightsOn = false;

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

ThreadPool threadPool;

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

    //Set Background Color
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);

    //Enable Z-Buffer
    glEnable(GL_DEPTH_TEST);

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

    //Load all models and stop thread pool
    Model backpack("./Models/Backpack/backpack.assbin");
    threadPool.stopAllThreads();

    //Setup shader
    lightingShader.use();
    lightingShader.setBool("uPointLightOn", false);
    lightingShader.setFloat("uMaterial.shininess", 32.0f);

    //Directional
    lightingShader.setVec3("uDirectionalLight.direction", -0.2f, -1.0f, -0.3f);
    lightingShader.setVec3("uDirectionalLight.ambient", 0.1f, 0.1f, 0.1f);
    lightingShader.setVec3("uDirectionalLight.diffuse", 1.0f, 1.0f, 1.0f);
    lightingShader.setVec3("uDirectionalLight.specular", 1.0f, 1.0f, 1.0f);
    
    //Spotlight
    lightingShader.setVec3("uSpotLight.ambient", 0.05f, 0.05f, 0.05f);
    lightingShader.setVec3("uSpotLight.diffuse", 1.0f, 1.0f, 1.0f);
    lightingShader.setVec3("uSpotLight.specular", 1.0f, 1.0f, 1.0f);

    lightingShader.setVec3("uSpotLight.position", camera.Position);
    lightingShader.setVec3("uSpotLight.direction", camera.Front);
  
    lightingShader.setFloat("uSpotLight.constant", 1.0f);
    lightingShader.setFloat("uSpotLight.linear", 0.045f);
    lightingShader.setFloat("uSpotLight.quadratic", 0.0075f);
    
    lightingShader.setFloat("uSpotLight.cutoff", glm::cos(glm::radians(12.5f)));
    lightingShader.setFloat("uSpotLight.outerCutoff", glm::cos(glm::radians(17.5f)));
    
    //Model
    glm::mat4 model = glm::mat4(1.0f);
    lightingShader.setMat4("modelMatrix", model);

    //Normal matrix
    glm::mat3 normal = (glm::mat3)(glm::transpose(glm::inverse(model)));
    lightingShader.setMat3("normalMatrix", normal);

    //Define and set projection matrix
    glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 100.0f);
    lightingShader.setMat4("projectionMatrix", projection);

    //Define and set the view matrix
    glm::mat4 view = camera.GetViewMatrix();
    lightingShader.setMat4("viewMatrix", view);

    lightingShader.setVec3("uViewPosition", camera.Position);

    double totalTime = 0;
    double framesRendered = 0;

    //Render Loop
    while (!glfwWindowShouldClose(window)){
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

        if (camera.hasUpdated()) {
            lightingShader.setVec3("uViewPosition", camera.Position);
            
            view = camera.GetViewMatrix();
            lightingShader.setMat4("viewMatrix", view);

            projection = glm::perspective(glm::radians(camera.Zoom), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 100.0f);
            lightingShader.setMat4("projectionMatrix", projection);
        }

        if (directionalLightOn) 
            lightingShader.setBool("uDirectionalLightOn", true);
        else
            lightingShader.setBool("uDirectionalLightOn", false);

        if (spotLightOn) {
            lightingShader.setBool("uSpotLightOn", true);
            lightingShader.setVec3("uSpotLight.position", camera.Position);
            lightingShader.setVec3("uSpotLight.direction", camera.Front);
        }
        else
            lightingShader.setBool("uSpotLightOn", false);

        backpack.draw(lightingShader);

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
  //  if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
      //  pointLightsOn = !pointLightsOn;

     if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        spotLightOn = !spotLightOn;

    else if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        directionalLightOn = !directionalLightOn;
}