#include "application/glfw_callbacks.h"
#include "application/application.h"
#include "application/gl_context.h"

void window_iconify_callback(GLFWwindow *window, int iconified)
{
    if (iconified)
    {
        // minimized = true;
        glfwWaitEvents();
    }
    else
    {
        // minimized = false;
        glfwRestoreWindow(window);
    }
}

// Global callbacks for GLFW
void framebuffer_size_callback(GLFWwindow *window, int width, int height)
{
    Application *app = static_cast<Application *>(glfwGetWindowUserPointer(window));

    if (!app)
        return;

    app->setWindow(width, height);
}
