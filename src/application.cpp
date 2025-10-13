#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../headers/application.h"
#include "../headers/scene.h"
#include "../headers/demo_scene.h"
#include "../headers/renderer.h"
#include "../headers/thread_pool.h"
#include "../headers/config_window.h"
#include <iostream>

void Application::run() {
  if (!configWindow.launchSettingsWindow()) return;

  createWindow();
  if (!window) return;

  DemoScene demoScene;
  Renderer renderer(ConfigWindow::globalGraphicsSettings.screenWidth,
                    ConfigWindow::globalGraphicsSettings.screenHeight);
  threadPool.stopAllThreads();

  createOpenGLViewport();
  registerCallbacks();
  configureGLFW(demoScene);

  renderer.updateUBO(demoScene);

  // Average FPS
  double totalTime = 0;
  double framesRendered = 0;

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
    processInput(demoScene);
    glfwPollEvents();

    // Exit condition
    if (glfwWindowShouldClose(window)) break;

    // Update GPU shared buffer if camera has moved
    if (demoScene.camera.hasUpdated()) renderer.updateUBO(demoScene);

    // Update scene objects
    onUpdate(demoScene, deltaTime);

    // Draw
    renderer.renderScene(demoScene);

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

  unsigned int msaa = ConfigWindow::globalGraphicsSettings.msaa;
  if (msaa != 0) glfwWindowHint(GLFW_SAMPLES, msaa);

  unsigned int width = ConfigWindow::globalGraphicsSettings.screenWidth;
  unsigned int height = ConfigWindow::globalGraphicsSettings.screenHeight;
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

  unsigned int width = ConfigWindow::globalGraphicsSettings.screenWidth;
  unsigned int height = ConfigWindow::globalGraphicsSettings.screenHeight;
  unsigned int msaa = ConfigWindow::globalGraphicsSettings.msaa;

  glViewport(0, 0, width, height);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glEnable(GL_FRAMEBUFFER_SRGB);

  if (msaa != 0) glEnable(GL_MULTISAMPLE);
}

void Application::processInput(Scene& scene) {
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

  // Set mouse position before registering callback, to avoid mouse jump on
  // first load
  double x, y;
  glfwGetCursorPos(window, &x, &y);
  lastMouseX = (float)x;
  lastMouseY = (float)y;
  glfwSetCursorPosCallback(window, mouse_callback);
}

void Application::configureGLFW(Scene& scene) {
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwSetWindowUserPointer(window, &scene);
}

void Application::onUpdate(Scene& scene, float deltaTime) {
  Transform& spaceDodeca = scene.getObjectTransform("spaceDodeca");
  spaceDodeca.rotateAroundAxis(5.0f * deltaTime, glm::vec3(0.34f, 0.87f, 0.4f));
}

void mouse_callback(GLFWwindow* window, double xPos, double yPos) {
  float xOffset = (float)(xPos - lastMouseX);
  float yOffset = (float)(lastMouseY - yPos);

  lastMouseX = (float)xPos;
  lastMouseY = (float)yPos;

  auto scenePointer = static_cast<DemoScene*>(glfwGetWindowUserPointer(window));

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
