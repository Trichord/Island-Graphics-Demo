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

Application::~Application()
{
    // Cleanup Jolt Physics
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

#ifndef _DEBUG
    registerCallbacks();
#endif

    registerJolt();

    Game game{graphicsSettings, windowWidth_, windowHeight_};

    while (!glfwWindowShouldClose(window_))
    {
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
    glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);
    glfwSetKeyCallback(window_, Application::keyCallback);
    glfwSetCursorPosCallback(window_, Application::mouse_callback);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowIconifyCallback(window_, window_iconify_callback);
    glfwSetWindowUserPointer(window_, this);
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
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

#ifdef _DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif

    if (graphicsSettings.fullscreen)
    {
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        const GLFWvidmode *mode{glfwGetVideoMode(glfwGetPrimaryMonitor())};
        window_ = glfwCreateWindow(mode->width, mode->height, "Island Graphics Demo", glfwGetPrimaryMonitor(), nullptr);
    }
    else
        window_ = glfwCreateWindow(graphicsSettings.renderWidth, graphicsSettings.renderHeight, "Island Graphics Demo",
                                   nullptr, nullptr);

    if (!window_)
    {
        std::cout << "Failed to create GLFW window\n";
        glfwTerminate();
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
        std::cout << "ERROR: Failed to initialize GLAD\n";
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
    glDebugMessageCallback(glDebugOutput, nullptr);
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
        std::cerr << "ERROR: GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS (" << maxSSBOs << ") does not meet required ("
                  << SSBOMinimum::combinedBlocks << ")\n";
        supported = false;
    }

    if (maxSSBObindings < SSBOMinimum::combinedBindings)
    {
        std::cerr << "ERROR: GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS (" << maxSSBObindings << ") does not meet required ("
                  << SSBOMinimum::combinedBindings << ")\n";
        supported = false;
    }

    if (maxVertexSSBOs < SSBOMinimum::vertexBlocks)
    {
        std::cerr << "ERROR: GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS (" << maxVertexSSBOs << ") does not meet required ("
                  << SSBOMinimum::vertexBlocks << ")\n";
        supported = false;
    }

    if (maxFragmentSSBOs < SSBOMinimum::fragmentBlocks)
    {
        std::cerr << "ERROR: GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS (" << maxFragmentSSBOs
                  << ") does not meet required (" << SSBOMinimum::fragmentBlocks << ")\n";
        supported = false;
    }

    if (maxImageUnits < TextureUnit::max)
    {
        std::cerr << "ERROR: GGL_MAX_TEXTURE_IMAGE_UNITS (" << maxImageUnits << ") does not meet required ("
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

#ifdef _DEBUG
void APIENTRY glDebugOutput(GLenum source, GLenum type, unsigned int id, GLenum severity, GLsizei length,
                            const char *message, const void *userParam)
{
    // Low priority messages
    if (id == 131169 || id == 131185 || id == 131218 || id == 131204)
        return;

    std::cout << "Debug message (" << id << "): " << message << "\n";

    switch (source)
    {
    case GL_DEBUG_SOURCE_API:
        std::cout << "Source: API";
        break;
    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        std::cout << "Source: Window System";
        break;
    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        std::cout << "Source: Shader Compiler";
        break;
    case GL_DEBUG_SOURCE_THIRD_PARTY:
        std::cout << "Source: Third Party";
        break;
    case GL_DEBUG_SOURCE_APPLICATION:
        std::cout << "Source: Application";
        break;
    case GL_DEBUG_SOURCE_OTHER:
        std::cout << "Source: Other";
        break;
    }
    std::cout << "\n";

    switch (type)
    {
    case GL_DEBUG_TYPE_ERROR:
        std::cout << "Type: Error";
        break;
    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        std::cout << "Type: Deprecated Behaviour";
        break;
    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        std::cout << "Type: Undefined Behaviour";
        break;
    case GL_DEBUG_TYPE_PORTABILITY:
        std::cout << "Type: Portability";
        break;
    case GL_DEBUG_TYPE_PERFORMANCE:
        std::cout << "Type: Performance";
        break;
    case GL_DEBUG_TYPE_MARKER:
        std::cout << "Type: Marker";
        break;
    case GL_DEBUG_TYPE_PUSH_GROUP:
        std::cout << "Type: Push Group";
        break;
    case GL_DEBUG_TYPE_POP_GROUP:
        std::cout << "Type: Pop Group";
        break;
    case GL_DEBUG_TYPE_OTHER:
        std::cout << "Type: Other";
        break;
    }
    std::cout << "\n";

    switch (severity)
    {
    case GL_DEBUG_SEVERITY_HIGH:
        std::cout << "Severity: high";
        break;
    case GL_DEBUG_SEVERITY_MEDIUM:
        std::cout << "Severity: medium";
        break;
    case GL_DEBUG_SEVERITY_LOW:
        std::cout << "Severity: low";
        break;
    case GL_DEBUG_SEVERITY_NOTIFICATION:
        std::cout << "Severity: notification";
        break;
    }
    std::cout << "\n\n";
}
#endif