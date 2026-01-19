#pragma once

/**
 * Include order matters. GLAD must be included first or GLFW may
 * include <GL/gl.h>, causing GLAD throw a "header already included" error.
 */

#include "glad/glad.h"
#include "GLFW/glfw3.h"