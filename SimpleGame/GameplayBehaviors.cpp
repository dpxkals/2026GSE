#include "stdafx.h"
#include "GameplayBehaviors.h"
#include <algorithm>
#include <cmath>

namespace Gameplay
{
    double Distance(WorldPoint a, WorldPoint b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    void UpdateEnemy(Enemy& enemy, double dt, const EnemyServices& services)
    {
        const auto& world = services.world;
        const auto& npcs = services.npcs;

        double distance = Distance(enemy.position, services.player);
        if (distance > 22.0 || enemy.health <= 0.0)
        {
            return;
        }
        enemy.hitFlash = (std::max)(0.0, enemy.hitFlash - dt);
        enemy.attackTimer -= dt;
        bool boss = enemy.kind == EnemyKind::Boss;
        bool enraged = boss && enemy.health <= enemy.maxHealth * 0.5;
        if (boss && enemy.windup > 0.0)
        {
            enemy.windup -= dt;
            if (enemy.windup <= 0.0)
            {
                if (Distance(services.player, enemy.attackPoint) < 2.3)
                {
                    services.hurt(enraged ? 32.0 : 24.0);
                }
                for (int i = 0; i < 8; ++i)
                {
                    double angle = i * 6.283185307 / 8;
                    services.shoot({enemy.position,
                                    std::cos(angle) * 5.0,
                                    std::sin(angle) * 5.0,
                                    9.0,
                                    12.0,
                                    true});
                }
                enemy.attackTimer = enraged ? 2.6 : 4.0;
            }
            return;
        }
        if (boss && enemy.attackTimer <= 0.0 && distance < 8.0 && !services.inCamp(services.player))
        {
            enemy.attackPoint = services.player;
            enemy.windup = 1.25;
            return;
        }
        if (!boss && enemy.attackTimer <= 0.0 && distance < 0.75)
        {
            services.hurt(enemy.kind == EnemyKind::Hound ? 10.0 : 8.0);
            enemy.attackTimer = 1.1;
        }
        if (boss && distance < 0.9)
        {
            services.hurt(14.0);
        }

        WorldPoint destination = services.player;
        auto route = services.route(enemy.position);
        if (!route)
        {
            return;
        }
        if (!world.ClearLine(enemy.position.x, enemy.position.y, destination.x, destination.y))
        {
            destination = *route;
        }
        double remaining = Distance(enemy.position, destination);
        if (remaining < 0.05)
        {
            return;
        }
        double speed = boss ? (enraged ? 2.3 : 1.6) : (enemy.kind == EnemyKind::Hound ? 2.7 : 1.8);
        double movement = (std::min)(remaining, speed * dt);
        double dx = (destination.x - enemy.position.x) / remaining * movement;
        double dy = (destination.y - enemy.position.y) / remaining * movement;
        WorldPoint next{enemy.position.x + dx, enemy.position.y};
        if (!services.inCamp(next) && npcs.CanWalk(next.x, next.y, world))
        {
            enemy.position = next;
        }
        next = {enemy.position.x, enemy.position.y + dy};
        if (!services.inCamp(next) && npcs.CanWalk(next.x, next.y, world))
        {
            enemy.position = next;
        }
    }

    void UpdateProjectile(Projectile& shot, double dt, const ProjectileServices& services)
    {
        const auto& world = services.world;

        double speed = std::hypot(shot.velocityX, shot.velocityY);
        double travel = (std::min)(shot.remainingRange, speed * dt);
        int steps = (std::max)(1, int(std::ceil(travel / 0.08)));
        for (int i = 0; i < steps && shot.remainingRange > 0.0; ++i)
        {
            shot.position.x += shot.velocityX / (std::max)(speed, 0.001) * travel / steps;
            shot.position.y += shot.velocityY / (std::max)(speed, 0.001) * travel / steps;
            shot.remainingRange -= travel / steps;
            if (!world.CanWalk(shot.position.x, shot.position.y))
            {
                shot.remainingRange = 0.0;
                break;
            }
            if (shot.hostile)
            {
                if (Distance(shot.position, services.player) < 0.32)
                {
                    services.hurt(shot.damage);
                    shot.remainingRange = 0.0;
                }
            }
            else
            {
                for (Enemy& enemy : services.enemies)
                {
                    double radius = enemy.kind == EnemyKind::Boss ? 0.6 : 0.34;
                    if (enemy.health > 0.0 && Distance(shot.position, enemy.position) <= radius)
                    {
                        enemy.health -= shot.damage;
                        enemy.hitFlash = 0.15;
                        services.damageNumber(
                            {enemy.position, int(std::round(shot.damage)), false, 0.75});
                        shot.remainingRange = 0.0;
                        break;
                    }
                }
            }
        }
    }

    bool MovePickup(ItemDrop& drop, double dt, const PlayerProgress& player, double magnetTime)
    {
        double distance = Distance(drop.position, player.position);
        double radius = magnetTime > 0.0 ? 12.0 : player.PickupRange();
        if (distance > radius ||
            (drop.kind == DropKind::Health && player.health >= player.MaxHealth()))
        {
            return false;
        }
        double step = (std::min)(distance, (magnetTime > 0.0 ? 12.0 : 7.0) * dt);
        if (distance > 0.001)
        {
            drop.position.x += (player.position.x - drop.position.x) / distance * step;
            drop.position.y += (player.position.y - drop.position.y) / distance * step;
        }
        return distance - step <= 0.35;
    }
}
