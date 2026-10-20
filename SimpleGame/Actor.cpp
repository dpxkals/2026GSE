#include "stdafx.h"
#include "Actor.h"
#include <utility>
#include <algorithm>
#include <cmath>

ActorPose Actor::Animate(double time, double attackCooldown, double health)
{
    auto position = Position();
    double dt = time - animationTime_;
    // Reappearing / teleported actors must not catch up through an entire walk cycle.
    if (animationTime_ < 0 || dt < 0 || dt > .5)
    {
        animationPosition_ = position;
        animationCooldown_ = attackCooldown;
        animationHealth_ = health;
        animationPhase_ = double(recordId % 31) * .37;
        animationMoving_ = animationAttack_ = animationHurt_ = 0;
        dt = 0;
    }
    double dx = position.x - animationPosition_.x, dy = position.y - animationPosition_.y;
    double distance = std::hypot(dx, dy);
    if (dt > 0)
    {
        if (distance > .001 && distance < 3)
        {
            animationMoving_ = 1;
            if (std::abs(dx - dy) > .001)
            {
                animationFacing_ = dx - dy < 0 ? -1.0f : 1.0f;
            }
        }
        else
        {
            animationMoving_ *= std::exp(-dt * 9);
        }
        animationPhase_ = std::fmod(animationPhase_ + dt * 11 * animationMoving_, 6.283185307);
        animationAttack_ = (std::max)(0.0, animationAttack_ - dt);
        animationHurt_ = (std::max)(0.0, animationHurt_ - dt);
        if (health >= 0 && animationHealth_ >= 0 && health < animationHealth_ - .01)
        {
            animationHurt_ = .24;
        }
        if (attackCooldown > animationCooldown_ + .01)
        {
            animationAttack_ = .28;
        }
    }
    animationTime_ = time;
    animationPosition_ = position;
    animationCooldown_ = attackCooldown;
    animationHealth_ = health;
    ActorPose pose;
    pose.moving = float(animationMoving_);
    pose.phase = float(animationPhase_);
    pose.stride = float(std::sin(animationPhase_) * animationMoving_);
    pose.breath = float(std::sin(time * 2.4 + double(recordId % 31)));
    pose.attack = float(std::sin(animationAttack_ / .28 * 3.141592654));
    pose.hurt = float(animationHurt_ / .24);
    pose.facing = animationFacing_;
    return pose;
}

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
