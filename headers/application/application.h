#pragma once

#include <filesystem>

#include <jolt/Jolt.h>

#include <jolt/Core/Core.h>

#include "application/gl_context.h"
#include "application/graphics_settings.h"
#include "physics/jolt_impl.h"
#include "physics/player_physics.h"
#include "scene/scene.h"

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

    static constexpr float physicsTick{1.0f / 60.0f};

  private:
    // Frame timing
    float deltaTime_{0.0f};
    float lastFrame_{0.0f};
    float currentFrame_{0.0f};
    float accumulator_{0.0f};

    // Window
    GLFWwindow *window_{nullptr};
    int windowWidth_{1920};
    int windowHeight_{1080};
    bool windowResized_{false};

    // Input tracking
    static inline bool leftCtrlToggle_{false};
    static inline bool leftShiftToggle_{false};
    static inline bool firstMouse_{true};
    static inline float lastMouseX_{0.0f};
    static inline float lastMouseY_{0.0f};

    // Settings Window
    std::filesystem::path savePath{"./settings.json"};

    // Physics engine constants
    static constexpr JPH::uint maxBodies{1024};
    static constexpr JPH::uint maxBodyPairs{1024};
    static constexpr JPH::uint maxPhysicsJobs_{2048};
    static constexpr JPH::uint maxContactConstraints{1024};
    static constexpr size_t joltStackSize{10 * 1024 * 1024}; // 10 MB

    // Jolt Physics
    BPLayerInterfaceImpl broadPhaseLayer_;
    ObjectLayerPairFilterImpl objectLayerPairFilter_;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter_;

    // Scene pointer, for GLFW callbacks
    Scene *scenePtr_{nullptr};

    /**
     * @brief Creates a GLFW window.  Display settings from the config window are
     * passed to GLFW here.
     */
    void createWindow(const GraphicsSettings &graphicsSettings);

    /**
     * @brief Checks for GLFW key presses and passes info to the scene.
     *
     * @param scene The target scene.
     */
    void processKeyboard(Scene &scene, PlayerPhysics &physicsHandler);

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
