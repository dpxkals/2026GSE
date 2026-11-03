#include "stdafx.h"
#include "GlfwWindow.h"
#include <Windows.h>
#include <mmsystem.h>
#include <GLFW/glfw3.h>
#include <iostream>

GlfwWindow::~GlfwWindow()
{
    if (window_)
    {
        glfwDestroyWindow(window_);
    }
    if (initialized_)
    {
        glfwTerminate();
    }
    if (timerResolution_)
    {
        timeEndPeriod(1);
    }
}

bool GlfwWindow::Initialize(
    int width, int height, const char* title, int swapInterval, bool timedFrames)
{
    glfwSetErrorCallback(
        [](int code, const char* description)
        {
            std::cerr << "[GLFW] error=" << code << " " << description << '\n';
        });
    if (!glfwInit())
    {
        return false;
    }
    initialized_ = true;
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_RED_BITS, 8);
    glfwWindowHint(GLFW_GREEN_BITS, 8);
    glfwWindowHint(GLFW_BLUE_BITS, 8);
    glfwWindowHint(GLFW_ALPHA_BITS, 8);
    // The ordered 2.5D renderer does not use depth/stencil/MSAA window buffers.
    glfwWindowHint(GLFW_DEPTH_BITS, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_SAMPLES, 0);
    // Match the old framebuffer size on high-DPI displays; callbacks still handle scaling.
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_FALSE);
    window_ = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window_)
    {
        return false;
    }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(swapInterval);
    if (timedFrames)
    {
        // Avoid coarse Windows timer resolution turning the 16 ms event wait into ~31 ms.
        // This is session-scoped, not a per-frame call or a busy-wait.
        timerResolution_ = timeBeginPeriod(1) == TIMERR_NOERROR;
        if (!timerResolution_)
        {
            std::cerr << "[Window] 1 ms timer request failed; frame pacing may be less precise.\n";
        }
    }
    std::cout << "[Window] GLFW " << glfwGetVersionString()
              << " | OpenGL 3.3 core | swap_interval=" << swapInterval
              << " | timer_1ms=" << timerResolution_ << '\n';
    return true;
}

GLFWwindow* GlfwWindow::Handle() const
{
    return window_;
}
