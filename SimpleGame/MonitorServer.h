#pragma once
#include <filesystem>
#include <memory>
#include <string>

// Loopback HTTP/SSE transport. Owns no gameplay objects.
class MonitorServer
{
  public:
    MonitorServer(const std::filesystem::path& assets, std::string quotedSource);
    ~MonitorServer();
    MonitorServer(const MonitorServer&) = delete;
    MonitorServer& operator=(const MonitorServer&) = delete;
    void Update(std::string snapshot, std::string state);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
