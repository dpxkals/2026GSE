#pragma once

#include <cstdint>
#include <string>

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
    std::uint64_t runtimeId = 0;
};

struct FloatingNumber
{
    WorldPoint position;
    int amount = 0;
    bool playerHit = false;
    double life = 0.75;
    std::uint64_t runtimeId = 0;
};

enum class LevelPhase
{
    Farming,
    BossFight,
    Cleared
};
