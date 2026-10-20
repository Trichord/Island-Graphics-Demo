#pragma once

#include <GLFW/glfw3.h>
#include <iostream>

inline void glfw_error_callback(int error, const char *description)
{
    std::cerr << "GLFW Error " << error << ": " << description << "\n";
}