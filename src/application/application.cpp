#include "application.h"

#include <iostream>

#include "camera.h"
#include "character_physics.h"
#include "config_window.h"
#include "gl_context.h"
#include "glfw_callbacks.h"
#include "glm/glm.hpp"
#include "island_scene.h"
#include "renderer.h"
#include "scene.h"

void Application::run()
{
    ConfigWindow configWindow;
    ConfigWindow::GraphicsSettings graphicsSettings;

    if (!configWindow.launchSettingsWindow(graphicsSettings))
        return;

    createWindow(graphicsSettings);
    if (!window)
        return;

    createOpenGLViewport(graphicsSettings);
    registerCallbacks();

    // Create resources
    Renderer renderer(graphicsSettings);
    IslandScene islandScene(renderer.getTextureManager(), graphicsSettings);
    CharacterPhysics physicsHandler(islandScene);

    renderer.initializeResources(islandScene);
    renderer.allResourcesLoaded();

    addGlfwUserParam(islandScene);

    // Average FPS
    float totalTime{0};
    float framesRendered{0};

    while (!glfwWindowShouldClose(window))
    {
        // Delta Time
        currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Final FPS count
        totalTime += deltaTime;
        ++framesRendered;

        // Update scene objects
        islandScene.onUpdate(deltaTime);

        // Physics
        processKeyboard(islandScene, physicsHandler);

        // Draw
        renderer.renderScene(islandScene);

        glfwSwapBuffers(window);

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
        window = glfwCreateWindow(width, height, "Island Graphics Demo", glfwGetPrimaryMonitor(), nullptr);

    else
        window = glfwCreateWindow(width, height, "Island Graphics Demo", NULL, NULL);

    if (!window)
    {
        std::cout << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
    }

    glfwMakeContextCurrent(window);

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

void Application::processKeyboard(Scene &scene, CharacterPhysics &physicsHandler)
{
    using enum CharacterMovement;
    constexpr float physicsTick{1.0f / 60.0f};

    std::uint8_t direction{0};
    Camera &camera{scene.getCamera()};
    const glm::vec3 front{camera.getFront()};
    glm::vec3 previousPos{physicsHandler.getPreviousPosition()};

    accumulator += deltaTime;

    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        direction |= FORWARD;

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        direction |= BACKWARD;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        direction |= LEFT;

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        direction |= RIGHT;

    while (accumulator > physicsTick)
    {
        previousPos = physicsHandler.getPosition();
        physicsHandler.update(direction, front, physicsTick);
        accumulator -= physicsTick;
    }

    // Interpolate camera between last position and latest physics tick
    float tickDelta{accumulator / physicsTick};
    glm::vec3 interpolatedPos{glm::mix(previousPos, physicsHandler.getPosition(), tickDelta)};

    camera.setPosition(interpolatedPos);
}

void Application::mouse_callback(GLFWwindow *window, double xPos, double yPos)
{

    if (firstMouse)
    {
        lastMouseX = (float)xPos;
        lastMouseY = (float)yPos;
        firstMouse = false;
    }

    float xOffset{(float)(xPos - lastMouseX)};
    float yOffset{(float)(lastMouseY - yPos)};

    lastMouseX = (float)xPos;
    lastMouseY = (float)yPos;

    auto scenePointer = static_cast<IslandScene *>(glfwGetWindowUserPointer(window));

    if (scenePointer)
        scenePointer->getCamera().processMouseMovement(xOffset, yOffset);
}

void Application::registerCallbacks()
{
    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, Application::keyCallback);
    glfwSetCursorPosCallback(window, Application::mouse_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwSetWindowIconifyCallback(window, window_iconify_callback);
}

void Application::addGlfwUserParam(Scene &scene)
{
    glfwSetWindowUserPointer(window, &scene);
}

void Application::keyCallback(GLFWwindow *window, int key, int scancode, int action, int mods)
{
    // Anything that needs to avoid key bouncing
}