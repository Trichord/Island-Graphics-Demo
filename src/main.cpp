#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>

#include "../headers/camera.h"
#include "../headers/model.h"
#include "../headers/shader.h"
#include "../headers/skybox.h"
#include "../headers/space_model.h"
#include "../headers/thread_pool.h"
#include "../headers/transform.h"
#include "../headers/demo_scene.h"
#include "../headers/renderer.h"

#define SCREEN_WIDTH 2560
#define SCREEN_HEIGHT 1440

enum LightType { POINT_LIGHT, DIRECTIONAL_LIGHT, SPOT_LIGHT };

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xPos, double yPos);
void processInput(GLFWwindow* window);
void keyCallback(GLFWwindow* window, int key, int scancode, int action,
                 int mods);

// Delta Time
float deltaTime = 0.0f;
float lastFrame = 0.0f;
float currentFrame = 0.0f;

// Mouse Positions
float lastX = SCREEN_WIDTH / 2;
float lastY = SCREEN_HEIGHT / 2;

bool firstMouseMovement = true;

ThreadPool threadPool;

int main() {
  // Average FPS
  double totalTime = 0;
  double framesRendered = 0;

  // Creating the window
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_SAMPLES, 8);

  // Windowed
  // GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OpenGL
  // Demo", NULL, NULL);

  // Fullscreen
  GLFWwindow* window =
      glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OpenGL Demo",
                       glfwGetPrimaryMonitor(), NULL);

  if (window == NULL) {
    std::cout << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return -1;
  }

  glfwMakeContextCurrent(window);

  // Vsync Off
  glfwSwapInterval(0);

  // Vsync On
  // glfwSwapInterval(1);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return -1;
  }

  // OpenGL Options
  glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_FRAMEBUFFER_SRGB);

  // GLFW Settings
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetKeyCallback(window, keyCallback);

  // stbi_set_flip_vertically_on_load(true);

  DemoScene demoScene;
  threadPool.stopAllThreads();
  Renderer renderer(demoScene.camera, SCREEN_WIDTH, SCREEN_HEIGHT);

  glfwSetWindowUserPointer(window, &demoScene);

  while (true) {
    // Clear background
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Delta Time
    currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    totalTime += deltaTime;
    framesRendered++;

    processInput(window);
    glfwPollEvents();

    if (glfwWindowShouldClose(window)) break;

    /*
    lightPosition.x =
        startingLightPos.x + (float)sin((float)glfwGetTime() * 0.25f);
    lightPosition.z =
        startingLightPos.z + (float)cos((float)glfwGetTime() * 0.25f);

    pointPBRShader.use();
    pointPBRShader.setVec3("lightPosition", lightPosition);
    */

    if (demoScene.camera.hasUpdated()) {
      renderer.updateUBO(demoScene.camera);
    }

    renderer.renderScene(demoScene);

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

  auto scenePointer = static_cast<DemoScene*>(glfwGetWindowUserPointer(window));
  scenePointer->camera.processMouseMovement(xOffset, yOffset, GL_TRUE);
}

void processInput(GLFWwindow* window) {
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
    return;
  }
  
  auto scenePointer = static_cast<DemoScene*>(glfwGetWindowUserPointer(window));
  Camera& camera = scenePointer->camera;

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

void keyCallback(GLFWwindow* window, int key, int scancode, int action,
                 int mods) {
  // Anything that needs to avoid key bouncing
}
