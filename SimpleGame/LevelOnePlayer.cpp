#include "stdafx.h"
#include "LevelOne.h"
#include "GameplayBehaviors.h"
#include "WorldSimulation.h"
#include <algorithm>
#include <cmath>
#include <queue>

bool LevelOne::Move(double dx, double dy, const World& world, const NpcSystem& npcs)
{
    auto* actor = scene_.Find("gameplay/player");
    if (!error_.empty() || !actor || !scene_.IsActive(actor->GetId()))
    {
        return false;
    }
    bool moved = false;
    WorldPoint before = player_.position;
    int steps = (std::max)(1, int(std::ceil(std::hypot(dx, dy) / 0.08)));
    for (int step = 0; step < steps; ++step)
    {
        if (npcs.CanWalk(player_.position.x + dx / steps, player_.position.y, world))
        {
            player_.position.x += dx / steps;
            moved = true;
        }
        if (npcs.CanWalk(player_.position.x, player_.position.y + dy / steps, world))
        {
            player_.position.y += dy / steps;
            moved = true;
        }
    }
    life_.distance += Gameplay::Distance(before, player_.position);
    WorldPoint destination = player_.position;
    player_.position = before;
    scene_.SetPosition(actor->GetId(), {destination.x, destination.y});
    return moved;
}

void LevelOne::ReturnToCamp()
{
    if (!InCamp(player_.position) && phase_ != LevelPhase::Cleared)
    {
        Announce(L"전투 중 즉시 귀환은 사용할 수 없습니다. 걸어서 거점으로 돌아오세요.");
        return;
    }
    SetPlayerPosition({});
    projectiles_.clear();
    routeTimer_ = 0.0;
    urgentSave_ = true;
}

void LevelOne::SetPlayerPosition(WorldPoint position)
{
    if (auto* actor = scene_.Find("gameplay/player"))
    {
        scene_.SetPosition(actor->GetId(), {position.x, position.y});
    }
    else
    {
        player_.position = position;
    }
}

void LevelOne::AutoFire(const World& world)
{
    target_ = 0;
    if (simulation_)
    {
        auto npc = simulation_->PlayerTarget(player_.position, player_.AttackRange(), world);
        if (npc.kind != CombatKind::None)
        {
            if (shotTimer_ <= 0)
            {
                double distance =
                    (std::max)(0.001, Gameplay::Distance(player_.position, npc.position));
                Projectile shot{player_.position,
                                (npc.position.x - player_.position.x) / distance * 12,
                                (npc.position.y - player_.position.y) / distance * 12,
                                player_.AttackRange(),
                                player_.Damage(),
                                false};
                shot.npcTarget = npc.id;
                projectiles_.push_back(shot);
                shotTimer_ = player_.ShotCooldown();
                ++life_.shots;
            }
            return;
        }
    }
    if (InCamp(player_.position) || (phase_ == LevelPhase::Cleared && !simulation_))
    {
        return;
    }
    const Enemy* target = nullptr;
    double nearest = player_.AttackRange();
    for (const Enemy& enemy : enemies_)
    {
        double distance = Gameplay::Distance(enemy.position, player_.position);
        if (enemy.health > 0.0 && distance <= nearest &&
            world.ClearLine(
                player_.position.x, player_.position.y, enemy.position.x, enemy.position.y))
        {
            nearest = distance;
            target = &enemy;
        }
    }
    if (!target)
    {
        return;
    }
    target_ = target->id;
    if (shotTimer_ > 0.0)
    {
        return;
    }
    double distance = (std::max)(0.001, nearest);
    double directionX =
        nearest < 0.001 ? 1.0 : (target->position.x - player_.position.x) / distance;
    double directionY =
        nearest < 0.001 ? 0.0 : (target->position.y - player_.position.y) / distance;
    projectiles_.push_back({player_.position,
                            directionX * 12.0,
                            directionY * 12.0,
                            player_.AttackRange(),
                            player_.Damage(),
                            false});
    shotTimer_ = player_.ShotCooldown();
    ++life_.shots;
}
