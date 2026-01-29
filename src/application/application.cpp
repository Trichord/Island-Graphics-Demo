#include "application.h"

#include <iostream>

#include "camera.h"
#include "config_window.h"
#include "gl_context.h"
#include "glm/glm.hpp"
#include "island_scene.h"
#include "renderer.h"
#include "scene.h"

void Application::run() {
  ConfigWindow configWindow;
  ConfigWindow::GraphicsSettings graphicsSettings;

  if (!configWindow.launchSettingsWindow(graphicsSettings)) return;

  createWindow(graphicsSettings);
  if (!window) return;

  createOpenGLViewport(graphicsSettings);
  registerCallbacks();

  Renderer renderer(graphicsSettings);

  IslandScene islandScene(renderer.getTextureManager(), graphicsSettings);

  renderer.initializeBuffers(islandScene);
  renderer.updateBuffers(islandScene);

  addGlfwUserParam(islandScene);

  // Average FPS
  double totalTime{0};
  double framesRendered{0};

  while (!glfwWindowShouldClose(window)) {
    // Delta Time
    currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // Final FPS count
    totalTime += deltaTime;
    framesRendered++;

    // Update GPU shared buffer if camera has moved
    if (islandScene.camera.hasUpdated()) renderer.updateBuffers(islandScene);

    // Update scene objects
    onUpdate(islandScene);

    // Draw
    renderer.renderScene(islandScene);

    glfwSwapBuffers(window);

    // User input
    glfwPollEvents();
    processInput(islandScene);
  }

  glfwTerminate();
  std::cout << "Average FPS: " << framesRendered / totalTime << std::endl;
  return;
}

void Application::createWindow(
    const ConfigWindow::GraphicsSettings& graphicsSettings) {
  glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

  if (graphicsSettings.msaa)
    glfwWindowHint(GLFW_SAMPLES, graphicsSettings.msaa);

  int width{graphicsSettings.screenWidth};
  int height{graphicsSettings.screenHeight};

  if (graphicsSettings.fullscreen)
    window = glfwCreateWindow(width, height, "Island Graphics Demo",
                              glfwGetPrimaryMonitor(), nullptr);

  else
    window =
        glfwCreateWindow(width, height, "Island Graphics Demo", NULL, NULL);

  if (!window) {
    std::cout << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
  }

  glfwMakeContextCurrent(window);

  if (graphicsSettings.vsync)
    glfwSwapInterval(1);

  else
    glfwSwapInterval(0);
}

void Application::createOpenGLViewport(
    const ConfigWindow::GraphicsSettings& graphicsSettings) {
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    glfwTerminate();
    return;
  }

  GLsizei width{static_cast<GLsizei>(graphicsSettings.screenWidth)};
  GLsizei height{static_cast<GLsizei>(graphicsSettings.screenHeight)};

  glViewport(0, 0, width, height);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glEnable(GL_FRAMEBUFFER_SRGB);
  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

  if (graphicsSettings.msaa) glEnable(GL_MULTISAMPLE);
}

void Application::processInput(Scene& scene) {
  using enum CameraMovement;
  if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
    glfwSetWindowShouldClose(window, GLFW_TRUE);
    return;
  }

  if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    scene.camera.processKeyboard(FORWARD, deltaTime);

  if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    scene.camera.processKeyboard(BACKWARD, deltaTime);

  if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    scene.camera.processKeyboard(LEFT, deltaTime);

  if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    scene.camera.processKeyboard(RIGHT, deltaTime);

  if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
    scene.camera.processKeyboard(UP, deltaTime);

  if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS)
    scene.camera.processKeyboard(DOWN, deltaTime);
}

void Application::registerCallbacks() {
  glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
  glfwSetKeyCallback(window, keyCallback);
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Application::addGlfwUserParam(Scene& scene) {
  glfwSetWindowUserPointer(window, &scene);
}

void Application::onUpdate(Scene& scene) {
  // Rotate the fancy space model
  auto& spaceModel = scene.getObjectTransform("spaceModel");
  spaceModel.rotateAroundAxis(5.0f * deltaTime, glm::vec3{0.34f, 0.87f, 0.4f});
}

void mouse_callback(GLFWwindow* window, double xPos, double yPos) {
  static bool firstMouse{true};

  if (firstMouse) {
    lastMouseX = (float)xPos;
    lastMouseY = (float)yPos;
    firstMouse = false;
  }

  float xOffset{(float)(xPos - lastMouseX)};
  float yOffset{(float)(lastMouseY - yPos)};

  lastMouseX = (float)xPos;
  lastMouseY = (float)yPos;

  auto scenePointer =
      static_cast<IslandScene*>(glfwGetWindowUserPointer(window));

  if (scenePointer)
    scenePointer->camera.processMouseMovement(xOffset, yOffset, GL_TRUE);
}

// Global callbacks for GLFW
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
}

static void keyCallback(GLFWwindow* window, int key, int scancode, int action,
                        int mods) {
  // Anything that needs to avoid key bouncing
}