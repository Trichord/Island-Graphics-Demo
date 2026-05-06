#include "glfw_callbacks.h"

#include "gl_context.h"

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
    glViewport(0, 0, width, height);
}
