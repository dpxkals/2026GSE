#include "stdafx.h"
#include "LevelOne.h"
#include "ActorRecords.h"
#include "GameplayBehaviors.h"
#include <cmath>
#include <set>

SceneGraph& LevelOne::Scene()
{
    return scene_;
}

const Enemy* LevelOne::FindEnemy(std::uint64_t id) const
{
    return FindRecord(enemies_, id);
}

const ItemDrop* LevelOne::FindDrop(std::uint64_t id) const
{
    return FindRecord(drops_, id);
}

const Projectile* LevelOne::FindProjectile(std::uint64_t id) const
{
    return FindRecord(projectiles_, id);
}

const SoulNpc* LevelOne::FindSoul(std::uint64_t id) const
{
    return FindRecord(soulNpcs_, id);
}

const FloatingNumber* LevelOne::FindNumber(std::uint64_t id) const
{
    return FindRecord(numbers_, id);
}

void LevelOne::SynchronizeActors()
{
    Actor& root = scene_.Ensure("gameplay", "group");
    if (!scene_.Find("gameplay/player"))
    {
        Actor& player = scene_.Ensure("gameplay/player", "player", root.GetId());
        player.phase = UpdatePhase::Player;
        player.BindPosition(
            [this]()
            {
                return ActorPosition{player_.position.x, player_.position.y};
            },
            [this](ActorPosition position)
            {
                player_.position = {position.x, position.y};
            });
        player.BindUpdate(
            [this](double)
            {
                if (activeWorld_ && phase_ != LevelPhase::Cleared)
                {
                    AutoFire(*activeWorld_);
                }
            });
    }
    for (auto& shot : projectiles_)
    {
        if (!shot.runtimeId)
        {
            shot.runtimeId = nextRuntimeId_++;
        }
    }
    for (auto& number : numbers_)
    {
        if (!number.runtimeId)
        {
            number.runtimeId = nextRuntimeId_++;
        }
    }
    auto bind = [this, &root](auto& records,
                              const std::string& type,
                              UpdatePhase phase,
                              std::function<void(std::uint64_t, double)> step)
    {
        std::string prefix = "gameplay/" + type + "/";
        std::set<std::string> keys;
        auto* data = &records;
        auto& sharedIndex = recordIndices_[type];
        if (!sharedIndex)
        {
            sharedIndex = std::make_shared<std::map<std::uint64_t, size_t>>();
        }
        auto indices = sharedIndex;
        indices->clear();
        for (size_t i = 0; i < records.size(); ++i)
        {
            indices->emplace(RecordId(records[i]), i);
        }
        for (const auto& record : records)
        {
            auto id = RecordId(record);
            auto key = prefix + std::to_string(id);
            keys.insert(key);
            if (scene_.Find(key))
            {
                continue;
            }
            Actor& actor = scene_.Ensure(key, type, root.GetId());
            actor.recordId = id;
            actor.phase = phase;
            actor.layer = type == "number" ? RenderLayer::Effects : RenderLayer::World;
            actor.BindPosition(
                [data, indices, id]()
                {
                    const auto* record = FindIndexedRecord(*data, *indices, id);
                    return record ? ActorPosition{record->position.x, record->position.y}
                                  : ActorPosition{};
                },
                [data, indices, id](ActorPosition position)
                {
                    if (auto* record = FindIndexedRecord(*data, *indices, id))
                    {
                        record->position = {position.x, position.y};
                    }
                });
            actor.BindUpdate(
                [step, id](double dt)
                {
                    if (step)
                    {
                        step(id, dt);
                    }
                });
            actor.BindRemoval(
                [this, data, id]()
                {
                    RemoveRecord(*data, id);
                    urgentSave_ = true;
                });
        }
        scene_.Retain(prefix, keys);
    };
    bind(enemies_,
         "enemy",
         UpdatePhase::Enemies,
         [this](auto id, double dt)
         {
             StepEnemy(id, dt);
         });
    bind(projectiles_,
         "projectile",
         UpdatePhase::Projectiles,
         [this](auto id, double dt)
         {
             StepProjectile(id, dt);
         });
    bind(drops_,
         "drop",
         UpdatePhase::Items,
         [this](auto id, double dt)
         {
             StepDrop(id, dt);
         });
    bind(soulNpcs_, "soul", UpdatePhase::Passive, {});
    bind(numbers_,
         "number",
         UpdatePhase::Effects,
         [this](auto id, double dt)
         {
             if (auto* number = FindRecord(numbers_, id))
             {
                 number->life -= dt;
                 if (number->life <= 0.0)
                 {
                     RemoveRecord(numbers_, id);
                 }
             }
         });
}

void LevelOne::StepEnemy(std::uint64_t id, double dt)
{
    auto* enemy = FindRecord(enemies_, id);
    if (!enemy || !activeWorld_ || !activeNpcs_)
    {
        return;
    }
    Gameplay::EnemyServices services{
        *activeWorld_,
        *activeNpcs_,
        player_.position,
        InCamp,
        [this](WorldPoint point) -> std::optional<WorldPoint>
        {
            Cell cell{WorldInt(std::floor(point.x)), WorldInt(std::floor(point.y))};
            auto found = routes_.find(cell);
            if (found == routes_.end())
            {
                return {};
            }
            return WorldPoint{found->second.next.first + 0.5, found->second.next.second + 0.5};
        },
        [this](double damage)
        {
            Hurt(damage);
        },
        [this](Projectile shot)
        {
            projectiles_.push_back(shot);
        }};
    Gameplay::UpdateEnemy(*enemy, dt, services);
}

void LevelOne::StepProjectile(std::uint64_t id, double dt)
{
    auto* shot = FindRecord(projectiles_, id);
    if (!shot || !activeWorld_)
    {
        return;
    }
    Gameplay::ProjectileServices services{*activeWorld_,
                                          player_.position,
                                          enemies_,
                                          [this](double damage)
                                          {
                                              Hurt(damage);
                                          },
                                          [this](FloatingNumber number)
                                          {
                                              numbers_.push_back(number);
                                          }};
    Gameplay::UpdateProjectile(*shot, dt, services);
    if (shot->remainingRange <= 0.0001)
    {
        RemoveRecord(projectiles_, id);
    }
}

void LevelOne::StepDrop(std::uint64_t id, double dt)
{
    auto* drop = FindRecord(drops_, id);
    if (drop && Gameplay::MovePickup(*drop, dt, player_, magnetTime_))
    {
        Collect(*drop);
        RemoveRecord(drops_, id);
    }
}
