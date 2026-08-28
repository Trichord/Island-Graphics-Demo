#pragma once

#include <filesystem>

#include "application/gl_context.h"
#include "application/graphics_settings.h"
#include "application/input.h"

/**
 * @brief Handles window creation, user input, and the main loop.
 *
 * Also initializes OpenGL and passes settings from the config window to OpenGL.
 */
class Application
{
  public:
    Application() = default;
    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;
    Application(Application &&other) = delete;
    Application &operator=(Application &&other) = delete;
    ~Application();

    // For GLFW resize callbacks
    void setWindow(int width, int height);

    void run();

  private:
    // Frame timing
    float deltaTime_{0.0f};
    double lastFrame_{0.0f};
    double currentFrame_{0.0f};

    // Window
    GLFWwindow *window_{nullptr};
    int windowWidth_{1920};
    int windowHeight_{1080};
    bool windowResized_{false};

    // Input tracking
    bool leftCtrlToggle_{false};
    bool leftShiftToggle_{false};
    bool firstMouse_{true};
    float lastMouseX_{0.0f};
    float lastMouseY_{0.0f};
    float mouseSensitivity_{0.1f};
    Input input_;

    // Settings Window
    std::filesystem::path savePath{"./settings.json"};

    void createWindow(const GraphicsSettings &graphicsSettings);
    void processKeyboard(Input &input);

    /**
     * @brief Registers OpenGL function pointers, checks for supported features, and creates an OpenGL viewport
     */
    bool createOpenGLViewport();

    /**
     * @brief Registers GLFW callbacks.  Mouse callback is seperate and should be
     * registered last with centerMouse()
     */
    void registerCallbacks();

    void registerJolt();

    /**
     * GLFW mouse callback. Recieves updated cursor coordinates.
     *
     * @param window The window that recieved the event
     * @param xPos The new x-coordinate, in screen coordinates, of the cursor.
     * @param yPos The new y-coordinate, in screen coordinates, of the cursor.
     */
    static void mouse_callback(GLFWwindow *window, double xPos, double yPos);

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
    static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
};

#ifdef _DEBUG
void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length,
                            const char *message, const void *userParam);
#endif
