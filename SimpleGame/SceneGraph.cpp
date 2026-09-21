#include "stdafx.h"
#include "SceneGraph.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

Actor& SceneGraph::Add(std::unique_ptr<Actor> actor, Actor::Id parent)
{
    if (!actor || keys_.count(actor->Key()) || (parent && !Find(parent)))
    {
        throw std::invalid_argument("Invalid or duplicate actor / parent");
    }
    actor->id_ = nextId_++;
    actor->parent_ = parent;
    auto id = actor->id_;
    keys_.emplace(actor->Key(), id);
    actors_.emplace(id, std::move(actor));
    children_[parent].insert(id);
    return *actors_.at(id);
}

Actor& SceneGraph::Ensure(const std::string& key, const std::string& type, Actor::Id parent)
{
    if (auto actor = Find(key))
    {
        if (actor->Type() != type)
        {
            throw std::invalid_argument("Actor key already belongs to another type");
        }
        return *actor;
    }
    return Add(std::make_unique<Actor>(key, type), parent);
}

Actor* SceneGraph::Find(Actor::Id id)
{
    return const_cast<Actor*>(static_cast<const SceneGraph*>(this)->Find(id));
}

const Actor* SceneGraph::Find(Actor::Id id) const
{
    auto it = actors_.find(id);
    return it == actors_.end() || pendingRemoval_.count(id) ? nullptr : it->second.get();
}

Actor* SceneGraph::Find(const std::string& key)
{
    auto it = keys_.find(key);
    return it == keys_.end() ? nullptr : Find(it->second);
}

std::vector<Actor::Id> SceneGraph::Children(Actor::Id parent) const
{
    std::vector<Actor::Id> result;
    auto found = children_.find(parent);
    if (found == children_.end())
    {
        return result;
    }
    for (auto id : found->second)
    {
        if (Find(id))
        {
            result.push_back(id);
        }
    }
    return result;
}

ActorPosition SceneGraph::LocalPosition(Actor::Id id) const
{
    const Actor* actor = Find(id);
    if (!actor)
    {
        return {};
    }
    auto position = actor->Position();
    if (auto parent = Find(actor->Parent()))
    {
        auto origin = parent->Position();
        position.x -= origin.x;
        position.y -= origin.y;
    }
    return position;
}

void SceneGraph::TranslateChildren(Actor::Id parent, ActorPosition delta)
{
    for (auto id : Children(parent))
    {
        Actor* child = Find(id);
        auto position = child->Position();
        child->WritePosition({position.x + delta.x, position.y + delta.y});
        TranslateChildren(id, delta);
    }
}

bool SceneGraph::SetPosition(Actor::Id id, ActorPosition position)
{
    Actor* actor = Find(id);
    if (!actor || !std::isfinite(position.x) || !std::isfinite(position.y))
    {
        return false;
    }
    auto before = actor->Position();
    actor->WritePosition(position);
    auto after = actor->Position();
    ActorPosition delta{after.x - before.x, after.y - before.y};
    actor->propagatedTranslation_.x += delta.x;
    actor->propagatedTranslation_.y += delta.y;
    TranslateChildren(id, delta);
    return true;
}

bool SceneGraph::SetLocalPosition(Actor::Id id, ActorPosition position)
{
    Actor* actor = Find(id);
    if (!actor)
    {
        return false;
    }
    if (auto parent = Find(actor->Parent()))
    {
        auto origin = parent->Position();
        position.x += origin.x;
        position.y += origin.y;
    }
    return SetPosition(id, position);
}

bool SceneGraph::Reparent(Actor::Id id, Actor::Id parent, bool keepWorldPosition)
{
    auto actor = Find(id);
    if (!actor || id == parent || (parent && !Find(parent)))
    {
        return false;
    }
    for (auto ancestor = Find(parent); ancestor; ancestor = Find(ancestor->Parent()))
    {
        if (ancestor->GetId() == id)
        {
            return false;
        }
    }
    auto local = LocalPosition(id);
    children_[actor->parent_].erase(id);
    children_[parent].insert(id);
    actor->parent_ = parent;
    return keepWorldPosition || SetLocalPosition(id, local);
}

bool SceneGraph::IsActive(Actor::Id id) const
{
    auto actor = Find(id);
    return actor && actor->active && (!actor->Parent() || IsActive(actor->Parent()));
}

bool SceneGraph::IsVisible(Actor::Id id) const
{
    auto actor = Find(id);
    return actor && actor->visible && (!actor->Parent() || IsVisible(actor->Parent()));
}

void SceneGraph::Remove(Actor::Id id, bool notify)
{
    if (!Find(id))
    {
        return;
    }
    for (auto child : Children(id))
    {
        Remove(child, notify);
    }
    pendingRemoval_[id] = notify;
}

void SceneGraph::FlushRemovals()
{
    if (flushing_)
    {
        return;
    }
    flushing_ = true;
    while (!pendingRemoval_.empty())
    {
        auto pending = *pendingRemoval_.begin();
        pendingRemoval_.erase(pending.first);
        auto it = actors_.find(pending.first);
        if (it != actors_.end())
        {
            auto actor = std::move(it->second);
            auto siblings = children_.find(actor->Parent());
            if (siblings != children_.end())
            {
                siblings->second.erase(pending.first);
                if (siblings->second.empty())
                {
                    children_.erase(siblings);
                }
            }
            children_.erase(pending.first);
            keys_.erase(actor->Key());
            actors_.erase(it);
            if (pending.second && actor->remove_)
            {
                actor->remove_();
            }
        }
    }
    flushing_ = false;
}

void SceneGraph::Destroy(Actor::Id id)
{
    Remove(id, true);
    if (!traversing_)
    {
        FlushRemovals();
    }
}

void SceneGraph::Retain(const std::string& prefix, const std::set<std::string>& keys)
{
    for (const auto& entry : actors_)
    {
        if (entry.second->Key().compare(0, prefix.size(), prefix) == 0 &&
            !keys.count(entry.second->Key()))
        {
            Remove(entry.first, false);
        }
    }
    if (!traversing_)
    {
        FlushRemovals();
    }
}

void SceneGraph::Update(UpdatePhase phase, double dt)
{
    std::vector<Actor::Id> order;
    for (const auto& entry : actors_)
    {
        if (entry.second->phase == phase)
        {
            order.push_back(entry.first);
        }
    }
    auto depth = [this](Actor::Id id)
    {
        size_t result = 0;
        for (auto actor = Find(id); actor && actor->Parent(); actor = Find(actor->Parent()))
        {
            ++result;
        }
        return result;
    };
    std::stable_sort(order.begin(),
                     order.end(),
                     [&depth](auto a, auto b)
                     {
                         return depth(a) < depth(b);
                     });
    traversing_ = true;
    for (auto id : order)
    {
        auto actor = Find(id);
        if (actor && IsActive(id))
        {
            auto before = actor->Position();
            auto propagated = actor->propagatedTranslation_;
            actor->Update(dt);
            auto after = actor->Position();
            ActorPosition delta{
                after.x - before.x - (actor->propagatedTranslation_.x - propagated.x),
                after.y - before.y - (actor->propagatedTranslation_.y - propagated.y)};
            if (Find(id) && (delta.x != 0.0 || delta.y != 0.0))
            {
                TranslateChildren(id, delta);
            }
        }
    }
    traversing_ = false;
    FlushRemovals();
}

void SceneGraph::SetRenderer(const std::string& type, Renderer renderer)
{
    renderers_[type] = std::move(renderer);
}

void SceneGraph::Render(RenderLayer layer, const std::function<bool(const Actor&)>& filter)
{
    std::vector<Actor::Id> order;
    for (const auto& entry : actors_)
    {
        if (entry.second->layer == layer && IsVisible(entry.first) &&
            renderers_.count(entry.second->Type()) && (!filter || filter(*entry.second)))
        {
            order.push_back(entry.first);
        }
    }
    std::sort(order.begin(),
              order.end(),
              [this](Actor::Id left, Actor::Id right)
              {
                  auto a = Find(left)->Position(), b = Find(right)->Position();
                  if (a.x + a.y != b.x + b.y)
                  {
                      return a.x + a.y < b.x + b.y;
                  }
                  if (a.x != b.x)
                  {
                      return a.x < b.x;
                  }
                  return left < right;
              });
    traversing_ = true;
    for (auto id : order)
    {
        if (auto actor = Find(id))
        {
            auto renderer = renderers_.find(actor->Type());
            if (IsVisible(id) && renderer != renderers_.end())
            {
                renderer->second(*actor);
            }
        }
    }
    traversing_ = false;
    FlushRemovals();
}

size_t SceneGraph::Size() const
{
    return std::count_if(actors_.begin(),
                         actors_.end(),
                         [this](const auto& entry)
                         {
                             return !pendingRemoval_.count(entry.first);
                         });
}
