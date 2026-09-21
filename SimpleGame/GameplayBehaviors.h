#pragma once

#include "GameplayTypes.h"
#include "World.h"
#include "NpcSystem.h"
#include <functional>
#include <optional>
#include <vector>

namespace Gameplay
{
    double Distance(WorldPoint a, WorldPoint b);

    struct EnemyServices
    {
        const World& world;
        const NpcSystem& npcs;
        WorldPoint player;
        std::function<bool(WorldPoint)> inCamp;
        std::function<std::optional<WorldPoint>(WorldPoint)> route;
        std::function<void(double)> hurt;
        std::function<void(Projectile)> shoot;
    };

    struct ProjectileServices
    {
        const World& world;
        WorldPoint player;
        std::vector<Enemy>& enemies;
        std::function<void(double)> hurt;
        std::function<void(FloatingNumber)> damageNumber;
    };

    // These behaviors know neither LevelOne nor the scene graph / OpenGL.
    void UpdateEnemy(Enemy& enemy, double dt, const EnemyServices& services);
    void UpdateProjectile(Projectile& shot, double dt, const ProjectileServices& services);
    bool MovePickup(ItemDrop& drop, double dt, const PlayerProgress& player, double magnetTime);
}
