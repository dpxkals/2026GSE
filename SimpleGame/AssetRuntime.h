#pragma once
#include "Actor.h"
#include "SceneRenderer.h"
#include "World.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

struct Citizen;

class AssetRuntime
{
  public:
    static AssetRuntime& Instance();
    ~AssetRuntime();
    bool Initialize(const std::filesystem::path& executableDirectory,
                    const std::filesystem::path& saveDirectory,
                    const std::filesystem::path& explicitAssetRoot = {});
    bool Reload(SceneRenderer& renderer);
    void Update(double simulationTime, const std::vector<Citizen>& citizens);
    void Shutdown();
    bool Ready() const;
    std::string PropMesh(SceneRenderer& renderer, Prop prop, int variation);
    bool DrawAnimatedProp(SceneRenderer& renderer,
                          Prop prop,
                          int variation,
                          const std::string& instance,
                          Point2 origin,
                          float zoom,
                          float opacity);
    bool DrawCitizen(SceneRenderer& renderer,
                     std::uint64_t id,
                     Point2 origin,
                     float zoom,
                     const ActorPose& pose);
    bool DrawRole(SceneRenderer& renderer,
                  const std::string& role,
                  const std::string& instance,
                  Point2 origin,
                  float zoom,
                  const ActorPose& pose);

  private:
    AssetRuntime();
    struct Implementation;
    std::unique_ptr<Implementation> data_;
    std::filesystem::path executable_, root_, save_;
};
