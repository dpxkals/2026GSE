#include "stdafx.h"
#include "WorldSimulation.h"
#include "LevelOne.h"
#include "NpcSystem.h"
#include "VillageLayout.h"
#include "GameplayBehaviors.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <map>
#include <set>
#include <iostream>
#include <chrono>

namespace
{
    double Distance(WorldPoint a, WorldPoint b)
    {
        return Gameplay::Distance(a, b);
    }

    constexpr double Pi = 3.141592653589793;

    size_t Index(NpcAction action)
    {
        return static_cast<size_t>(action);
    }
}

WorldSimulation::WorldSimulation(const std::filesystem::path& directory, std::uint64_t seed)
    : path_(directory / L"world_simulation.dat"),
      seed_(seed),
      random_(static_cast<unsigned>(seed ^ (seed >> 32) ^ 0x71C92))
{
    Load();
    events_.open(directory / L"world_events.csv", std::ios::app);
    states_.open(directory / L"npc_states.csv", std::ios::app);
    if (states_)
    {
        states_
            << "# v2: time,id,tendency,lod,action,hp,max_hp,level,gold,goods,insight,hunt,trade,loot,pk,explore,rest,flee,gather,craft,aid,socialize,travel,meditate; followed by learned values in the same order\n";
    }
    if (!events_)
    {
        std::cerr << "[Simulation] Cannot open world_events.csv\n";
    }
    else
    {
        events_ << "# session=" << std::chrono::system_clock::now().time_since_epoch().count()
                << ",seed=" << seed_ << ",columns=event_id,time_s,type,source,target,amount\n";
    }
}

const Citizen* WorldSimulation::Find(std::uint64_t id) const
{
    for (const auto& npc : people_)
    {
        if (npc.id == id)
        {
            return &npc;
        }
    }
    return nullptr;
}

Citizen* WorldSimulation::Find(std::uint64_t id)
{
    return const_cast<Citizen*>(static_cast<const WorldSimulation*>(this)->Find(id));
}

Citizen WorldSimulation::Create(int home)
{
    Citizen npc;
    npc.id = nextId_++;
    npc.home = home;
    auto center = Villages()[home].center;
    double angle = double(random_() % 360) * Pi / 180.0;
    double radius = 1.0 + double(random_() % 12) / 10;
    npc.position = {center.x + std::cos(angle) * radius, center.y + std::sin(angle) * radius};
    npc.destination = npc.position;
    for (auto& trait : npc.personality)
    {
        trait = 0.1 + double(random_() % 81) / 100.0;
    }
    npc.decision = double(npc.id % 10) * 0.15;
    return npc;
}

NpcTendency WorldSimulation::Tendency(const Citizen& npc)
{
    const auto& l = npc.learned;
    std::array<double, 6> scores{
        {l[Index(NpcAction::Hunt)],
         (std::max)(l[Index(NpcAction::Loot)], l[Index(NpcAction::PK)]),
         l[Index(NpcAction::Trade)],
         (std::max)(l[Index(NpcAction::Explore)], l[Index(NpcAction::Meditate)]),
         (std::max)(l[Index(NpcAction::Gather)], l[Index(NpcAction::Craft)]),
         (std::max)(l[Index(NpcAction::Aid)], l[Index(NpcAction::Socialize)])}};
    auto best = std::max_element(scores.begin(), scores.end());
    if (*best < 4)
    {
        return NpcTendency::Wanderer;
    }
    static const NpcTendency labels[] = {NpcTendency::Hunter,
                                         NpcTendency::Raider,
                                         NpcTendency::Merchant,
                                         NpcTendency::Seeker,
                                         NpcTendency::Crafter,
                                         NpcTendency::Helper};
    return labels[best - scores.begin()];
}

const wchar_t* WorldSimulation::TendencyName(NpcTendency tendency)
{
    static const wchar_t* names[] = {L"사냥 성향",
                                     L"약탈 성향",
                                     L"교역 성향",
                                     L"탐구 성향",
                                     L"미분화",
                                     L"생산 성향",
                                     L"협력 성향"};
    return names[int(tendency)];
}

void WorldSimulation::Learn(Citizen& npc, NpcAction action, double reward)
{
    auto before = Tendency(npc);
    auto& value = npc.learned[Index(action)];
    value = std::clamp(value + reward, -20.0, 40.0);
    if (before != Tendency(npc))
    {
        Record("tendency",
               npc.id,
               0,
               int(Tendency(npc)),
               Name(npc) + L"의 경험이 " + TendencyName(Tendency(npc)) + L"으로 기울었다.");
    }
}

const wchar_t* WorldSimulation::GoalName(NpcGoal goal)
{
    static const wchar_t* names[] = {L"체력과 성장", L"부의 축적", L"깨달음"};
    return names[int(goal)];
}

const wchar_t* WorldSimulation::ActionName(NpcAction action)
{
    static const wchar_t* names[] = {L"사냥",
                                     L"거래",
                                     L"약탈",
                                     L"PK",
                                     L"탐구",
                                     L"휴식",
                                     L"도주",
                                     L"채집",
                                     L"제작",
                                     L"구호",
                                     L"교류",
                                     L"마을 방문",
                                     L"명상"};
    return names[int(action)];
}

std::wstring WorldSimulation::Name(const Citizen& npc)
{
    static const wchar_t* names[] = {L"카일",
                                     L"미라",
                                     L"로웬",
                                     L"에다",
                                     L"가렌",
                                     L"루나",
                                     L"세드",
                                     L"노아",
                                     L"아린",
                                     L"브람",
                                     L"레아",
                                     L"오르"};
    return std::wstring(names[(npc.id - 1) % 12]) + L" " + std::to_wstring(npc.id);
}

void WorldSimulation::Record(const std::string& kind,
                             std::uint64_t from,
                             std::uint64_t to,
                             int amount,
                             const std::wstring& message)
{
    ++eventId_;
    if (events_)
    {
        events_ << eventId_ << ',' << time_ << ',' << kind << ',' << from << ',' << to << ','
                << amount << '\n';
    }
    news_.push_front(message);
    while (news_.size() > 5)
    {
        news_.pop_back();
    }
}

void WorldSimulation::Grow(Citizen& npc, int experience)
{
    npc.experience += experience;
    if (npc.level >= MaxCharacterLevel)
    {
        npc.experience = 0;
        return;
    }
    while (npc.experience >= npc.level * 12 && npc.level < MaxCharacterLevel)
    {
        npc.experience -= npc.level * 12;
        ++npc.level;
        npc.maxHealth += 18;
        npc.health = (std::min)(npc.maxHealth, npc.health + 40);
        Record("npc_level", npc.id, 0, npc.level, Name(npc) + L"의 체력이 성장했다.");
    }
    if (npc.level >= MaxCharacterLevel)
    {
        npc.experience = 0;
    }
}

void WorldSimulation::Choose(Citizen& npc, const World& world, const LevelOne& level)
{
    const double hp = npc.health / npc.maxHealth;
    const auto& p = npc.personality;
    const Enemy* monster = nullptr;
    double monsterDistance = 30;
    for (const auto& enemy : level.Enemies())
    {
        double distance = Distance(npc.position, enemy.position);
        if (enemy.health > 0 && distance < monsterDistance &&
            (enemy.kind != EnemyKind::Boss || npc.level >= 10))
        {
            monster = &enemy;
            monsterDistance = distance;
        }
    }
    const Citizen* victim = nullptr;
    double value = 0;
    for (const auto& other : people_)
    {
        double distance = Distance(npc.position, other.position);
        if (other.id == npc.id || other.health <= 0 || distance > 10 ||
            other.gold + other.goods == 0)
        {
            continue;
        }
        double score =
            (other.gold + other.goods * 3.0) / (1 + distance) * npc.health / (other.health + 25);
        if (score > value)
        {
            value = score;
            victim = &other;
        }
    }
    const CitizenLoot* treasure = nullptr;
    double lootDistance = 12;
    for (const auto& item : loot_)
    {
        double distance = Distance(npc.position, item.position);
        if (item.remaining > 0 && distance < lootDistance)
        {
            lootDistance = distance;
            treasure = &item;
        }
    }
    double law = VillageAt(npc.position) >= 0 ? 0.08 : 1.0;
    npc.goal = hp < 0.55 ? NpcGoal::Vitality : npc.gold < 40 ? NpcGoal::Wealth : NpcGoal::Wisdom;
    npc.weights = {38.0 * p[3] * hp * (monster ? 2.0 : 0.6),
                   12.0 * p[2] + (std::min)(npc.goods, 12) * 3.0,
                   treasure ? 18.0 + p[1] * 65.0 : 0.0,
                   victim ? 110.0 * p[0] * p[1] * (1 - p[4]) * hp * law : 0.0,
                   (npc.goal == NpcGoal::Wisdom ? 38.0 : 8.0) * p[5],
                   2.0 + (1 - hp) * 55.0,
                   hp < 0.3 ? 100.0 : 0.0,
                   npc.goods < 20 ? 14 + 15 * p[5] : 0.0,
                   npc.goods >= 2 ? 12 + 15 * p[1] : 0.0,
                   0.0,
                   10 + 18 * p[2],
                   4 + 12 * p[5],
                   5 + 18 * p[5]};
    const Citizen* injured = nullptr;
    for (const auto& other : people_)
    {
        if (other.id != npc.id && other.health > 0 && other.health < other.maxHealth * 0.7 &&
            other.hostility <= 0 && Distance(npc.position, other.position) < 10)
        {
            injured = &other;
            break;
        }
    }
    npc.weights[Index(NpcAction::Aid)] = injured && npc.goods > 0 ? 30 * p[2] * p[4] : 0;
    // Growth and survival goals change scores; experience modifies the base propensity.
    if (npc.goal == NpcGoal::Vitality)
    {
        npc.weights[0] *= 1.4;
    }
    const bool playerClose = Distance(npc.position, level.Player().position) < 9;
    if (playerClose && playerGold_ > 0)
    {
        npc.weights[Index(NpcAction::PK)] += 30 * p[0] * p[1] * (1 - p[4]) * law;
    }
    for (size_t i = 0; i < npc.weights.size(); ++i)
    {
        if (npc.weights[i] > 0)
        {
            npc.weights[i] =
                (std::max)(0.1,
                           npc.weights[i] +
                               npc.learned[i] * (i == Index(NpcAction::PK) ? law : 1.0));
        }
    }
    std::discrete_distribution<int> choice(npc.weights.begin(), npc.weights.end());
    npc.action = static_cast<NpcAction>(choice(random_));
    if (hp < 0.4)
    {
        npc.action = NpcAction::Flee;
    }
    else if (npc.hostility > 0 && playerClose)
    {
        npc.action = NpcAction::PK;
    }
    npc.target = 0;
    npc.targetKind = CombatKind::None;
    npc.destination = Villages()[npc.home].center;
    if (npc.action == NpcAction::Hunt && monster)
    {
        npc.targetKind = CombatKind::Monster;
        npc.target = monster->id;
    }
    if (npc.action == NpcAction::PK)
    {
        if (playerClose && (npc.hostility > 0 || (playerGold_ > 0 && random_() % 3 == 0)))
        {
            npc.targetKind = CombatKind::Player;
            npc.hostility = 18;
        }
        else if (victim)
        {
            npc.targetKind = CombatKind::Citizen;
            npc.target = victim->id;
        }
        else
        {
            npc.action = NpcAction::Explore;
        }
    }
    if (npc.action == NpcAction::Loot && treasure)
    {
        npc.target = treasure->id;
        npc.destination = treasure->position;
    }
    if (npc.action == NpcAction::Aid && injured)
    {
        npc.target = injured->id;
        npc.destination = injured->position;
    }
    if (npc.action == NpcAction::Travel)
    {
        int target = int(random_() % (VillageCount - 1));
        if (target >= npc.home)
        {
            ++target;
        }
        npc.target = std::uint64_t(target + 1);
        npc.travelViaHub = npc.home != 0 && target != 0;
        npc.destination = Villages()[target].center;
    }
    if (npc.action == NpcAction::Gather || npc.action == NpcAction::Explore ||
        (npc.action == NpcAction::Hunt && !monster))
    {
        auto home = Villages()[npc.home].center;
        double angle = double(random_() % 360) * Pi / 180;
        npc.destination = {home.x + std::cos(angle) * 22, home.y + std::sin(angle) * 22};
    }
    npc.decision = 30.0 + double(random_() % 60) / 10;
    if (npc.action == NpcAction::Travel)
    {
        npc.decision = 240;
    }
    npc.route.clear();
    npc.pathTimer = 0;
    npc.actionTime = 0;
    npc.dirty = false;
}

void WorldSimulation::Navigate(Citizen& npc, WorldPoint goal, double dt, const World& world)
{
    if (!world.Find(WorldInt(std::floor(goal.x)), WorldInt(std::floor(goal.y))))
    {
        // Walk toward a distant loaded frontier; never require the destination chunk to be loaded.
        double distance = Distance(npc.position, goal);
        if (distance < 0.01)
        {
            return;
        }
        WorldPoint direction{(goal.x - npc.position.x) / distance,
                             (goal.y - npc.position.y) / distance};
        if (npc.action == NpcAction::Travel && npc.lod == 1 &&
            VillageRoad(npc.position.x - 0.5, npc.position.y - 0.5) &&
            !world.Find(WorldInt(std::floor(npc.position.x + direction.x * 4)),
                        WorldInt(std::floor(npc.position.y + direction.y * 4))))
        {
            // A road frontier outside the near simulation hands off to coarse travel.
            npc.position.x += direction.x * 4;
            npc.position.y += direction.y * 4;
            npc.route.clear();
            return;
        }
        double reach = (std::min)(12.0, distance);
        while (reach > 0.5 &&
               !world.Find(WorldInt(std::floor(npc.position.x + direction.x * reach)),
                           WorldInt(std::floor(npc.position.y + direction.y * reach))))
        {
            reach -= 1;
        }
        if (reach <= 0.5)
        {
            return;
        }
        goal = {npc.position.x + direction.x * reach, npc.position.y + direction.y * reach};
    }
    WorldPoint waypoint = goal;
    if (!world.ClearLine(npc.position.x, npc.position.y, goal.x, goal.y))
    {
        if (npc.pathTimer <= 0)
        {
            npc.pathTimer = 1.5 + double(npc.id % 4) * 0.2;
            using Cell = std::pair<WorldInt, WorldInt>;
            Cell origin{WorldInt(std::floor(npc.position.x)), WorldInt(std::floor(npc.position.y))};
            std::queue<Cell> queue;
            std::map<Cell, Cell> previous;
            queue.push(origin);
            previous[origin] = origin;
            Cell best = origin;
            double closest = Distance(npc.position, goal);
            while (!queue.empty() && previous.size() < 2048)
            {
                Cell cell = queue.front();
                queue.pop();
                double distance = Distance({cell.first + 0.5, cell.second + 0.5}, goal);
                if (distance < closest)
                {
                    closest = distance;
                    best = cell;
                }
                if (distance < 0.8)
                {
                    break;
                }
                for (Cell offset : {Cell{-1, 0}, {1, 0}, {0, -1}, {0, 1}})
                {
                    Cell next{cell.first + offset.first, cell.second + offset.second};
                    if (previous.count(next) || std::abs(next.first - origin.first) > 28 ||
                        std::abs(next.second - origin.second) > 28 ||
                        !world.CanWalk(next.first + 0.5, next.second + 0.5))
                    {
                        continue;
                    }
                    previous[next] = cell;
                    queue.push(next);
                }
            }
            npc.route.clear();
            for (Cell cell = best; cell != origin; cell = previous.at(cell))
            {
                npc.route.push_back({cell.first + 0.5, cell.second + 0.5});
            }
        }
        while (!npc.route.empty() && Distance(npc.position, npc.route.back()) < 0.15)
        {
            npc.route.pop_back();
        }
        if (npc.route.empty())
        {
            return;
        }
        waypoint = npc.route.back();
    }
    double distance = Distance(npc.position, waypoint);
    if (distance < 0.05)
    {
        return;
    }
    double step = (std::min)(distance, dt * (npc.action == NpcAction::Flee ? 3.8 : 2.6));
    double dx = (waypoint.x - npc.position.x) / distance * step;
    double dy = (waypoint.y - npc.position.y) / distance * step;
    int steps = (std::max)(1, int(std::ceil(step / 0.08)));
    for (int i = 0; i < steps; ++i)
    {
        if (world.CanWalk(npc.position.x + dx / steps, npc.position.y))
        {
            npc.position.x += dx / steps;
        }
        if (world.CanWalk(npc.position.x, npc.position.y + dy / steps))
        {
            npc.position.y += dy / steps;
        }
    }
}

void WorldSimulation::Trade(Citizen& npc)
{
    if (VillageAt(npc.position) != npc.home || npc.cooldown > 0)
    {
        return;
    }
    for (auto& buyer : people_)
    {
        if (buyer.id == npc.id || buyer.health <= 0 || buyer.hostility > 0 ||
            Distance(npc.position, buyer.position) > 3 || VillageAt(buyer.position) != npc.home)
        {
            continue;
        }
        if (npc.goods > 0 && buyer.gold >= 3 && buyer.goods < 30)
        {
            --npc.goods;
            ++buyer.goods;
            npc.gold += 3;
            buyer.gold -= 3;
            Learn(npc, NpcAction::Trade, 2);
            Learn(buyer, NpcAction::Trade, 1);
            buyer.dirty = true;
            npc.cooldown = 3;
            Record("trade", npc.id, buyer.id, 3, Name(npc) + L"가 마을에서 재료를 거래했다.");
            return;
        }
    }
}

void WorldSimulation::Kill(Citizen& npc, CombatKind source, std::uint64_t sourceId)
{
    npc.health = 0;
    npc.deadTime = 60;
    npc.hostility = 0;
    loot_.push_back({nextId_++, npc.position, npc.gold, npc.goods, 120});
    npc.gold = npc.goods = 0;
    if (selected_ == npc.id)
    {
        selected_ = 0;
        attackTarget_ = 0;
    }
    if (source == CombatKind::Citizen)
    {
        if (auto* killer = Find(sourceId))
        {
            Learn(*killer, NpcAction::PK, 5);
            Learn(*killer, NpcAction::Loot, 3);
            killer->action = NpcAction::Loot;
            killer->target = loot_.back().id;
            killer->destination = npc.position;
            killer->decision = 12;
            Grow(*killer, 6);
        }
    }
    Record("npc_death", sourceId, npc.id, int(source), Name(npc) + L"가 쓰러져 소지품을 남겼다.");
}

void WorldSimulation::Damage(Citizen& npc, double amount, CombatKind source, std::uint64_t sourceId)
{
    if (npc.health <= 0 || amount <= 0)
    {
        return;
    }
    if (source == CombatKind::Monster)
    {
        if (npc.hurtCooldown > 0)
        {
            return;
        }
        npc.hurtCooldown = 0.65;
        amount *= 0.65;
    }
    npc.health = (std::max)(0.0, npc.health - amount);
    Learn(npc, npc.action, -1);
    Learn(npc, NpcAction::Flee, 1);
    npc.dirty = true;
    if (source == CombatKind::Player)
    {
        npc.hostility = 30;
        ++crime_;
        for (auto& witness : people_)
        {
            if (witness.id == npc.id || witness.health <= 0 ||
                Distance(witness.position, npc.position) > 7)
            {
                continue;
            }
            Learn(witness, NpcAction::Flee, 4);
            witness.dirty = true;
            if (witness.personality[3] > 0.6)
            {
                witness.hostility = 15;
            }
        }
        Record("player_assault",
               0,
               npc.id,
               int(amount),
               Name(npc) + L"가 공격받았다. 목격자들이 반응한다.");
    }
    if (npc.health <= 0)
    {
        Kill(npc, source, sourceId);
    }
}

void WorldSimulation::HurtNpc(std::uint64_t id, double amount, std::uint64_t monster)
{
    if (auto* npc = Find(id))
    {
        Damage(*npc, amount, CombatKind::Monster, monster);
        if (npc->health > 0)
        {
            npc->action = npc->health < npc->maxHealth * 0.4 ? NpcAction::Flee : NpcAction::Hunt;
            npc->targetKind = CombatKind::Monster;
            npc->target = monster;
            npc->decision = 12;
            npc->dirty = false;
        }
    }
}

void WorldSimulation::Step(Citizen& npc, double dt, const World& world, LevelOne& level)
{
    npc.cooldown -= dt;
    npc.pathTimer -= dt;
    npc.decision -= dt;
    npc.actionTime += dt;
    npc.hostility = (std::max)(0.0, npc.hostility - dt);
    if (npc.dirty || npc.decision <= 0)
    {
        if (!npc.dirty)
        {
            Learn(npc, npc.action, -0.5);
        }
        Choose(npc, world, level);
    }
    if (CivilAction(npc, dt, world))
    {
        return;
    }
    if (npc.action == NpcAction::Rest || npc.action == NpcAction::Flee ||
        npc.action == NpcAction::Trade)
    {
        Navigate(npc, Villages()[npc.home].center, dt, world);
        if (VillageAt(npc.position) >= 0)
        {
            double before = npc.health;
            npc.health = (std::min)(npc.maxHealth, npc.health + dt * 7);
            if (before < npc.maxHealth && npc.health >= npc.maxHealth &&
                npc.action != NpcAction::Trade)
            {
                Learn(npc, npc.action, 2);
                npc.dirty = true;
            }
            if (npc.action == NpcAction::Trade)
            {
                Trade(npc);
            }
            if (npc.health < npc.maxHealth - 15 && npc.goods > 0 && npc.cooldown <= 0)
            {
                --npc.goods;
                npc.health = (std::min)(npc.maxHealth, npc.health + 20);
                npc.cooldown = 4;
            }
        }
        return;
    }
    if (npc.action == NpcAction::Loot)
    {
        for (auto& bag : loot_)
        {
            if (bag.id != npc.target || bag.remaining <= 0)
            {
                continue;
            }
            Navigate(npc, bag.position, dt, world);
            if (Distance(npc.position, bag.position) < 0.8)
            {
                npc.gold += bag.gold;
                npc.goods += bag.goods;
                Learn(npc, NpcAction::Loot, 3);
                bag.remaining = 0;
                Record(
                    "loot", npc.id, bag.id, bag.gold, Name(npc) + L"가 약탈품으로 자산을 늘렸다.");
                npc.dirty = true;
            }
            return;
        }
        npc.dirty = true;
        return;
    }
    CombatTarget target;
    if (npc.targetKind == CombatKind::Monster)
    {
        if (const auto* enemy = level.FindEnemy(npc.target))
        {
            target = {CombatKind::Monster, enemy->id, enemy->position, enemy->health};
        }
    }
    if (npc.targetKind == CombatKind::Citizen)
    {
        if (const auto* other = Find(npc.target))
        {
            target = {CombatKind::Citizen, other->id, other->position, other->health};
        }
    }
    if (npc.targetKind == CombatKind::Player)
    {
        target = {CombatKind::Player, 0, level.Player().position, level.Player().health};
    }
    if (npc.targetKind != CombatKind::None &&
        (target.kind == CombatKind::None || target.health <= 0))
    {
        npc.dirty = true;
        return;
    }
    if (target.kind != CombatKind::None && target.health > 0)
    {
        Navigate(npc, target.position, dt, world);
        if (npc.cooldown <= 0 && Distance(npc.position, target.position) < 1.8 &&
            world.ClearLine(npc.position.x, npc.position.y, target.position.x, target.position.y))
        {
            double damage = 11 + npc.level * 2.4;
            strikes_.push_back({npc.position, target.position, 0.16});
            npc.cooldown = 0.85;
            if (target.kind == CombatKind::Monster)
            {
                level.DamageMonster(target.id, damage, npc.id);
            }
            if (target.kind == CombatKind::Citizen)
            {
                if (auto* other = Find(target.id))
                {
                    Damage(*other, damage, CombatKind::Citizen, npc.id);
                    if (other->health > 0)
                    {
                        other->action = other->health < other->maxHealth * 0.3 ? NpcAction::Flee
                                                                               : NpcAction::PK;
                        other->targetKind = CombatKind::Citizen;
                        other->target = npc.id;
                        other->decision = 6;
                        other->dirty = false;
                    }
                }
            }
            if (target.kind == CombatKind::Player)
            {
                double before = level.Player().health;
                level.ReceiveNpcDamage(damage);
                if (before > 0 && level.Player().health <= 0 && playerGold_ > 0)
                {
                    int stolen = (std::min)(playerGold_, 25);
                    playerGold_ -= stolen;
                    npc.gold += stolen;
                    Learn(npc, NpcAction::PK, 4);
                    Record(
                        "player_robbed", npc.id, 0, stolen, Name(npc) + L"에게 금화를 빼앗겼다.");
                }
            }
        }
        return;
    }
    Navigate(npc, npc.destination, dt, world);
    if (npc.action == NpcAction::Explore && npc.actionTime >= 8 && VillageAt(npc.position) < 0 &&
        Distance(npc.position, npc.destination) < 2.5)
    {
        ++npc.insight;
        Grow(npc, 3);
        Learn(npc, NpcAction::Explore, 2);
        Record("insight", npc.id, 0, npc.insight, Name(npc) + L"가 범람에 대한 깨달음을 얻었다.");
        npc.dirty = true;
    }
}

bool WorldSimulation::CivilAction(Citizen& npc, double dt, const World& world)
{
    if (npc.action < NpcAction::Gather)
    {
        return false;
    }
    if (npc.action == NpcAction::Travel)
    {
        WorldPoint waypoint = npc.travelViaHub ? Villages()[0].center : npc.destination;
        Navigate(npc, waypoint, dt, world);
        if (Distance(npc.position, waypoint) < 1.5)
        {
            if (npc.travelViaHub)
            {
                npc.travelViaHub = false;
                npc.route.clear();
                npc.pathTimer = 0;
            }
            else
            {
                npc.home = int(npc.target) - 1;
                ++npc.insight;
                Learn(npc, npc.action, 3);
                Record("travel", npc.id, 0, npc.home, Name(npc) + L"가 다른 성채에 도착했다.");
                npc.dirty = true;
            }
        }
        return true;
    }
    if (npc.action == NpcAction::Aid)
    {
        auto* patient = Find(npc.target);
        if (!patient || patient->health <= 0 || patient->health >= patient->maxHealth ||
            patient->hostility > 0 || npc.goods <= 0)
        {
            npc.dirty = true;
            return true;
        }
        Navigate(npc, patient->position, dt, world);
        if (Distance(npc.position, patient->position) < 1.5 && npc.actionTime >= 2)
        {
            --npc.goods;
            patient->health = (std::min)(patient->maxHealth, patient->health + 25);
            Learn(npc, NpcAction::Aid, 4);
            Learn(*patient, NpcAction::Socialize, 1);
            Record("aid", npc.id, patient->id, 25, Name(npc) + L"가 다친 주민을 치료했다.");
            npc.dirty = true;
        }
        return true;
    }
    Navigate(npc, npc.destination, dt, world);
    if (Distance(npc.position, npc.destination) > 1.5 || npc.actionTime < 8)
    {
        return true;
    }
    if (npc.action == NpcAction::Gather)
    {
        if (VillageAt(npc.position) < 0 && wildStock_[npc.home] > 0)
        {
            --wildStock_[npc.home];
            ++npc.goods;
            Learn(npc, npc.action, 3);
            Record("gather", npc.id, 0, 1, Name(npc) + L"가 야외에서 재료를 채집했다.");
        }
        else
        {
            Learn(npc, npc.action, -1);
        }
    }
    else if (VillageAt(npc.position) < 0)
    {
        return true;
    }
    else if (npc.action == NpcAction::Craft && npc.goods >= 2)
    {
        npc.goods -= 2;
        Grow(npc, 5);
        Learn(npc, npc.action, 3);
        Record("craft", npc.id, 0, 2, Name(npc) + L"가 재료로 도구를 제작하며 숙련을 쌓았다.");
    }
    else if (npc.action == NpcAction::Meditate)
    {
        ++npc.insight;
        Grow(npc, 2);
        Learn(npc, npc.action, 2);
        Record("meditate", npc.id, 0, 1, Name(npc) + L"가 명상으로 깨달음을 얻었다.");
    }
    else if (npc.action == NpcAction::Socialize)
    {
        for (auto& other : people_)
        {
            if (other.id == npc.id || other.health <= 0 || other.hostility > 0 ||
                Distance(npc.position, other.position) > 3)
            {
                continue;
            }
            ++npc.insight;
            Learn(npc, npc.action, 2);
            Learn(other, NpcAction::Socialize, 1);
            Record("socialize", npc.id, other.id, 1, Name(npc) + L"가 주민과 경험을 나눴다.");
            break;
        }
    }
    npc.dirty = true;
    return true;
}

void WorldSimulation::Ecology(double dt, const World& world, LevelOne& level)
{
    ecologyTimer_ -= dt;
    stockTimer_ += dt;
    if (stockTimer_ >= 20)
    {
        stockTimer_ = 0;
        for (auto& stock : wildStock_)
        {
            stock = (std::min)(12, stock + 1);
        }
    }
    if (ecologyTimer_ > 0)
    {
        return;
    }
    ecologyTimer_ = 3;
    for (int region = 0; region < VillageCount; ++region)
    {
        auto center = Villages()[region].center;
        int count = 0;
        for (const auto& enemy : level.Enemies())
        {
            if (enemy.health > 0 && Distance(center, enemy.position) < 30)
            {
                ++count;
            }
        }
        if (count >= 7)
        {
            continue;
        }
        for (int attempt = 0; attempt < 12; ++attempt)
        {
            double angle = double(random_() % 360) * Pi / 180;
            WorldPoint location{center.x + std::cos(angle) * 22, center.y + std::sin(angle) * 22};
            if (level.SpawnWild(
                    location, random_() % 3 ? EnemyKind::Husk : EnemyKind::Hound, world))
            {
                break;
            }
        }
    }
}

void WorldSimulation::Update(double dt,
                             const World& world,
                             LevelOne& level,
                             const NpcSystem& residents)
{
    if (!error_.empty() || !world.Error().empty() || !level.Error().empty())
    {
        return;
    }
    accumulator_ += std::clamp(dt, 0.0, 0.1);
    while (accumulator_ >= 0.1)
    {
        accumulator_ -= 0.1;
        time_ += 0.1;
        saveTimer_ += 0.1;
        logTimer_ += 0.1;
        Ecology(0.1, world, level);
        for (auto& npc : people_)
        {
            npc.hurtCooldown = (std::max)(0.0, npc.hurtCooldown - 0.1);
            if (npc.health <= 0)
            {
                npc.deadTime -= 0.1;
                if (npc.deadTime <= 0)
                {
                    auto home = npc.home;
                    npc = Create(home);
                    Record("immigration", npc.id, 0, home, Name(npc) + L"가 마을에 새로 정착했다.");
                }
                continue;
            }
            for (auto& weight : npc.learned)
            {
                weight *= std::exp(-0.1 / 600.0);
            }
            bool loaded = world.Find(WorldInt(std::floor(npc.position.x)),
                                     WorldInt(std::floor(npc.position.y))) != nullptr;
            npc.lod = loaded ? (Distance(npc.position, level.Player().position) < 22 ? 0 : 1) : 2;
            if (loaded)
            {
                if (!world.CanWalk(npc.position.x, npc.position.y))
                {
                    auto center = Villages()[npc.home].center;
                    bool relocated = false;
                    for (int y = -3; y <= 3 && !relocated; ++y)
                    {
                        for (int x = -3; x <= 3; ++x)
                        {
                            WorldPoint candidate{center.x + x, center.y + y};
                            if (residents.CanWalk(candidate.x, candidate.y, world))
                            {
                                npc.position = candidate;
                                npc.route.clear();
                                relocated = true;
                                break;
                            }
                        }
                    }
                    if (!relocated)
                    {
                        continue;
                    }
                }
                npc.lodTime += 0.1;
                if (npc.lodTime >= (npc.lod == 0 ? 0.1 : 0.5))
                {
                    Step(npc, npc.lodTime, world, level);
                    npc.lodTime = 0;
                }
            }
            else
            {
                // No precise off-screen attacks. Bounded regional stock feeds coarse hunting.
                npc.hostility = (std::max)(0.0, npc.hostility - 0.1);
                npc.actionTime += 0.1;
                if (npc.actionTime >= 15)
                {
                    npc.actionTime = 0;
                    npc.cooldown -= 15;
                    if (npc.action != NpcAction::Travel || npc.dirty)
                    {
                        Choose(npc, world, level);
                    }
                    if ((npc.action == NpcAction::Hunt || npc.action == NpcAction::Gather) &&
                        wildStock_[npc.home] > 0 && npc.health > 30)
                    {
                        --wildStock_[npc.home];
                        ++npc.goods;
                        npc.health -= 5;
                        Grow(npc, 12);
                        Learn(npc, npc.action, 2);
                        Record("lod_hunt",
                               npc.id,
                               0,
                               1,
                               Name(npc) + L"가 먼 사냥터에서 돌아올 준비를 한다.");
                    }
                    else if (npc.action == NpcAction::Explore || npc.action == NpcAction::Meditate)
                    {
                        ++npc.insight;
                        Learn(npc, npc.action, 2);
                    }
                    else if (npc.action == NpcAction::Craft && npc.goods >= 2)
                    {
                        npc.goods -= 2;
                        Grow(npc, 5);
                        Learn(npc, npc.action, 3);
                        Record("lod_craft",
                               npc.id,
                               0,
                               2,
                               Name(npc) + L"가 먼 마을에서 제작 숙련을 쌓았다.");
                    }
                    else if (npc.action == NpcAction::Travel)
                    {
                        WorldPoint waypoint =
                            npc.travelViaHub ? Villages()[0].center : npc.destination;
                        double distance = Distance(npc.position, waypoint);
                        double step = (std::min)(distance, 15.0 * 2.6);
                        if (distance > 0.01)
                        {
                            npc.position.x += (waypoint.x - npc.position.x) / distance * step;
                            npc.position.y += (waypoint.y - npc.position.y) / distance * step;
                        }
                        if (distance <= step + 1.5)
                        {
                            if (npc.travelViaHub)
                            {
                                npc.travelViaHub = false;
                            }
                            else
                            {
                                npc.home = int(npc.target) - 1;
                                ++npc.insight;
                                Learn(npc, npc.action, 3);
                                npc.dirty = true;
                            }
                        }
                        continue;
                    }
                    else
                    {
                        npc.health = (std::min)(npc.maxHealth, npc.health + 10);
                    }
                    // Coarse return is only allowed where no physical Actor exists.
                    npc.position = Villages()[npc.home].center;
                    if (npc.action == NpcAction::Trade)
                    {
                        Trade(npc);
                    }
                    npc.dirty = true;
                }
            }
        }
        for (auto& bag : loot_)
        {
            bag.remaining -= 0.1;
        }
        loot_.erase(std::remove_if(loot_.begin(),
                                   loot_.end(),
                                   [](const auto& bag)
                                   {
                                       return bag.remaining <= 0;
                                   }),
                    loot_.end());
        for (auto& strike : strikes_)
        {
            strike.life -= 0.1;
        }
        strikes_.erase(std::remove_if(strikes_.begin(),
                                      strikes_.end(),
                                      [](const auto& strike)
                                      {
                                          return strike.life <= 0;
                                      }),
                       strikes_.end());
        if (saveTimer_ >= 5)
        {
            Save();
            saveTimer_ = 0;
            if (events_)
            {
                events_.flush();
            }
        }
        if (logTimer_ >= 5)
        {
            std::array<int, 3> lod{};
            int wealth = 0, alive = 0;
            for (const auto& npc : people_)
            {
                if (npc.health > 0)
                {
                    ++alive;
                    ++lod[npc.lod];
                    wealth += npc.gold;
                }
            }
            if (states_)
            {
                for (const auto& npc : people_)
                {
                    states_ << time_ << ',' << npc.id << ',' << int(Tendency(npc)) << ',' << npc.lod
                            << ',' << int(npc.action) << ',' << npc.health << ',' << npc.maxHealth
                            << ',' << npc.level << ',' << npc.gold << ',' << npc.goods << ','
                            << npc.insight;
                    for (double weight : npc.weights)
                    {
                        states_ << ',' << weight;
                    }
                    for (double weight : npc.learned)
                    {
                        states_ << ',' << weight;
                    }
                    states_ << '\n';
                }
                states_.flush();
            }
            std::cout << "[Simulation] t=" << int(time_) << " population=" << alive << '/'
                      << VillagePopulation << " LOD=" << lod[0] << "/" << lod[1] << "/" << lod[2]
                      << " gold=" << wealth << " events=" << eventId_ << '\n';
            logTimer_ = 0;
        }
    }
}
