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
    UserInput userInput_;

    // Settings Window
    std::filesystem::path savePath{"./settings.json"};

    void createWindow(const GraphicsSettings &graphicsSettings);
    void processKeyboard(UserInput &input);
    bool createOpenGLViewport();
    void registerCallbacks();
    void registerJolt();
    void updateProgress(const std::string &message);

    // Required GLFW callbacks
    static void mouse_callback(GLFWwindow *window, double xPos, double yPos);
    static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
};

#ifdef _DEBUG
void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length,
                            const char *message, const void *userParam);
#endif
