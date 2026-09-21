#pragma once

#include <cstdint>
#include <functional>
#include <string>

struct ActorPosition
{
    double x = 0.0;
    double y = 0.0;
};

enum class RenderLayer
{
    Ground,
    GroundOverlay,
    World,
    Effects,
    UI
};

enum class UpdatePhase
{
    Passive,
    Effects,
    Player,
    Enemies,
    Projectiles,
    Items
};

class SceneGraph;

// Rendering-library independent scene object. A binding can expose a saved component's
// position directly, avoiding a second transform that drifts away from gameplay state.
class Actor
{
  public:
    using Id = std::uint64_t;
    Actor(std::string key, std::string type);
    virtual ~Actor() = default;
    Actor(const Actor&) = delete;
    Actor& operator=(const Actor&) = delete;

    Id GetId() const;
    Id Parent() const;
    const std::string& Key() const;
    const std::string& Type() const;
    virtual ActorPosition Position() const;
    virtual void Update(double dt);
    void BindPosition(std::function<ActorPosition()> read,
                      std::function<void(ActorPosition)> write);
    void BindUpdate(std::function<void(double)> update);
    void BindRemoval(std::function<void()> remove);

    bool active = true;
    bool visible = true;
    RenderLayer layer = RenderLayer::World;
    UpdatePhase phase = UpdatePhase::Passive;
    std::uint64_t recordId = 0;
    float opacity = 1.0f;

  private:
    friend class SceneGraph;
    void WritePosition(ActorPosition value);
    Id id_ = 0;
    Id parent_ = 0;
    std::string key_;
    std::string type_;
    ActorPosition position_;
    ActorPosition propagatedTranslation_;
    std::function<ActorPosition()> readPosition_;
    std::function<void(ActorPosition)> writePosition_;
    std::function<void(double)> update_;
    std::function<void()> remove_;
};
