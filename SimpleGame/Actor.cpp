#include "stdafx.h"
#include "Actor.h"
#include <utility>

Actor::Actor(std::string key, std::string type)
    : key_(std::move(key)),
      type_(std::move(type))
{
}

Actor::Id Actor::GetId() const
{
    return id_;
}

Actor::Id Actor::Parent() const
{
    return parent_;
}

const std::string& Actor::Key() const
{
    return key_;
}

const std::string& Actor::Type() const
{
    return type_;
}

ActorPosition Actor::Position() const
{
    return readPosition_ ? readPosition_() : position_;
}

void Actor::WritePosition(ActorPosition value)
{
    if (writePosition_)
    {
        writePosition_(value);
    }
    else
    {
        position_ = value;
    }
}

void Actor::BindPosition(std::function<ActorPosition()> read,
                         std::function<void(ActorPosition)> write)
{
    readPosition_ = std::move(read);
    writePosition_ = std::move(write);
}

void Actor::BindUpdate(std::function<void(double)> update)
{
    update_ = std::move(update);
}

void Actor::BindRemoval(std::function<void()> remove)
{
    remove_ = std::move(remove);
}

void Actor::Update(double dt)
{
    if (update_)
    {
        update_(dt);
    }
}
