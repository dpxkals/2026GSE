#pragma once

#include "Actor.h"
#include <map>
#include <memory>
#include <set>
#include <vector>

class SceneGraph
{
  public:
    using Renderer = std::function<void(Actor&)>;
    Actor& Add(std::unique_ptr<Actor> actor, Actor::Id parent = 0);
    Actor& Ensure(const std::string& key, const std::string& type, Actor::Id parent = 0);
    Actor* Find(Actor::Id id);
    const Actor* Find(Actor::Id id) const;
    Actor* Find(const std::string& key);
    std::vector<Actor::Id> Children(Actor::Id parent) const;
    bool Reparent(Actor::Id id, Actor::Id parent, bool keepWorldPosition = true);
    bool SetPosition(Actor::Id id, ActorPosition worldPosition);
    bool SetLocalPosition(Actor::Id id, ActorPosition localPosition);
    ActorPosition LocalPosition(Actor::Id id) const;
    bool IsActive(Actor::Id id) const;
    bool IsVisible(Actor::Id id) const;
    void Destroy(Actor::Id id);
    // Unbind expired/streamed-out records without deleting persistent world data.
    void Retain(const std::string& prefix, const std::set<std::string>& keys);
    void Update(UpdatePhase phase, double dt);
    void SetRenderer(const std::string& type, Renderer renderer);
    void Render(RenderLayer layer, const std::function<bool(const Actor&)>& filter = {});
    size_t Size() const;

  private:
    void Remove(Actor::Id id, bool notify);
    void FlushRemovals();
    void TranslateChildren(Actor::Id parent, ActorPosition delta);
    std::map<Actor::Id, std::unique_ptr<Actor>> actors_;
    std::map<std::string, Actor::Id> keys_;
    std::map<Actor::Id, std::set<Actor::Id>> children_;
    std::map<std::string, Renderer> renderers_;
    std::map<Actor::Id, bool> pendingRemoval_;
    Actor::Id nextId_ = 1;
    bool traversing_ = false;
    bool flushing_ = false;
};
