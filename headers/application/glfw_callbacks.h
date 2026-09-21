#pragma once

#include <GLFW/glfw3.h>
#include <iostream>

inline void window_iconify_callback(GLFWwindow *window, int iconified)
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

inline void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW Error " << error << ": " << description << "\n";
}