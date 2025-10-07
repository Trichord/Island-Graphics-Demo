#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "../headers/application.h"
#include "../headers/scene.h"
#include "../headers/demo_scene.h"
#include "../headers/renderer.h"
#include "../headers/thread_pool.h"
#include <iostream>

void Application::run() {
  window = createWindow();
  if (!window) return;

  DemoScene demoScene;
  Renderer renderer(SCREEN_WIDTH, SCREEN_HEIGHT);
  threadPool.stopAllThreads();

  createOpenGLViewport();
  registerCallbacks();
  configureGLFW(demoScene);

  renderer.updateUBO(demoScene);

  // Average FPS
  double totalTime = 0;
  double framesRendered = 0;

  while (true) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Delta Time
    currentFrame = (float)glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    totalTime += deltaTime;
    framesRendered++;

    processInput(demoScene);
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

    if (demoScene.camera.hasUpdated()) renderer.updateUBO(demoScene);

    renderer.renderScene(demoScene);

    glfwSwapBuffers(window);
  }

  std::cout << "Average FPS: " << framesRendered / totalTime << std::endl;

  glfwTerminate();

  return;
}

GLFWwindow* Application::createWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_SAMPLES, 8);

  // Windowed
  // GLFWwindow* window = glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OpenGL
  // Demo", NULL, NULL);

  // Fullscreen
  GLFWwindow* windowPtr =
      glfwCreateWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "OpenGL Demo",
                       glfwGetPrimaryMonitor(), nullptr);

  if (windowPtr == nullptr) {
    std::cout << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return nullptr;
  }

  glfwMakeContextCurrent(windowPtr);

  // Vsync Off
  glfwSwapInterval(0);

  // Vsync On
  // glfwSwapInterval(1);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cout << "Failed to initialize GLAD" << std::endl;
    return nullptr;
  }

  return windowPtr;
}

void Application::createOpenGLViewport() {
  glViewport(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glEnable(GL_MULTISAMPLE);
  glEnable(GL_FRAMEBUFFER_SRGB);
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

  //Set mouse position before registering callback, to avoid mouse jump on first load
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

//Global callbacks for GLFW
void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
  glViewport(0, 0, width, height);
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

void keyCallback(GLFWwindow* window, int key, int scancode, int action,
                 int mods) {
  // Anything that needs to avoid key bouncing
}
