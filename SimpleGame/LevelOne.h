#pragma once

#include "NpcSystem.h"
#include "World.h"
#include <cstdint>
#include <filesystem>
#include <map>
#include <random>
#include <string>
#include <utility>
#include <vector>

struct WorldPoint
{
    double x = 0.5;
    double y = 0.5;
};

struct PlayerProgress
{
    WorldPoint position;
    int level = 1;
    int experience = 0;
    double health = 100.0;
    int weaponRank = 0;
    int kills = 0;
    int retreats = 0;
    int souls = 0;
    int upgrades = 0;
    int potions = 0;
    int magnets = 0;

    int ExperienceNeeded() const;
    double MaxHealth() const;
    double Damage() const;
    double ShotCooldown() const;
    double MoveSpeed() const;
    double PickupRange() const;
    double AttackRange() const;
};

// One life, reset only after a death has produced its own persistent soul.
struct LifeRecord
{
    double seconds = 0.0;
    double distance = 0.0;
    double damageTaken = 0.0;
    int shots = 0;
    int kills = 0;
    int souls = 0;
    int heals = 0;
    int conversations = 0;
};

struct SoulNpc
{
    std::uint64_t id = 0;
    int deathNumber = 0;
    WorldPoint position;
    LifeRecord life;
    int inheritedLevel = 1;
    int inheritedWeapon = 0;
    int meetings = 0;
    // Game-character emotional echo, not an assessment of the human player.
    int feeling = 0;

    std::wstring Name() const;
    std::wstring Role() const;
};

enum class EnemyKind
{
    Husk,
    Hound,
    Boss
};

struct Enemy
{
    std::uint64_t id = 0;
    EnemyKind kind = EnemyKind::Husk;
    WorldPoint position;
    double health = 0.0;
    double maxHealth = 0.0;
    double attackTimer = 1.0;
    double windup = 0.0;
    WorldPoint attackPoint;
    double hitFlash = 0.0;
};

enum class DropKind
{
    Soul,
    Upgrade,
    Health,
    Magnet
};

struct ItemDrop
{
    std::uint64_t id = 0;
    DropKind kind = DropKind::Soul;
    WorldPoint position;
    int amount = 1;
};

struct Projectile
{
    WorldPoint position;
    double velocityX = 0.0;
    double velocityY = 0.0;
    double remainingRange = 0.0;
    double damage = 0.0;
    bool hostile = false;
};

struct FloatingNumber
{
    WorldPoint position;
    int amount = 0;
    bool playerHit = false;
    double life = 0.75;
};

enum class LevelPhase
{
    Farming,
    BossFight,
    Cleared
};

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
    void UpdateEnemies(double dt, const World& world, const NpcSystem& npcs);
    void UpdateProjectiles(double dt, const World& world);
    void UpdateDrops(double dt);
    void AutoFire(const World& world);
    void Defeat(const Enemy& enemy);
    void Drop(DropKind kind, WorldPoint position, int amount);
    void GainExperience(int amount);
    void Hurt(double amount);
    void Announce(const std::wstring& text);
    void Die();

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
