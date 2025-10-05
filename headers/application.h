#pragma once

#include <GLFW/glfw3.h>
#include "thread_pool.h"
#include "scene.h"

void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xPos, double yPos);
void keyCallback(GLFWwindow* window, int key, int scancode, int action,
                 int mods);

#define SCREEN_WIDTH 2560
#define SCREEN_HEIGHT 1440

inline ThreadPool threadPool;
inline float lastMouseX = SCREEN_WIDTH / 2;
inline float lastMouseY = SCREEN_HEIGHT / 2;

class Application {
 public:
  Application() = default;
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&& other) = delete;
  Application& operator=(Application&& other) = delete;
  void run();

 private:
  float deltaTime = 0.0f;
  float lastFrame = 0.0f;
  float currentFrame = 0.0f;
  GLFWwindow* window;
  
  GLFWwindow* createWindow();
  void processInput(Scene& scene);
  void createOpenGLViewport();
  void registerCallbacks();
  void configureGLFW(Scene& scene);
};
