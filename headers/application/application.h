#pragma once

/* Included glad.h, as this is the very first header used by main. If it were
 * not here, GLFW would trigger a glad.h include error.
 */

#include "config_window.h"
#include "gl_context.h"
#include "scene.h"

/**
 * GLFW window resize callback. Receives the new size of the framebuffer
 *
 * @param window The window whose framebuffer was resized.
 * @param width The new width, in pixels, of the framebuffer.
 * @param height The new height, in pixels, of the framebuffer.
 */
void framebuffer_size_callback(GLFWwindow* window, int width, int height);

/**
 * GLFW mouse callback. Recieves updated cursor coordinates.
 *
 * @param window The window that recieved the event
 * @param xPos The new x-coordinate, in screen coordinates, of the cursor.
 * @param yPos The new y-coordinate, in screen coordinates, of the cursor.
 */
void mouse_callback(GLFWwindow* window, double xPos, double yPos);

/**
 * GLFW key callback, triggered when a keyboard button is pressed, repeated or
 * released.
 *
 * @param window The window that received the event.
 * @param key The keyboard key that was pressed or released.
 * @param scancode The system-specific scancode of the key.
 * @param action The button action, such as GLFW_PRESS, GLFW_RELEASE or
 * GLFW_REPEAT
 * @param mods Bit field describing which modifier keys were held down.
 */
void keyCallback(GLFWwindow* window, int key, int scancode, int action,
                 int mods);

inline float lastMouseX{0.0f};
inline float lastMouseY{0.0f};

/**
 * @brief Handles window creation, user input, and the main loop.
 *
 * Also initializes OpenGL and passes settings from the config window to OpenGL.
 */
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

  /**
   * @brief Creates a GLFW window.  Display settings from the config window are
   * passed to GLFW here.
   */
  void createWindow(const ConfigWindow::GraphicsSettings& graphicsSettings);

  /**
   * @brief Checks for GLFW key presses and passes info to the scene.
   *
   * @param scene The target scene.
   */
  void processInput(Scene& scene);

  /**
   * @brief Registers OpenGL function pointers, creates an OpenGL viewport, and
   * optionally sets MSAA.
   */
  void createOpenGLViewport(
      const ConfigWindow::GraphicsSettings& graphicsSettings);

  /**
   * @brief Registers GLFW callbacks.  Mouse callback is seperate and should be
   * registered last with centerMouse()
   */
  void registerCallbacks();

  /**
   * @brief Registers a scene with GLFW to be used with GLFW's built in
   * call-backs.
   *
   * @param scene The target scene to be processed by GLFW.
   */
  void addGlfwUserParam(Scene& scene);

  /**
   * @brief Per-frame update logic.
   *
   * @param scene The target scene to be updated.
   * @param deltaTime The time elapsed since the last frame.
   */
  void onUpdate(Scene& scene);
};
