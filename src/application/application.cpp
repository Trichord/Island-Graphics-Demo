#include "application.h"

#include <cstdint>
#include <iostream>

#include "jolt/Jolt.h"

#include "camera.h"
#include "config_window.h"
#include "gl_context.h"
#include "glfw_callbacks.h"
#include "glm/glm.hpp"
#include "island_scene.h"
#include "jolt/registerTypes.h"
#include "player_physics.h"
#include "renderer.h"
#include "scene.h"

#include <cstdarg>

constexpr size_t joltStackSize{10 * 1024 * 1024}; // 10 MB for Jolt's internal stack space

void Application::run()
{
    ConfigWindow configWindow;
    ConfigWindow::GraphicsSettings graphicsSettings;

    if (!configWindow.launchSettingsWindow(graphicsSettings))
        return;

    createWindow(graphicsSettings);
    if (!window_)
        return;

    createOpenGLViewport(graphicsSettings);
    registerCallbacks();
    initializeJolt();

    // Create resources
    Renderer renderer(graphicsSettings);
    IslandScene islandScene(renderer.getTextureManager(), graphicsSettings);
    JPH::PhysicsSystem physicsSystem;
    JPH::TempAllocatorImpl tempAllocator{joltStackSize};

    // Initialize scene dependent resources
    renderer.initializeResources(islandScene);
    renderer.allResourcesLoaded();
    PlayerPhysics physicsHandler(islandScene, physicsSystem, tempAllocator);

    addGlfwUserParam(islandScene);

    // Average FPS
    float totalTime{0};
    unsigned long long framesRendered{0};

    while (!glfwWindowShouldClose(window_))
    {
        // Delta Time
        currentFrame_ = (float)glfwGetTime();
        deltaTime_ = currentFrame_ - lastFrame_;
        lastFrame_ = currentFrame_;

        // Final FPS count
        totalTime += deltaTime_;
        ++framesRendered;

        // Update scene objects
        islandScene.onUpdate(deltaTime_);

        // Physics
        processKeyboard(islandScene, physicsHandler);

        // Draw
        renderer.renderScene(islandScene);

        glfwSwapBuffers(window_);

        // User input
        glfwPollEvents();
    }

    glfwTerminate();
    std::cout << "Average FPS: " << framesRendered / totalTime << std::endl;
    return;
}

void Application::createWindow(const ConfigWindow::GraphicsSettings &graphicsSettings)
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 32);

    if (graphicsSettings.msaa)
        glfwWindowHint(GLFW_SAMPLES, graphicsSettings.msaa);

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

void Application::createOpenGLViewport(const ConfigWindow::GraphicsSettings &graphicsSettings)
{
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        std::cout << "Failed to initialize GLAD" << std::endl;
        glfwTerminate();
        return;
    }

    GLsizei width{static_cast<GLsizei>(graphicsSettings.screenWidth)};
    GLsizei height{static_cast<GLsizei>(graphicsSettings.screenHeight)};

    glViewport(0, 0, width, height);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void Application::processKeyboard(Scene &scene, PlayerPhysics &physicsHandler)
{
    using enum CharacterMovement;
    constexpr float objectSpeed{0.5f};

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

    /*
    //****************************************
    // Object Positioning - For Testing Only *
    //****************************************
    Transform &transform{scene.getObjectTransform("spaceModel")};

    // +- Z
    if (glfwGetKey(window, GLFW_KEY_KP_8) == GLFW_PRESS)
        transform.setRelativePosition(glm::vec3{0.0f, 0.0f, -objectSpeed * deltaTime});

    if (glfwGetKey(window, GLFW_KEY_KP_5) == GLFW_PRESS)
        transform.setRelativePosition(glm::vec3{0.0f, 0.0f, objectSpeed * deltaTime});

    // +- X
    if (glfwGetKey(window, GLFW_KEY_KP_4) == GLFW_PRESS)
        transform.setRelativePosition(glm::vec3{-objectSpeed * deltaTime, -0.0f, 0.0f});

    if (glfwGetKey(window, GLFW_KEY_KP_6) == GLFW_PRESS)
        transform.setRelativePosition(glm::vec3{objectSpeed * deltaTime, 0.0f, 0.0f});

    // +- Y
    if (glfwGetKey(window, GLFW_KEY_KP_7) == GLFW_PRESS)
        transform.setRelativePosition(glm::vec3{0.0f, -objectSpeed * deltaTime, 0.0f});

    if (glfwGetKey(window, GLFW_KEY_KP_9) == GLFW_PRESS)
        transform.setRelativePosition(glm::vec3{0.0f, objectSpeed * deltaTime, 0.0f});

    if (glfwGetKey(window, GLFW_KEY_P) == GLFW_PRESS)
    {
        const glm::vec3 &pos{transform.getPosition()};
        std::cout << "Transform at: (" << pos.x << ", " << pos.y << ", " << pos.z << ")" << std::endl;
    }
    */
}

void Application::mouse_callback(GLFWwindow *window, double xPos, double yPos)
{

    if (firstMouse_)
    {
        lastMouseX_ = (float)xPos;
        lastMouseY_ = (float)yPos;
        firstMouse_ = false;
    }

    float xOffset{(float)(xPos - lastMouseX_)};
    float yOffset{(float)(lastMouseY_ - yPos)};

    lastMouseX_ = (float)xPos;
    lastMouseY_ = (float)yPos;

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

void Application::initializeJolt()
{
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
}