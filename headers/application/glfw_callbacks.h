#pragma once

#include "application/gl_context.h"

/**
 * @brief Callback handles iconfiying and restoring the application window
 */
void window_iconify_callback(GLFWwindow* window, int iconified);

/**
 * GLFW window resize callback. Receives the new size of the framebuffer
 *
 * @param window The window whose framebuffer was resized.
 * @param width The new width, in pixels, of the framebuffer.
 * @param height The new height, in pixels, of the framebuffer.
 */
void framebuffer_size_callback(GLFWwindow* window, int width, int height);