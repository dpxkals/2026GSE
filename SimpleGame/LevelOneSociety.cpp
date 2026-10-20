#include "stdafx.h"
#include "LevelOne.h"
#include "GameplayBehaviors.h"
#include <algorithm>
#include <cmath>

bool LevelOne::DamageMonster(std::uint64_t id, double damage, std::uint64_t npc)
{
    for (auto& enemy : enemies_)
    {
        if (enemy.id == id && enemy.health > 0)
        {
            enemy.health = (std::max)(0.0, enemy.health - damage);
            enemy.lastHitNpc = npc;
            enemy.hitFlash = 0.15;
            numbers_.push_back({enemy.position, int(damage), false, 0.75});
            return enemy.health <= 0;
        }
    }
    return false;
}

bool LevelOne::SpawnWild(WorldPoint position, EnemyKind kind, const World& world)
{
    if (enemies_.size() >= 64 || InCamp(position) || !world.CanWalk(position.x, position.y))
    {
        return false;
    }
    for (const auto& enemy : enemies_)
    {
        if (Gameplay::Distance(enemy.position, position) < 2)
        {
            return false;
        }
    }
    Enemy enemy;
    enemy.id = nextId_++;
    enemy.kind = kind;
    enemy.position = enemy.attackPoint = position;
    enemy.health = enemy.maxHealth = kind == EnemyKind::Boss    ? VillageBossHealth
                                     : kind == EnemyKind::Hound ? 24
                                                                : 34;
    enemies_.push_back(enemy);
    return true;
}

void LevelOne::ReceiveNpcDamage(double amount)
{
    if (invulnerable_ > 0 || player_.health <= 0)
    {
        return;
    }
    amount = player_.IncomingDamage(amount);
    life_.damageTaken += (std::min)(player_.health, amount);
    player_.health = (std::max)(0.0, player_.health - amount);
    invulnerable_ = 0.65;
    numbers_.push_back({player_.position, int(amount), true, 0.75});
}

void LevelOne::ReceiveTrade(bool training)
{
    if (training)
    {
        GainExperience(20);
    }
    else
    {
        player_.health = (std::min)(player_.MaxHealth(), player_.health + 35);
    }
    urgentSave_ = true;
}
