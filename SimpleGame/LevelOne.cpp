#include "stdafx.h"
#include "LevelOne.h"
#include <algorithm>
#include <cmath>
#include <queue>

namespace
{
    double Distance(WorldPoint a, WorldPoint b)
    {
        return std::hypot(a.x - b.x, a.y - b.y);
    }

    bool EnemyStep(WorldPoint position, const World& world, const NpcSystem& npcs)
    {
        return !LevelOne::InCamp(position) && npcs.CanWalk(position.x, position.y, world);
    }
}

int PlayerProgress::ExperienceNeeded() const
{
    return 24 + (level - 1) * 16;
}

double PlayerProgress::MaxHealth() const
{
    return 100.0 + (level - 1) * 15.0;
}

double PlayerProgress::Damage() const
{
    return 12.0 + (level - 1) * 2.5 + weaponRank * 4.0;
}

double PlayerProgress::ShotCooldown() const
{
    return (std::max)(0.24, 0.85 * std::pow(0.91, level - 1) * std::pow(0.96, weaponRank));
}

double PlayerProgress::MoveSpeed() const
{
    return 4.5 + (std::min)(0.9, (level - 1) * 0.06);
}

double PlayerProgress::PickupRange() const
{
    return (std::min)(4.0, 2.2 + (level - 1) * 0.15);
}

double PlayerProgress::AttackRange() const
{
    return 6.0;
}

bool LevelOne::InCamp(WorldPoint position)
{
    return std::hypot(position.x - 0.5, position.y - 0.5) < 3.8;
}

LevelOne::LevelOne(const std::filesystem::path& directory, std::uint64_t seed)
    : random_(static_cast<std::uint32_t>(seed ^ (seed >> 32))),
      seed_(seed),
      savePath_(directory / L"level_one.dat")
{
    Load();
    Announce(L"레벨 1 · 잿빛 들판 — 거점 밖으로 나가 파밍을 시작하세요.");
}

void LevelOne::Announce(const std::wstring& text)
{
    notice_ = text;
    noticeTime_ = 4.0;
}

std::wstring LevelOne::Objective() const
{
    if (phase_ == LevelPhase::Cleared)
    {
        return L"레벨 1 완료 — 잿빛 파수꾼을 쓰러뜨렸습니다.";
    }
    if (phase_ == LevelPhase::BossFight)
    {
        return L"보스: 잿빛 파수꾼 — 붉은 공격 예고를 피하세요.";
    }
    return L"파밍: 적 " + std::to_wstring(player_.kills) + L" / 24 처치 후 보스 등장";
}

std::wstring LevelOne::Hint() const
{
    if (phase_ == LevelPhase::Cleared)
    {
        return L"파밍 훈련을 마쳤습니다. 남은 보상을 줍거나 거점으로 돌아가세요.";
    }
    if (InCamp(player_.position))
    {
        return L"안전 거점: 자동 회복 중. 원 밖으로 이동하면 자동 공격합니다.";
    }
    if (player_.souls == 0)
    {
        return L"적에게 6타일 이내로 접근하세요. 영혼석은 가까이 가면 자동 습득합니다.";
    }
    if (player_.upgrades == 0)
    {
        return L"주황색 강화석을 모으세요. 공격력과 발사 속도가 증가합니다.";
    }
    if (player_.magnets == 0)
    {
        return L"청록색 자석은 8초 동안 12타일 이내의 아이템을 끌어옵니다.";
    }
    return L"붉은 물약은 체력 회복, 영혼석은 성장. R: 사거리 표시 / P: 일시정지";
}

double LevelOne::ShotReady() const
{
    return 1.0 - std::clamp(shotTimer_ / player_.ShotCooldown(), 0.0, 1.0);
}

bool LevelOne::Move(double dx, double dy, const World& world, const NpcSystem& npcs)
{
    if (!error_.empty())
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
    life_.distance += Distance(before, player_.position);
    return moved;
}

void LevelOne::ReturnToCamp()
{
    if (!InCamp(player_.position) && phase_ != LevelPhase::Cleared)
    {
        Announce(L"전투 중 즉시 귀환은 사용할 수 없습니다. 걸어서 거점으로 돌아오세요.");
        return;
    }
    player_.position = {};
    projectiles_.clear();
    routeTimer_ = 0.0;
    urgentSave_ = true;
}

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
        double distance = Distance(point, player_.position);
        if (distance < 6.5 || distance > 9.0 || InCamp(point) || route.second.steps > 24)
        {
            continue;
        }
        bool occupied = false;
        for (const auto& enemy : enemies_)
        {
            if (Distance(enemy.position, point) < 1.2)
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

void LevelOne::GainExperience(int amount)
{
    if (player_.level >= 30)
    {
        return;
    }
    player_.experience += amount;
    while (player_.level < 30 && player_.experience >= player_.ExperienceNeeded())
    {
        player_.experience -= player_.ExperienceNeeded();
        ++player_.level;
        if (player_.health > 0.0)
        {
            player_.health = (std::min)(player_.MaxHealth(), player_.health + 25.0);
        }
        Announce(L"레벨 업! 공격력 · 발사 속도 · 최대 체력 · 이동/습득 범위 증가");
    }
    if (player_.level == 30)
    {
        player_.experience = 0;
    }
}

void LevelOne::Drop(DropKind kind, WorldPoint position, int amount)
{
    drops_.push_back({nextId_++, kind, position, amount});
}

void LevelOne::Defeat(const Enemy& enemy)
{
    if (enemy.kind == EnemyKind::Boss)
    {
        phase_ = LevelPhase::Cleared;
        GainExperience(100);
        Drop(DropKind::Soul, enemy.position, 80);
        Drop(DropKind::Upgrade, enemy.position, 3);
        Drop(DropKind::Health, enemy.position, 60);
        Drop(DropKind::Magnet, enemy.position, 1);
        Announce(L"레벨 1 완료! 잿빛 파수꾼이 쓰러졌습니다. 남은 보상을 수집하세요.");
    }
    else
    {
        ++player_.kills;
        ++life_.kills;
        GainExperience(4);
        Drop(DropKind::Soul, enemy.position, 8);
        if (player_.kills % 4 == 0)
        {
            Drop(DropKind::Upgrade, enemy.position, 1);
        }
        if (player_.kills % 6 == 0 || random_() % 100 < 12)
        {
            Drop(DropKind::Health, enemy.position, 30);
        }
        if (player_.kills % 9 == 0)
        {
            Drop(DropKind::Magnet, enemy.position, 1);
        }
    }
    urgentSave_ = true;
}

void LevelOne::Hurt(double amount)
{
    if (invulnerable_ > 0.0 || InCamp(player_.position) || phase_ == LevelPhase::Cleared)
    {
        return;
    }
    life_.damageTaken += (std::min)(player_.health, amount);
    player_.health = (std::max)(0.0, player_.health - amount);
    invulnerable_ = 0.65;
    numbers_.push_back({player_.position, int(amount), true, 0.75});
}

void LevelOne::AutoFire(const World& world)
{
    target_ = 0;
    if (InCamp(player_.position) || phase_ == LevelPhase::Cleared)
    {
        return;
    }
    const Enemy* target = nullptr;
    double nearest = player_.AttackRange();
    for (const Enemy& enemy : enemies_)
    {
        double distance = Distance(enemy.position, player_.position);
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

void LevelOne::UpdateEnemies(double dt, const World& world, const NpcSystem& npcs)
{
    for (Enemy& enemy : enemies_)
    {
        double distance = Distance(enemy.position, player_.position);
        if (distance > 22.0 || enemy.health <= 0.0)
        {
            continue;
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
                if (Distance(player_.position, enemy.attackPoint) < 2.3)
                {
                    Hurt(enraged ? 32.0 : 24.0);
                }
                for (int i = 0; i < 8; ++i)
                {
                    double angle = i * 6.283185307 / 8;
                    projectiles_.push_back({enemy.position,
                                            std::cos(angle) * 5.0,
                                            std::sin(angle) * 5.0,
                                            9.0,
                                            12.0,
                                            true});
                }
                enemy.attackTimer = enraged ? 2.6 : 4.0;
            }
            continue;
        }
        if (boss && enemy.attackTimer <= 0.0 && distance < 8.0 && !InCamp(player_.position))
        {
            enemy.attackPoint = player_.position;
            enemy.windup = 1.25;
            continue;
        }
        if (!boss && enemy.attackTimer <= 0.0 && distance < 0.75)
        {
            Hurt(enemy.kind == EnemyKind::Hound ? 10.0 : 8.0);
            enemy.attackTimer = 1.1;
        }
        if (boss && distance < 0.9)
        {
            Hurt(14.0);
        }

        WorldPoint destination = player_.position;
        Cell cell{WorldInt(std::floor(enemy.position.x)), WorldInt(std::floor(enemy.position.y))};
        auto route = routes_.find(cell);
        if (route == routes_.end())
        {
            continue;
        }
        if (!world.ClearLine(enemy.position.x, enemy.position.y, destination.x, destination.y))
        {
            destination = {route->second.next.first + 0.5, route->second.next.second + 0.5};
        }
        double remaining = Distance(enemy.position, destination);
        if (remaining < 0.05)
        {
            continue;
        }
        double speed = boss ? (enraged ? 2.3 : 1.6) : (enemy.kind == EnemyKind::Hound ? 2.7 : 1.8);
        double movement = (std::min)(remaining, speed * dt);
        double dx = (destination.x - enemy.position.x) / remaining * movement;
        double dy = (destination.y - enemy.position.y) / remaining * movement;
        WorldPoint next{enemy.position.x + dx, enemy.position.y};
        if (EnemyStep(next, world, npcs))
        {
            enemy.position = next;
        }
        next = {enemy.position.x, enemy.position.y + dy};
        if (EnemyStep(next, world, npcs))
        {
            enemy.position = next;
        }
    }
}

void LevelOne::UpdateProjectiles(double dt, const World& world)
{
    for (Projectile& shot : projectiles_)
    {
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
                if (Distance(shot.position, player_.position) < 0.32)
                {
                    Hurt(shot.damage);
                    shot.remainingRange = 0.0;
                }
            }
            else
            {
                for (Enemy& enemy : enemies_)
                {
                    double radius = enemy.kind == EnemyKind::Boss ? 0.6 : 0.34;
                    if (enemy.health > 0.0 && Distance(shot.position, enemy.position) <= radius)
                    {
                        enemy.health -= shot.damage;
                        enemy.hitFlash = 0.15;
                        numbers_.push_back(
                            {enemy.position, int(std::round(shot.damage)), false, 0.75});
                        shot.remainingRange = 0.0;
                        break;
                    }
                }
            }
        }
    }
    projectiles_.erase(std::remove_if(projectiles_.begin(),
                                      projectiles_.end(),
                                      [](const Projectile& shot)
                                      {
                                          return shot.remainingRange <= 0.0001;
                                      }),
                       projectiles_.end());

    for (auto it = enemies_.begin(); it != enemies_.end();)
    {
        if (it->health <= 0.0)
        {
            Defeat(*it);
            it = enemies_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    // Include the final frame's kills in the life snapshot; XP never revives zero HP.
    if (player_.health <= 0.0)
    {
        Die();
    }
    if (phase_ == LevelPhase::Cleared)
    {
        projectiles_.clear();
    }
}

void LevelOne::UpdateDrops(double dt)
{
    for (auto it = drops_.begin(); it != drops_.end();)
    {
        double distance = Distance(it->position, player_.position);
        double radius = magnetTime_ > 0.0 ? 12.0 : player_.PickupRange();
        if (distance > radius)
        {
            ++it;
            continue;
        }
        // A healing item is left on the ground when no health is missing.
        if (it->kind == DropKind::Health && player_.health >= player_.MaxHealth())
        {
            ++it;
            continue;
        }
        double step = (std::min)(distance, (magnetTime_ > 0.0 ? 12.0 : 7.0) * dt);
        if (distance > 0.001)
        {
            it->position.x += (player_.position.x - it->position.x) / distance * step;
            it->position.y += (player_.position.y - it->position.y) / distance * step;
        }
        if (distance - step > 0.35)
        {
            ++it;
            continue;
        }
        switch (it->kind)
        {
            case DropKind::Soul:
                GainExperience(it->amount);
                ++player_.souls;
                ++life_.souls;
                break;
            case DropKind::Upgrade:
                player_.weaponRank = (std::min)(20, player_.weaponRank + it->amount);
                ++player_.upgrades;
                Announce(L"무기 강화 +" + std::to_wstring(player_.weaponRank) +
                         L" — 공격력과 발사 속도 증가");
                break;
            case DropKind::Health:
                player_.health = (std::min)(player_.MaxHealth(), player_.health + it->amount);
                ++player_.potions;
                ++life_.heals;
                Announce(L"회복 물약 자동 습득 — 체력을 회복했습니다.");
                break;
            case DropKind::Magnet:
                magnetTime_ = 8.0;
                ++player_.magnets;
                Announce(L"영혼의 자석 — 8초 동안 주변 아이템을 끌어옵니다.");
                break;
        }
        it = drops_.erase(it);
        urgentSave_ = true;
    }
}

void LevelOne::Update(double dt, const World& world, const NpcSystem& npcs)
{
    if (!error_.empty() || !world.Error().empty())
    {
        return;
    }
    if (!world.Find(WorldInt(std::floor(player_.position.x)),
                    WorldInt(std::floor(player_.position.y))))
    {
        return;
    }
    if (!npcs.CanWalk(player_.position.x, player_.position.y, world))
    {
        player_.position = {};
        routeTimer_ = 0.0;
        Announce(L"이동 가능한 거점으로 위치를 복구했습니다.");
        return;
    }
    noticeTime_ -= dt;
    life_.seconds += dt;
    if (noticeTime_ <= 0.0)
    {
        notice_.clear();
    }
    invulnerable_ = (std::max)(0.0, invulnerable_ - dt);
    magnetTime_ = (std::max)(0.0, magnetTime_ - dt);
    shotTimer_ = (std::max)(0.0, shotTimer_ - dt);
    for (auto& number : numbers_)
    {
        number.life -= dt;
    }
    numbers_.erase(std::remove_if(numbers_.begin(),
                                  numbers_.end(),
                                  [](const FloatingNumber& number)
                                  {
                                      return number.life <= 0.0;
                                  }),
                   numbers_.end());

    routeTimer_ -= dt;
    if (routeTimer_ <= 0.0)
    {
        RebuildRoutes(world, npcs);
        routeTimer_ = 0.25;
    }
    spawnTimer_ -= dt;
    if (phase_ == LevelPhase::Farming && !InCamp(player_.position))
    {
        if (player_.kills >= BossKillRequirement)
        {
            if (Spawn(EnemyKind::Boss, world))
            {
                phase_ = LevelPhase::BossFight;
                Announce(L"잿빛 파수꾼 등장! 붉은 원이 사라지기 전에 피하세요.");
                urgentSave_ = true;
            }
        }
        else if (spawnTimer_ <= 0.0)
        {
            int activeCount = 0;
            for (const Enemy& enemy : enemies_)
            {
                activeCount += Distance(enemy.position, player_.position) < 20.0 ? 1 : 0;
            }
            int limit = (std::min)(12, 4 + player_.kills / 4);
            if (activeCount < limit)
            {
                Spawn(random_() % 3 == 0 ? EnemyKind::Hound : EnemyKind::Husk, world);
            }
            spawnTimer_ = (std::max)(0.8, 2.2 - player_.kills * 0.045);
        }
    }
    if (InCamp(player_.position))
    {
        player_.health = (std::min)(player_.MaxHealth(), player_.health + 5.0 * dt);
    }
    if (phase_ != LevelPhase::Cleared)
    {
        AutoFire(world);
        UpdateEnemies(dt, world, npcs);
        UpdateProjectiles(dt, world);
    }
    if (player_.health <= 0.0)
    {
        Die();
    }
    UpdateDrops(dt);
    saveTimer_ += dt;
    if (urgentSave_ || saveTimer_ >= 5.0)
    {
        Save();
        urgentSave_ = false;
        saveTimer_ = 0.0;
    }
}
