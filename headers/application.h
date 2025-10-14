#pragma once

//---------------------
//	Included glad.h, as this is the very first header used by main.
//  If it were not here, GLFW would trigger a glad.h include error.
//---------------------

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include "thread_pool.h"
#include "scene.h"
#include "config_window.h"

// GLFW callbacks
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xPos, double yPos);
void keyCallback(GLFWwindow* window, int key, int scancode, int action,
                 int mods);

inline ThreadPool threadPool;
inline float lastMouseX = 0.0f;
inline float lastMouseY = 0.0f;


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
  GLFWwindow* window = nullptr;
  ConfigWindow configWindow;

  void createWindow();
  void processInput(Scene& scene);
  void createOpenGLViewport();
  void registerCallbacks();
  void addGlfwUserParam(Scene& scene);
  void onUpdate(Scene& scene, float deltaTime);
};
