#pragma once

#include <chrono>
#include <condition_variable>
#include <filesystem>
#include <mutex>
#include <memory>
#include <string>
#include <thread>

class World;
class WorldSimulation;
class LevelOne;
class NpcSystem;
class MonitorServer;

// Game-thread snapshot capture; the writer never accesses gameplay objects.
class WorldMonitor
{
  public:
    WorldMonitor(const std::filesystem::path& directory, const std::filesystem::path& assets);
    ~WorldMonitor();
    WorldMonitor(const WorldMonitor&) = delete;
    WorldMonitor& operator=(const WorldMonitor&) = delete;

    void Publish(const World& world,
                 const WorldSimulation& society,
                 const LevelOne& level,
                 const NpcSystem& residents,
                 bool running,
                 bool paused,
                 bool focused,
                 bool force = false);

  private:
    void WriteLoop();
    std::filesystem::path path_;
    std::string session_;
    unsigned long long sequence_ = 0;
    std::chrono::steady_clock::time_point next_{};
    std::mutex mutex_;
    std::condition_variable ready_;
    std::string pending_;
    bool stopping_ = false;
    std::thread writer_;
    std::unique_ptr<MonitorServer> server_;
};
