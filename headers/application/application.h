#pragma once

#include <filesystem>
#include <string>

#include "application/gl_context.h"
#include "application/graphics_settings.h"
#include "application/input.h"

/**
 * @brief Verifies OpenGL capabilities, handles window creation, user input, and the main loop.
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
    double mouseX_{0.0f};
    double mouseY_{0.0f};
    float mouseSensitivity_{0.1f};
    UserInput userInput_;

    // Settings Window
    std::filesystem::path savePath{"./settings.json"};

    void createWindow(const GraphicsSettings &graphicsSettings);
    bool createOpenGLViewport();
    void registerCallbacks();
    void registerJolt();
    void updateProgress(const std::string &message);
    void processKeyboard(UserInput &input);
    void processMouse(UserInput &input);

    // Required GLFW callback
    static void keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods);
};