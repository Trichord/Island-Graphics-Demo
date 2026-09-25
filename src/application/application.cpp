#include "application/application.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/Memory.h>
#include <Jolt/registerTypes.h>
#include <iostream>

#include "application/gl_context.h"
#include "application/glfw_callbacks.h"
#include "application/graphics_settings.h"
#include "application/settings_window.h"
#include "game/game.h"
#include "renderer/buffers.h"
#include "renderer/textures.h"
#include "utilities/json_loader.h"

#ifdef _DEBUG
#include "application/gl_debug.h"
#endif

namespace
{
// Callback for GLFW
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));

    if (!app)
        return;

    app->setWindow(width, height);
}
} // namespace

Application::~Application()
{
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    glfwTerminate();
}

void Application::setWindow(int width, int height)
{
    windowWidth_ = width;
    windowHeight_ = height;
    windowResized_ = true;
}

void Application::run()
{
    SettingsWindow settingsWindow;
    GraphicsSettings graphicsSettings{jsonLoader::loadSettingsFromJSON(savePath)};

    if (!settingsWindow.launchSettingsWindow(graphicsSettings))
        return;

    jsonLoader::saveSettingsToJSON(graphicsSettings, savePath);

    createWindow(graphicsSettings);
    if (!window_)
        return;

    if (!createOpenGLViewport())
        return;

    registerCallbacks();
    registerJolt();

    // Game must have a valid OpenGL context with Jolt setup and ready to go. The callback passed to Game allows it to
    // update the console with messages and keep the window from being flagged as unresponsive.
    Game game{graphicsSettings, windowWidth_, windowHeight_, [this](const std::string &s) { updateProgress(s); }};
    glfwShowWindow(window_);

    while (!glfwWindowShouldClose(window_))
    {
        // Sleep if minimized
        while (glfwGetWindowAttrib(window_, GLFW_ICONIFIED) && !glfwWindowShouldClose(window_))
        {
            glfwWaitEvents();
            lastFrame_ = glfwGetTime();
        }

        // Delta Time
        currentFrame_ = glfwGetTime();
        deltaTime_ = static_cast<float>(currentFrame_ - lastFrame_);
        lastFrame_ = currentFrame_;
        const float time{static_cast<float>(currentFrame_)};

        // Fresh input context
        userInput_ = {};

        glfwPollEvents();
        processKeyboard(userInput_);

        if (windowResized_)
        {
            game.resizeExternalResolution(windowWidth_, windowHeight_);
            windowResized_ = false;
        }

        game.update(userInput_, time, deltaTime_);
        glfwSwapBuffers(window_);
    }
}

void Application::processKeyboard(UserInput &input)
{
    using enum UserInput::Direction;

    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window_, GLFW_TRUE);

    if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS)
        input.direction |= forward;

    if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS)
        input.direction |= backward;

    if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS)
        input.direction |= left;

    if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS)
        input.direction |= right;

    if (glfwGetKey(window_, GLFW_KEY_SPACE) == GLFW_PRESS)
        input.jump = true;

    userInput_.crouchToggle = leftCtrlToggle_;
    userInput_.sprintToggle = leftShiftToggle_;
}

void Application::mouse_callback(GLFWwindow *window, double xPos, double yPos)
{
    Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));

    if (!app)
        return;

    if (app->firstMouse_)
    {
        app->lastMouseX_ = static_cast<float>(xPos);
        app->lastMouseY_ = static_cast<float>(yPos);
        app->firstMouse_ = false;
    }

    float xOffset{static_cast<float>(xPos - app->lastMouseX_)};
    float yOffset{static_cast<float>(app->lastMouseY_ - yPos)};

    app->userInput_.mouseDeltaX = xOffset * app->mouseSensitivity_;
    app->userInput_.mouseDeltaY = yOffset * app->mouseSensitivity_;

    app->lastMouseX_ = static_cast<float>(xPos);
    app->lastMouseY_ = static_cast<float>(yPos);
}

void Application::registerCallbacks()
{
    glfwSetWindowUserPointer(window_, this);
    glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);
    glfwSetKeyCallback(window_, Application::keyCallback);
    glfwSetCursorPosCallback(window_, Application::mouse_callback);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void Application::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));

    if (!app)
        return;

    // Anything that needs to avoid key bouncing
    if (key == GLFW_KEY_LEFT_CONTROL && action == GLFW_PRESS)
        app->leftCtrlToggle_ = !app->leftCtrlToggle_;

    if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_PRESS)
        app->leftShiftToggle_ = !app->leftShiftToggle_;
}

void Application::createWindow(const GraphicsSettings &graphicsSettings)
{
    // Set the callback before trying to init, to capture any error messages
    glfwSetErrorCallback(glfw_error_callback);
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

#ifdef _DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif

    if (graphicsSettings.fullscreen)
    {
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);

        GLFWmonitor *monitor{glfwGetPrimaryMonitor()};
        const GLFWvidmode *mode{glfwGetVideoMode(monitor)};

        window_ = glfwCreateWindow(mode->width, mode->height, "Island Graphics Demo", nullptr, nullptr);
    }
    else
        window_ = glfwCreateWindow(graphicsSettings.renderWidth, graphicsSettings.renderHeight, "Island Graphics Demo",
                                   nullptr, nullptr);

    if (!window_)
    {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
    }

    // Safe to set now the window pointer check has passed
    if (graphicsSettings.fullscreen)
    {
        int monitorX{0};
        int monitorY{0};
        GLFWmonitor *monitor{glfwGetPrimaryMonitor()};
        glfwGetMonitorPos(monitor, &monitorX, &monitorY);
        glfwSetWindowPos(window_, monitorX, monitorY);
    }

    glfwMakeContextCurrent(window_);

    if (graphicsSettings.vsync)
        glfwSwapInterval(1);

    else
        glfwSwapInterval(0);

    // Get the actual framebuffer given by GLFW since it might not be the requested resolution
    glfwGetFramebufferSize(window_, &windowWidth_, &windowHeight_);
}

bool Application::createOpenGLViewport()
{
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Error: Failed to initialize GLAD\n";
        glfwTerminate();
        return false;
    }

    bool supported{true};

    GLsizei width{static_cast<GLsizei>(windowWidth_)};
    GLsizei height{static_cast<GLsizei>(windowHeight_)};

    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_PROGRAM_POINT_SIZE);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

#ifdef _DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(printGLError, nullptr);
    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
#endif

    // If MDI Count not supported
    if (!glMultiDrawElementsIndirectCount)
    {
        std::cerr << "GPU does not support required OpenGL features: MultiDrawElementsIndirectCount\n";
        supported = false;
    }

    GLint maxSSBOs{0};
    GLint maxSSBObindings{0};
    GLint maxVertexSSBOs{0};
    GLint maxFragmentSSBOs{0};
    GLint maxImageUnits{0};

    glGetIntegerv(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, &maxSSBOs);
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxSSBObindings);
    glGetIntegerv(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, &maxVertexSSBOs);
    glGetIntegerv(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, &maxFragmentSSBOs);
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &maxImageUnits);

    if (maxSSBOs < SSBOMinimum::combinedBlocks)
    {
        std::cerr << "Error: GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS (" << maxSSBOs << ") does not meet required ("
                  << SSBOMinimum::combinedBlocks << ")\n";
        supported = false;
    }

    if (maxSSBObindings < SSBOMinimum::combinedBindings)
    {
        std::cerr << "Error: GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS (" << maxSSBObindings << ") does not meet required ("
                  << SSBOMinimum::combinedBindings << ")\n";
        supported = false;
    }

    if (maxVertexSSBOs < SSBOMinimum::vertexBlocks)
    {
        std::cerr << "Error: GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS (" << maxVertexSSBOs << ") does not meet required ("
                  << SSBOMinimum::vertexBlocks << ")\n";
        supported = false;
    }

    if (maxFragmentSSBOs < SSBOMinimum::fragmentBlocks)
    {
        std::cerr << "Error: GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS (" << maxFragmentSSBOs
                  << ") does not meet required (" << SSBOMinimum::fragmentBlocks << ")\n";
        supported = false;
    }

    if (maxImageUnits < TextureUnit::max)
    {
        std::cerr << "Error: GL_MAX_TEXTURE_IMAGE_UNITS (" << maxImageUnits << ") does not meet required ("
                  << TextureUnit::max << ")\n";
        supported = false;
    }

    if (!supported)
        glfwTerminate();

    return supported;
}

void Application::registerJolt()
{
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
}

void Application::updateProgress(const std::string &message)
{
    std::cout << message;
    glfwPollEvents();
}