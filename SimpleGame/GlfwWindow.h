#pragma once

struct GLFWwindow;

// Owns the native window/context. Keep this alive until renderer shutdown has completed.
class GlfwWindow
{
  public:
    GlfwWindow() = default;
    ~GlfwWindow();
    GlfwWindow(const GlfwWindow&) = delete;
    GlfwWindow& operator=(const GlfwWindow&) = delete;
    bool Initialize(int width, int height, const char* title, int swapInterval, bool timedFrames);
    GLFWwindow* Handle() const;

  private:
    GLFWwindow* window_ = nullptr;
    bool initialized_ = false;
    bool timerResolution_ = false;
};
