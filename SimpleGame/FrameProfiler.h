#pragma once

#include "Dependencies/glew.h"
#include <array>
#include <algorithm>
#include <utility>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <string>

// Render-thread only. CPU timings are wall-clock timings, never GPU timings.
class FrameProfiler
{
  public:
    enum Metric
    {
        Draws,
        InstancedDraws,
        Instances,
        SubmittedVertices,
        UploadBytes,
        MeshRequests,
        MemoryHits,
        DiskHits,
        CacheMisses,
        MeshBuilds,
        GeneratedVertices,
        DiskReadBytes,
        DiskWriteBytes,
        CacheErrors,
        BatchVertices,
        Evictions,
        SyncMs,
        SubmitMs,
        SwapMs,
        CacheIoMs,
        MeshBuildMs,
        GroundDraws,
        OverlayDraws,
        WorldDraws,
        EffectsDraws,
        PostDraws,
        UiDraws,
        MeshCount,
        MeshCpuBytes,
        MeshGpuBytes,
        Actors,
        Chunks,
        PendingChunks,
        Width,
        Height,
        Zoom,
        PostEnabled,
        BloomEnabled,
        EdgeBlurEnabled,
        VignetteEnabled,
        RenderMs,
        Count
    };

    static FrameProfiler& Instance()
    {
        static FrameProfiler profiler;
        return profiler;
    }

    void OpenLog(const std::filesystem::path& directory, const std::string& context = {})
    {
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
        auto path = directory / ("render-" + std::to_string(stamp) + ".csv");
        if (!error)
        {
            log_.open(path);
        }
        if (!log_)
        {
            std::fprintf(stderr, "[Profile] Cannot open CSV; console logging remains enabled.\n");
            return;
        }
        log_ << "# schema=2; timings=CPU wall ms; samples=completed display callbacks\n";
        log_ << "# context=" << context << '\n';
#ifdef _DEBUG
        log_ << "# configuration=Debug\n";
#else
        log_ << "# configuration=Release\n";
#endif
        for (auto entry : {std::pair<GLenum, const char*>{GL_VENDOR, "vendor"},
                           {GL_RENDERER, "renderer"},
                           {GL_VERSION, "gl"}})
        {
            const auto* value = glGetString(entry.first);
            log_ << "# " << entry.second << "="
                 << (value ? reinterpret_cast<const char*>(value) : "unknown") << '\n';
        }
        log_ << "# build=" << __DATE__ << " " << __TIME__ << '\n';
        log_ << "elapsed_s,frames,interval_s,fps,frame_ms_avg,frame_ms_max";
        for (const char* name : Names())
        {
            log_ << ',' << name << "_last," << name << "_avg," << name << "_max";
        }
        log_ << '\n';
        auto eventPath = directory / ("render-" + std::to_string(stamp) + "-events.csv");
        events_.open(eventPath);
        if (events_)
        {
            events_ << "unix_us,event,key,vertices,bytes\n";
        }
        else
        {
            std::fprintf(stderr, "[Profile] Cannot open cache event CSV.\n");
        }
        std::printf("[Profile] CSV session: render-%lld.csv\n", static_cast<long long>(stamp));
    }

    void Event(const char* event, const std::string& key, size_t vertices = 0, size_t bytes = 0)
    {
        if (!events_)
        {
            return;
        }
        auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                         std::chrono::system_clock::now().time_since_epoch())
                         .count();
        std::string escaped;
        for (char c : key)
        {
            if (c == '"')
            {
                escaped += '"';
            }
            escaped += c;
        }
        events_ << stamp << ',' << event << ",\"" << escaped << "\"," << vertices << ',' << bytes
                << '\n';
    }

    void Add(Metric metric, double value = 1.0)
    {
        if (inFrame_)
        {
            current_[metric] += value;
        }
    }

    void Set(Metric metric, double value)
    {
        if (inFrame_)
        {
            current_[metric] = value;
        }
    }

    void SetPass(Metric pass)
    {
        pass_ = pass;
    }

    class Scope
    {
      public:
        explicit Scope(Metric metric)
            : metric_(metric),
              start_(Clock::now())
        {
        }

        ~Scope()
        {
            Instance().Add(
                metric_, std::chrono::duration<double, std::milli>(Clock::now() - start_).count());
        }

      private:
        using Clock = std::chrono::steady_clock;
        Metric metric_;
        Clock::time_point start_;
    };

    void BeginFrame()
    {
        auto now = Clock::now();
        if (!started_)
        {
            intervalStart_ = sessionStart_ = previousEnd_ = now;
            started_ = true;
        }
        frameStart_ = now;
        current_.fill(0.0);
        inFrame_ = true;
    }

    static void DrawArrays(GLenum mode, GLint first, GLsizei count)
    {
        auto& profiler = Instance();
        profiler.Add(Draws);
        profiler.Add(profiler.pass_);
        profiler.Add(SubmittedVertices, count);
        glDrawArrays(mode, first, count);
    }

    static void DrawArraysInstanced(GLenum mode, GLint first, GLsizei count, GLsizei instances)
    {
        auto& profiler = Instance();
        profiler.Add(Draws);
        profiler.Add(profiler.pass_);
        profiler.Add(InstancedDraws);
        profiler.Add(Instances, instances);
        profiler.Add(SubmittedVertices, double(count) * instances);
        glDrawArraysInstanced(mode, first, count, instances);
    }

    void EndFrame()
    {
        if (!inFrame_)
        {
            return;
        }
        auto now = Clock::now();
        Set(RenderMs, std::chrono::duration<double, std::milli>(now - frameStart_).count());
        double frameMs = std::chrono::duration<double, std::milli>(now - previousEnd_).count();
        previousEnd_ = now;
        frameMaxMs_ = (std::max)(frameMaxMs_, frameMs);
        inFrame_ = false;
        for (size_t i = 0; i < Count; ++i)
        {
            total_[i] += current_[i];
            maximum_[i] = (std::max)(maximum_[i], current_[i]);
        }
        if (!frames_ || current_[Draws] < minimumDraws_)
        {
            minimumDraws_ = current_[Draws];
        }
        ++frames_;
        double seconds = std::chrono::duration<double>(now - intervalStart_).count();
        if (seconds < 1.0)
        {
            return;
        }
        double fps = frames_ / seconds;
        std::printf("[Render] FPS: %.1f | Draw calls/frame: %.0f (avg %.1f, min %.0f, max %.0f)\n",
                    fps,
                    current_[Draws],
                    total_[Draws] / frames_,
                    minimumDraws_,
                    maximum_[Draws]);
        std::printf("[Profile] avg/frame: instances=%.1f upload=%.1f KB generated=%.1f verts "
                    "cache[mem/disk/miss]=%.1f/%.1f/%.1f | CPU render=%.2f ms swap=%.2f ms "
                    "| frame max=%.2f ms\n",
                    total_[Instances] / frames_,
                    total_[UploadBytes] / frames_ / 1024.0,
                    total_[GeneratedVertices] / frames_,
                    total_[MemoryHits] / frames_,
                    total_[DiskHits] / frames_,
                    total_[CacheMisses] / frames_,
                    total_[RenderMs] / frames_,
                    total_[SwapMs] / frames_,
                    frameMaxMs_);
        if (log_)
        {
            log_ << std::fixed << std::setprecision(4)
                 << std::chrono::duration<double>(now - sessionStart_).count() << ',' << frames_
                 << ',' << seconds << ',' << fps << ',' << seconds * 1000.0 / frames_ << ','
                 << frameMaxMs_;
            for (size_t i = 0; i < Count; ++i)
            {
                log_ << ',' << current_[i] << ',' << total_[i] / frames_ << ',' << maximum_[i];
            }
            log_ << '\n';
            log_.flush();
            events_.flush();
            if (!log_)
            {
                std::fprintf(stderr, "[Profile] CSV write failed.\n");
            }
        }
        std::fflush(stdout);
        frames_ = 0;
        frameMaxMs_ = 0;
        total_.fill(0);
        maximum_.fill(0);
        intervalStart_ = now;
    }

  private:
    static const std::array<const char*, Count>& Names()
    {
        static const std::array<const char*, Count> names = {"draws",
                                                             "instanced_draws",
                                                             "instances",
                                                             "submitted_vertices",
                                                             "upload_bytes",
                                                             "mesh_requests",
                                                             "memory_hits",
                                                             "disk_hits",
                                                             "cache_misses",
                                                             "mesh_builds",
                                                             "generated_vertices",
                                                             "disk_read_bytes",
                                                             "disk_write_bytes",
                                                             "cache_errors",
                                                             "batch_vertices",
                                                             "evictions",
                                                             "sync_ms",
                                                             "submit_ms",
                                                             "swap_ms",
                                                             "cache_io_ms",
                                                             "mesh_build_ms",
                                                             "ground_draws",
                                                             "overlay_draws",
                                                             "world_draws",
                                                             "effects_draws",
                                                             "post_draws",
                                                             "ui_draws",
                                                             "mesh_count",
                                                             "mesh_cpu_bytes",
                                                             "mesh_gpu_bytes",
                                                             "actors",
                                                             "chunks",
                                                             "pending_chunks",
                                                             "width",
                                                             "height",
                                                             "zoom",
                                                             "post_enabled",
                                                             "bloom_enabled",
                                                             "edge_blur_enabled",
                                                             "vignette_enabled",
                                                             "render_ms"};
        return names;
    }

    using Clock = std::chrono::steady_clock;
    Clock::time_point intervalStart_{}, sessionStart_{}, previousEnd_{}, frameStart_{};
    std::array<double, Count> current_{}, total_{}, maximum_{};
    std::ofstream log_;
    std::ofstream events_;
    std::uint64_t frames_ = 0;
    double minimumDraws_ = 0, frameMaxMs_ = 0;
    Metric pass_ = WorldDraws;
    bool started_ = false, inFrame_ = false;
};
