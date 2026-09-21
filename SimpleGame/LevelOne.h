#pragma once

#include "NpcSystem.h"
#include "World.h"
#include "GameplayTypes.h"
#include "SceneGraph.h"
#include <cstdint>
#include <filesystem>
#include <map>
#include <random>
#include <string>
#include <utility>
#include <vector>

class LevelOne
{
  public:
    static constexpr int BossKillRequirement = 24;

    LevelOne(const std::filesystem::path& directory, std::uint64_t seed);
    void Update(double dt, const World& world, const NpcSystem& npcs);
    bool Move(double dx, double dy, const World& world, const NpcSystem& npcs);
    void ReturnToCamp();
    bool Save();
    static bool InCamp(WorldPoint position);
    SceneGraph& Scene();
    void SynchronizeActors();
    const Enemy* FindEnemy(std::uint64_t id) const;
    const ItemDrop* FindDrop(std::uint64_t id) const;
    const Projectile* FindProjectile(std::uint64_t id) const;
    const SoulNpc* FindSoul(std::uint64_t id) const;
    const FloatingNumber* FindNumber(std::uint64_t id) const;

    const std::vector<SoulNpc>& Souls() const
    {
        return soulNpcs_;
    }

    int NearestSoul(double x, double y, const World& world) const;
    std::vector<std::wstring> SoulConversation(int index);
    void RecordConversation();

    const PlayerProgress& Player() const
    {
        return player_;
    }

    const std::vector<Enemy>& Enemies() const
    {
        return enemies_;
    }

    const std::vector<ItemDrop>& Drops() const
    {
        return drops_;
    }

    const std::vector<Projectile>& Projectiles() const
    {
        return projectiles_;
    }

    const std::vector<FloatingNumber>& Numbers() const
    {
        return numbers_;
    }

    LevelPhase Phase() const
    {
        return phase_;
    }

    double MagnetTime() const
    {
        return magnetTime_;
    }

    double InvulnerableTime() const
    {
        return invulnerable_;
    }

    double ShotReady() const;

    std::uint64_t Target() const
    {
        return target_;
    }

    const std::wstring& Notice() const
    {
        return notice_;
    }

    const std::string& Error() const
    {
        return error_;
    }

    std::wstring Objective() const;
    std::wstring Hint() const;

  private:
    using Cell = std::pair<WorldInt, WorldInt>;

    struct RouteCell
    {
        Cell next;
        int steps;
    };

    void Load();
    void RebuildRoutes(const World& world, const NpcSystem& npcs);
    bool Spawn(EnemyKind kind, const World& world);
    void StepEnemy(std::uint64_t id, double dt);
    void StepProjectile(std::uint64_t id, double dt);
    void StepDrop(std::uint64_t id, double dt);
    void ResolveCombat();
    void SetPlayerPosition(WorldPoint position);
    void Collect(const ItemDrop& drop);
    void AutoFire(const World& world);
    void Defeat(const Enemy& enemy);
    void Drop(DropKind kind, WorldPoint position, int amount);
    void GainExperience(int amount);
    void Hurt(double amount);
    void Announce(const std::wstring& text);
    void Die();

    SceneGraph scene_;
    const World* activeWorld_ = nullptr;
    const NpcSystem* activeNpcs_ = nullptr;
    std::uint64_t nextRuntimeId_ = 1;
    std::map<std::string, std::shared_ptr<std::map<std::uint64_t, size_t>>> recordIndices_;
    PlayerProgress player_;
    LevelPhase phase_ = LevelPhase::Farming;
    std::vector<Enemy> enemies_;
    std::vector<ItemDrop> drops_;
    std::vector<Projectile> projectiles_;
    std::vector<FloatingNumber> numbers_;
    std::vector<SoulNpc> soulNpcs_;
    LifeRecord life_;
    std::map<Cell, RouteCell> routes_;
    std::mt19937 random_;
    std::uint64_t seed_ = 0;
    std::uint64_t nextId_ = 1;
    std::uint64_t target_ = 0;
    std::filesystem::path savePath_;
    std::string error_;
    std::wstring notice_;
    double noticeTime_ = 0.0;
    double spawnTimer_ = 2.0;
    double routeTimer_ = 0.0;
    double shotTimer_ = 0.0;
    double magnetTime_ = 0.0;
    double invulnerable_ = 0.0;
    double saveTimer_ = 0.0;
    bool urgentSave_ = false;
};
