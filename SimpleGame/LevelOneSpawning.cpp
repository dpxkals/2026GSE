#include "stdafx.h"
#include "LevelOne.h"
#include "GameplayBehaviors.h"
#include <algorithm>
#include <cmath>
#include <queue>

void LevelOne::RebuildRoutes(const World& world, const NpcSystem& npcs)
{
    routes_.clear();
    Cell origin{WorldInt(std::floor(player_.position.x)), WorldInt(std::floor(player_.position.y))};
    if (!world.Find(origin.first, origin.second))
    {
        return;
    }

    std::queue<Cell> frontier;
    routes_.emplace(origin, RouteCell{origin, 0});
    frontier.push(origin);
    while (!frontier.empty())
    {
        Cell cell = frontier.front();
        frontier.pop();
        int steps = routes_.at(cell).steps;
        for (auto offset : {Cell{-1, 0}, {1, 0}, {0, -1}, {0, 1}})
        {
            Cell next{cell.first + offset.first, cell.second + offset.second};
            if (std::abs(next.first - origin.first) > 18 ||
                std::abs(next.second - origin.second) > 18 || routes_.find(next) != routes_.end())
            {
                continue;
            }
            if (npcs.CanWalk(next.first + 0.5, next.second + 0.5, world))
            {
                routes_.emplace(next, RouteCell{cell, steps + 1});
                frontier.push(next);
            }
        }
    }
}

bool LevelOne::Spawn(EnemyKind kind, const World& world)
{
    std::vector<WorldPoint> candidates;
    for (const auto& route : routes_)
    {
        WorldPoint point{route.first.first + 0.5, route.first.second + 0.5};
        double distance = Gameplay::Distance(point, player_.position);
        if (distance < 6.5 || distance > 9.0 || InCamp(point) || route.second.steps > 24)
        {
            continue;
        }
        bool occupied = false;
        for (const auto& enemy : enemies_)
        {
            if (Gameplay::Distance(enemy.position, point) < 1.2)
            {
                occupied = true;
                break;
            }
        }
        if (!occupied && world.CanWalk(point.x, point.y))
        {
            candidates.push_back(point);
        }
    }
    if (candidates.empty())
    {
        return false;
    }

    Enemy enemy;
    enemy.id = nextId_++;
    enemy.kind = kind;
    enemy.position = candidates[random_() % candidates.size()];
    enemy.maxHealth = kind == EnemyKind::Boss ? 650.0 : (kind == EnemyKind::Hound ? 24.0 : 34.0);
    enemy.health = enemy.maxHealth;
    enemy.attackTimer = kind == EnemyKind::Boss ? 3.0 : 1.0;
    enemy.attackPoint = enemy.position;
    enemies_.push_back(enemy);
    return true;
}
