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

ThreadPool threadPool{ 4 };

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

    double totalTime = 0;
    double framesRendered = 0;

    //Setup shader
    lightingShader.use();
    lightingShader.setBool("pointLightOn", false);
 
    //Directional
    lightingShader.setVec3("directionalLight.direction", -0.2f, -1.0f, -0.3f);
    lightingShader.setVec3("directionalLight.ambient", 0.1f, 0.1f, 0.1f);
    lightingShader.setVec3("directionalLight.diffuse", 1.0f, 1.0f, 1.0f);
    lightingShader.setVec3("directionalLight.specular", 1.0f, 1.0f, 1.0f);
    
    //Spotlight
    lightingShader.setVec3("spotLight.ambient", 0.05f, 0.05f, 0.05f);
    lightingShader.setVec3("spotLight.diffuse", 1.0f, 1.0f, 1.0f);
    lightingShader.setVec3("spotLight.specular", 1.0f, 1.0f, 1.0f);

    lightingShader.setVec3("spotLight.position", camera.Position);
    lightingShader.setVec3("spotLight.direction", camera.Front);
  
    lightingShader.setFloat("spotLight.constant", 1.0f);
    lightingShader.setFloat("spotLight.linear", 0.045f);
    lightingShader.setFloat("spotLight.quadratic", 0.0075f);
    
    lightingShader.setFloat("spotLight.cutoff", glm::cos(glm::radians(12.5f)));
    lightingShader.setFloat("spotLight.outerCutoff", glm::cos(glm::radians(17.5f)));
    
    //Model
    glm::mat4 model = glm::mat4(1.0f);
    lightingShader.setMat4("modelMatrix", model);

    //Render Loop
    while (!glfwWindowShouldClose(window)){

        //Delta Time
        currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        totalTime += deltaTime;
        framesRendered++;

        //Process user input
        processInput(window);

        //Clear background
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        lightingShader.setVec3("uViewPosition", camera.Position);
        
        //Define and set projection matrix
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCREEN_WIDTH / (float)SCREEN_HEIGHT, 0.1f, 100.0f);
        lightingShader.setMat4("projectionMatrix", projection);

        //Define and set the view matrix
        glm::mat4 view = camera.GetViewMatrix();
        lightingShader.setMat4("viewMatrix", view);

        //Normal matrix
        glm::mat3 normal = (glm::mat3)(glm::transpose(glm::inverse(model)));
        lightingShader.setMat3("normalMatrix", normal);

        //Materials
        lightingShader.setFloat("material.shininess", 32.0f);

        if (directionalLightOn) 
            lightingShader.setBool("directionalLightOn", true);
        else
            lightingShader.setBool("directionalLightOn", false);

        if (spotLightOn) {
            lightingShader.setBool("spotLightOn", true);
            lightingShader.setVec3("spotLight.position", camera.Position);
            lightingShader.setVec3("spotLight.direction", camera.Front);
        }
        else
            lightingShader.setBool("spotLightOn", false);

        backpack.draw(lightingShader);

        glfwSwapBuffers(window);
        glfwPollEvents();
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

    camera.ProcessMouseMovement(xOffset, yOffset, GL_TRUE);
}

void scrollCallback(GLFWwindow* window, double xOffset, double yOffset) {
    camera.ProcessMouseScroll((float)yOffset);
}

void processInput(GLFWwindow* window) {

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

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  //  if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
      //  pointLightsOn = !pointLightsOn;

     if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS)
        spotLightOn = !spotLightOn;

    else if (glfwGetKey(window, GLFW_KEY_L) == GLFW_PRESS)
        directionalLightOn = !directionalLightOn;
}