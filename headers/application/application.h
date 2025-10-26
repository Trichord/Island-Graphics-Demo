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
inline float lastMouseX{0.0f};
inline float lastMouseY{0.0f};

class Application {
 public:
  Application() = default;
  Application(const Application&) = delete;
  Application& operator=(const Application&) = delete;
  Application(Application&& other) = delete;
  Application& operator=(Application&& other) = delete;
  void run();

 private:
  float deltaTime{0.0f};
  float lastFrame{0.0f};
  float currentFrame{0.0f};
  GLFWwindow* window{nullptr};
  ConfigWindow configWindow;

  /**
   * Creates a GLFW window.  Display settings from the config window are passed
   * to GLFW here.
   */
  void createWindow();

  /**
   * Checks for GLFW key presses and passes info to the scene.
   * @param scene The target scene.
   */
  void processInput(Scene& scene);

  /**
   * Registers OpenGL function pointers, creates an OpenGL viewport, and
   * optionally sets MSAA.
   */
  void createOpenGLViewport();

  /**
   * Registers GLFW callbacks.  Mouse callback is seperate and should be
   * registered last with centerMouse()
   */
  void registerCallbacks();

  /**
   * Registers a scene with GLFW to be used with GLFW's built in call-backs.
   * @param scene The target scene to be processed by GLFW.
   */
  void addGlfwUserParam(Scene& scene);

  /**
   * Per-frame update logic.
   * @param scene The target scene to be updated.
   * @param deltaTime The time elapsed since the last frame.
   */
  void onUpdate(Scene& scene, float deltaTime);

  /**
   * Registers the mouse callback with GLFW.
   */
  void centerMouse();
};
