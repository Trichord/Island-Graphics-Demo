#include "application.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>

#include "config_window.h"
#include "island_scene.h"
#include "object_positions.h"
#include "renderer.h"
#include "scene.h"
#include "thread_pool.h"

void Application::run() {
  //if (!configWindow.launchSettingsWindow()) return;

  createWindow();
  if (!window) return;

  createOpenGLViewport();
  registerCallbacks();

  IslandScene islandScene;
  Renderer renderer(ConfigWindow::globalGraphicsSettings.screenWidth,
                    ConfigWindow::globalGraphicsSettings.screenHeight);
  threadPool.stopAllThreads();

  addGlfwUserParam(islandScene);
  centerMouse();

  renderer.updateUBO(islandScene);

  // Average FPS
  double totalTime{0};
  double framesRendered{0};

  while (true) {
    // Clear screen
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Delta Time
    currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    // Final FPS count
    totalTime += deltaTime;
    framesRendered++;

    // User input
    processInput(islandScene);
    glfwPollEvents();

    // Exit condition
    if (glfwWindowShouldClose(window)) break;

    // Update GPU shared buffer if camera has moved
    if (islandScene.camera.hasUpdated()) renderer.updateUBO(islandScene);

    // Update scene objects
    onUpdate(islandScene, deltaTime);

    // Draw
    renderer.renderScene(islandScene);

    glfwSwapBuffers(window);
  }

  glfwTerminate();
  std::cout << "Average FPS: " << framesRendered / totalTime << std::endl;
  return;
}

void Application::createWindow() {
  glfwInit();

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_FLOATING, GLFW_TRUE);

  unsigned int msaa{ConfigWindow::globalGraphicsSettings.msaa};
  if (msaa != 0) glfwWindowHint(GLFW_SAMPLES, msaa);

  unsigned int width{ConfigWindow::globalGraphicsSettings.screenWidth};
  unsigned int height{ConfigWindow::globalGraphicsSettings.screenHeight};

  if (ConfigWindow::globalGraphicsSettings.fullscreen)
    window = glfwCreateWindow(width, height, "OpenGL Demo",
                              glfwGetPrimaryMonitor(), nullptr);

  else
    window = glfwCreateWindow(width, height, "OpenGL Demo", NULL, NULL);

  if (!window) {
    std::cout << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
  }

  glfwMakeContextCurrent(window);

  if (ConfigWindow::globalGraphicsSettings.vsync)
    glfwSwapInterval(1);

  else
    glfwSwapInterval(0);
}

void Application::createOpenGLViewport() {
  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    glfwTerminate();
    return;
  }

  unsigned int width{ConfigWindow::globalGraphicsSettings.screenWidth};
  unsigned int height{ConfigWindow::globalGraphicsSettings.screenHeight};
  unsigned int msaa{ConfigWindow::globalGraphicsSettings.msaa};

  glViewport(0, 0, width, height);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glEnable(GL_FRAMEBUFFER_SRGB);

  if (msaa != 0) glEnable(GL_MULTISAMPLE);
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
}

void Application::centerMouse() {
  // Set mouse position before registering callback
  double x{0.0f};
  double y{0.0f};
  glfwGetCursorPos(window, &x, &y);
  lastMouseX = (float)x;
  lastMouseY = (float)y;
  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Application::addGlfwUserParam(Scene& scene) {
  glfwSetWindowUserPointer(window, &scene);
}

void Application::onUpdate(Scene& scene, float deltaTime) {
  // Rotate the fancy space dodecahedron
  auto& spaceDodeca = scene.getObjectTransform("spaceDodeca");
  spaceDodeca.rotateAroundAxis(5.0f * deltaTime, glm::vec3{0.34f, 0.87f, 0.4f});

  // Move the light around the flight helmet
  auto& circlingLight =
      static_cast<PointLight&>(scene.getNamedLight("circlingLight"));

  glm::vec3 newPos;
  const float radius{1.25f};
  float time{(float)glfwGetTime()};

  newPos.x = LIGHTBULB_START.x + glm::sin(time * 0.5f) * radius;
  newPos.z = LIGHTBULB_START.z + glm::cos(time * 0.5f) * radius;
  newPos.y = LIGHTBULB_START.y;

  circlingLight.setPosition(newPos);

  Transform& lightTransform{scene.getObjectTransform("lightbulb")};
  lightTransform.setAbsolutePosition(newPos);
}

void mouse_callback(GLFWwindow* window, double xPos, double yPos) {
  float xOffset{(float)(xPos - lastMouseX)};
  float yOffset{(float)(lastMouseY - yPos)};

  lastMouseX = (float)xPos;
  lastMouseY = (float)yPos;

  auto scenePointer = static_cast<IslandScene*>(glfwGetWindowUserPointer(window));

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
