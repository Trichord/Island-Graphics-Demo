#include "application/application.h"

#include <algorithm>
#include <cstdint>
#include <glm/glm.hpp>
#include <iostream>
#include <jolt/Core/Factory.h>
#include <jolt/Core/JobSystemSingleThreaded.h>
#include <jolt/Core/Memory.h>
#include <jolt/Core/TempAllocator.h>
#include <jolt/Physics/PhysicsSystem.h>
#include <jolt/registerTypes.h>
#include <thread>

#include "application/gl_context.h"
#include "application/glfw_callbacks.h"
#include "application/graphics_settings.h"
#include "application/settings_window.h"
#include "physics/player_physics.h"
#include "renderer/buffers.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "scene/island_scene/island_scene.h"
#include "scene/scene.h"
#include "utilities/json_loader.h"
#include "utilities/texture_manager.h"
#include "utilities/thread_pool.h"

Application::~Application()
{
    using namespace JPH;

    // Cleanup Jolt Physics
    delete Factory::sInstance;
    Factory::sInstance = nullptr;

    glfwTerminate();
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

    if (!createOpenGLViewport(graphicsSettings))
        return;

#ifndef _DEBUG
    registerCallbacks();
#endif

    // Jolt Physics
    registerJolt();
    JPH::PhysicsSystem physicsSystem;
    JPH::TempAllocatorImpl tempAllocator{joltStackSize};
    JPH::JobSystemSingleThreaded jobSystem_{maxPhysicsJobs_};
    physicsSystem.Init(maxBodies, 0, maxBodyPairs, maxContactConstraints, broadPhaseLayer_,
                       objectVsBroadPhaseLayerFilter_, objectLayerPairFilter_);

    // Create resources
    ThreadPool threadPool{std::max(2u, std::thread::hardware_concurrency() / 2u)};
    TextureManager textureManager{threadPool, graphicsSettings};
    Renderer renderer(textureManager, graphicsSettings);

    std::cout << "Loading Models... ";
    IslandScene islandScene(textureManager, graphicsSettings, "./res/scenes/island_scene.json");
    std::cout << "Done" << std::endl;

    // Initialize scene dependent resources
    renderer.initializeResources(islandScene);

    std::cout << "Creating Collision Mesh... ";
    PlayerPhysics physicsHandler(islandScene, physicsSystem, tempAllocator);
    std::cout << "Done" << std::endl;

    threadPool.stopAllThreads();
    addGlfwUserParam(islandScene);

    while (!glfwWindowShouldClose(window_))
    {
        // Delta Time
        currentFrame_ = (float)glfwGetTime();
        deltaTime_ = currentFrame_ - lastFrame_;
        lastFrame_ = currentFrame_;

        // Update
        islandScene.onUpdate(currentFrame_, deltaTime_);

        glfwPollEvents();
        processKeyboard(islandScene, physicsHandler);

        // Draw
        renderer.renderScene(islandScene, deltaTime_);
        glfwSwapBuffers(window_);
    }

    return;
}

void Application::createWindow(const GraphicsSettings &graphicsSettings)
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);

#ifdef _DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, true);
#endif

    int width{graphicsSettings.screenWidth};
    int height{graphicsSettings.screenHeight};

    if (graphicsSettings.fullscreen)
        window_ = glfwCreateWindow(width, height, "Island Graphics Demo", glfwGetPrimaryMonitor(), nullptr);

    else
        window_ = glfwCreateWindow(width, height, "Island Graphics Demo", NULL, NULL);

    if (!window_)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
    }

    glfwMakeContextCurrent(window_);

    if (graphicsSettings.vsync)
        glfwSwapInterval(1);

    else
        glfwSwapInterval(0);
}

bool Application::createOpenGLViewport(const GraphicsSettings &graphicsSettings)
{
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "ERROR: Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return false;
    }

    bool supported{true};

    GLsizei width{static_cast<GLsizei>(graphicsSettings.screenWidth)};
    GLsizei height{static_cast<GLsizei>(graphicsSettings.screenHeight)};

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
        std::cerr << "GPU does not support required OpenGL features: MultiDrawElementsIndirectCount" << std::endl;
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
                  << SSBOMinimum::combinedBlocks << ")" << std::endl;
        supported = false;
    }

    if (maxSSBObindings < SSBOMinimum::combinedBindings)
    {
        std::cerr << "ERROR: GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS (" << maxSSBObindings << ") does not meet required ("
                  << SSBOMinimum::combinedBindings << ")" << std::endl;
        supported = false;
    }

    if (maxVertexSSBOs < SSBOMinimum::vertexBlocks)
    {
        std::cerr << "ERROR: GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS (" << maxVertexSSBOs << ") does not meet required ("
                  << SSBOMinimum::vertexBlocks << ")" << std::endl;
        supported = false;
    }

    if (maxFragmentSSBOs < SSBOMinimum::fragmentBlocks)
    {
        std::cerr << "ERROR: GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS (" << maxFragmentSSBOs
                  << ") does not meet required (" << SSBOMinimum::fragmentBlocks << ")" << std::endl;
        supported = false;
    }

    if (maxImageUnits < TextureUnit::max)
    {
        std::cerr << "ERROR: GGL_MAX_TEXTURE_IMAGE_UNITS (" << maxImageUnits << ") does not meet required ("
                  << TextureUnit::max << ")" << std::endl;
        supported = false;
    }

    if (!supported)
        glfwTerminate();

    return supported;
}

void Application::processKeyboard(Scene &scene, PlayerPhysics &physicsHandler)
{
    using enum CharacterMovement;

    std::uint8_t direction{0};
    bool jump{false};

    Camera &camera{scene.getCamera()};
    const glm::vec3 front{camera.getFront()};

    glm::vec3 previousPos{physicsHandler.getPreviousPosition()};

    accumulator_ += deltaTime_;

    if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window_, GLFW_TRUE);
        return;
    }

    if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS)
        direction |= FORWARD;

    if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS)
        direction |= BACKWARD;

    if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS)
        direction |= LEFT;

    if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS)
        direction |= RIGHT;

    if (glfwGetKey(window_, GLFW_KEY_SPACE) == GLFW_PRESS)
        jump = true;

    physicsHandler.toggleCrouch(leftCtrlToggle_);
    physicsHandler.toggleSprint(leftShiftToggle_);

    while (accumulator_ > physicsTick)
    {
        previousPos = physicsHandler.getPosition();
        physicsHandler.update(direction, front, jump, leftCtrlToggle_, physicsTick);
        accumulator_ -= physicsTick;
    }

    // Interpolate camera between last position and latest physics tick (Glenn Fiedler)
    float alpha{accumulator_ / physicsTick};
    glm::vec3 interpolatedPos{glm::mix(previousPos, physicsHandler.getPosition(), alpha)};

    camera.setPosition(interpolatedPos);
}

void Application::mouse_callback(GLFWwindow *window, double xPos, double yPos)
{
    if (firstMouse_)
    {
        lastMouseX_ = static_cast<float>(xPos);
        lastMouseY_ = static_cast<float>(yPos);
        firstMouse_ = false;
    }

    float xOffset{static_cast<float>(xPos - lastMouseX_)};
    float yOffset{static_cast<float>(lastMouseY_ - yPos)};

    lastMouseX_ = static_cast<float>(xPos);
    lastMouseY_ = static_cast<float>(yPos);

    auto scenePointer = static_cast<IslandScene *>(glfwGetWindowUserPointer(window));

    if (scenePointer)
        scenePointer->getCamera().processMouseMovement(xOffset, yOffset);
}

void Application::registerCallbacks()
{
    glfwSetFramebufferSizeCallback(window_, framebuffer_size_callback);
    glfwSetKeyCallback(window_, Application::keyCallback);
    glfwSetCursorPosCallback(window_, Application::mouse_callback);
    glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowIconifyCallback(window_, window_iconify_callback);
}

void Application::addGlfwUserParam(Scene &scene)
{
    glfwSetWindowUserPointer(window_, &scene);
}

void Application::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    // Anything that needs to avoid key bouncing
    if (key == GLFW_KEY_LEFT_CONTROL && action == GLFW_PRESS)
        leftCtrlToggle_ = !leftCtrlToggle_;

    if (key == GLFW_KEY_LEFT_SHIFT && action == GLFW_PRESS)
        leftShiftToggle_ = !leftShiftToggle_;
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

    std::cout << "Debug message (" << id << "): " << message << std::endl;

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
    std::cout << std::endl;

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
    std::cout << std::endl;

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
    std::cout << std::endl << std::endl;
}
#endif