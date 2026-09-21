#pragma once

#include "Dependencies/glew.h"
#include <chrono>
#include <cstdint>
#include <cstdio>

// Single render-thread counters. Route new GL draw entry points through this profiler too.
class FrameProfiler
{
  public:
    static FrameProfiler& Instance()
    {
        static FrameProfiler profiler;
        return profiler;
    }

    void BeginFrame()
    {
        if (!started_)
        {
            intervalStart_ = Clock::now();
            started_ = true;
        }
        drawCalls_ = 0;
        inFrame_ = true;
    }

    static void DrawArrays(GLenum mode, GLint first, GLsizei count)
    {
        auto& profiler = Instance();
        if (profiler.inFrame_)
        {
            ++profiler.drawCalls_;
        }
        glDrawArrays(mode, first, count);
    }

    void EndFrame()
    {
        if (!inFrame_)
        {
            return;
        }
        inFrame_ = false;
        if (frames_ == 0 || drawCalls_ < minimum_)
        {
            minimum_ = drawCalls_;
        }
        if (frames_ == 0 || drawCalls_ > maximum_)
        {
            maximum_ = drawCalls_;
        }
        ++frames_;
        totalDrawCalls_ += drawCalls_;
        auto now = Clock::now();
        double seconds = std::chrono::duration<double>(now - intervalStart_).count();
        if (seconds < 1.0)
        {
            return;
        }
        // Wall-clock FPS includes frame pacing and swap waits, not GPU execution time.
        std::printf("[Render] FPS: %.1f | Draw calls/frame: %llu (avg %.1f, min %llu, max %llu)\n",
                    static_cast<double>(frames_) / seconds,
                    static_cast<unsigned long long>(drawCalls_),
                    static_cast<double>(totalDrawCalls_) / static_cast<double>(frames_),
                    static_cast<unsigned long long>(minimum_),
                    static_cast<unsigned long long>(maximum_));
        std::fflush(stdout);
        frames_ = 0;
        totalDrawCalls_ = 0;
        intervalStart_ = now;
    }

  private:
    using Clock = std::chrono::steady_clock;
    Clock::time_point intervalStart_{};
    std::uint64_t drawCalls_ = 0;
    std::uint64_t totalDrawCalls_ = 0;
    std::uint64_t frames_ = 0;
    std::uint64_t minimum_ = 0;
    std::uint64_t maximum_ = 0;
    bool started_ = false;
    bool inFrame_ = false;
};
