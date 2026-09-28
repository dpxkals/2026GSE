#pragma once
#include "GameplayTypes.h"
#include "World.h"
#include "SceneGraph.h"
#include <array>
#include <deque>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

class LevelOne;
class NpcSystem;

// A presentation label derived from experience, never an AI permission or profession.
enum class NpcTendency
{
    Hunter,
    Raider,
    Merchant,
    Seeker,
    Wanderer,
    Crafter,
    Helper
};
enum class NpcGoal
{
    Vitality,
    Wealth,
    Wisdom
};
enum class NpcAction
{
    Hunt,
    Trade,
    Loot,
    PK,
    Explore,
    Rest,
    Flee,
    Gather,
    Craft,
    Aid,
    Socialize,
    Travel,
    Meditate,
    Count
};
enum class CombatKind
{
    None,
    Monster,
    Citizen,
    Player
};

struct CombatTarget
{
    CombatKind kind = CombatKind::None;
    std::uint64_t id = 0;
    WorldPoint position;
    double health = 0;
};

struct Citizen
{
    std::uint64_t id = 0;
    WorldPoint position;
    int home = 0;
    NpcGoal goal = NpcGoal::Vitality;
    NpcAction action = NpcAction::Rest;
    double health = 90, maxHealth = 90;
    int level = 1, experience = 0, gold = 30, goods = 2, insight = 0;
    // Aggression, greed, sociability, courage, morality, curiosity.
    std::array<double, 6> personality{};
    std::array<double, static_cast<size_t>(NpcAction::Count)> learned{}, weights{};
    bool dirty = true;
    double decision = 0, cooldown = 0, hostility = 0, deadTime = 0, actionTime = 0;
    double pathTimer = 0;
    WorldPoint destination;
    CombatKind targetKind = CombatKind::None;
    std::uint64_t target = 0;
    std::vector<WorldPoint> route;
    int lod = 0;
    double lodTime = 0;
};

struct CitizenLoot
{
    std::uint64_t id = 0;
    WorldPoint position;
    int gold = 0, goods = 0;
    double remaining = 120;
};

struct SocialStrike
{
    WorldPoint from, to;
    double life = 0.16;
};

class WorldSimulation
{
  public:
    WorldSimulation(const std::filesystem::path& directory, std::uint64_t seed);
    void Update(double dt, const World& world, LevelOne& level, const NpcSystem& residents);
    void SynchronizeActors(SceneGraph& scene, const World& world);
    bool Save();

    const std::string& Error() const
    {
        return error_;
    }

    const std::vector<Citizen>& People() const
    {
        return people_;
    }

    const std::vector<CitizenLoot>& Loot() const
    {
        return loot_;
    }

    const std::vector<SocialStrike>& Strikes() const
    {
        return strikes_;
    }

    const std::deque<std::wstring>& News() const
    {
        return news_;
    }

    const Citizen* Find(std::uint64_t id) const;
    Citizen* Find(std::uint64_t id);

    void Select(std::uint64_t id, bool attack = false)
    {
        selected_ = id;
        attackTarget_ = attack ? id : 0;
    }

    bool AttackingSelected() const
    {
        return selected_ != 0 && attackTarget_ == selected_;
    }

    std::uint64_t Selected() const
    {
        return selected_;
    }

    CombatTarget PlayerTarget(WorldPoint player, double range, const World& world) const;
    bool HitProjectile(Projectile& shot, WorldPoint player);
    bool HitHostileProjectile(Projectile& shot);
    void HurtNpc(std::uint64_t id, double amount, std::uint64_t monster);
    std::wstring LootPlayer(WorldPoint position);
    void MonsterKilled(std::uint64_t killer, const Enemy& enemy);
    void PlayerMonsterKilled();
    std::wstring TradePlayer(LevelOne& level, bool training);

    int PlayerGold() const
    {
        return playerGold_;
    }

    int PlayerGoods() const
    {
        return playerGoods_;
    }

    int Crime() const
    {
        return crime_;
    }

    double Time() const
    {
        return time_;
    }

    static std::wstring Name(const Citizen& npc);
    static NpcTendency Tendency(const Citizen& npc);
    static const wchar_t* TendencyName(NpcTendency tendency);
    static const wchar_t* ActionName(NpcAction action);
    static const wchar_t* GoalName(NpcGoal goal);

  private:
    Citizen Create(int home);
    void Learn(Citizen& npc, NpcAction action, double reward);
    bool CivilAction(Citizen& npc, double dt, const World& world);
    void Load();
    void Choose(Citizen& npc, const World& world, const LevelOne& level);
    void Step(Citizen& npc, double dt, const World& world, LevelOne& level);
    void Navigate(Citizen& npc, WorldPoint goal, double dt, const World& world);
    void Damage(Citizen& npc, double amount, CombatKind source, std::uint64_t sourceId);
    void Kill(Citizen& npc, CombatKind source, std::uint64_t sourceId);
    void Record(const std::string& kind,
                std::uint64_t from,
                std::uint64_t to,
                int amount,
                const std::wstring& message);
    void Trade(Citizen& npc);
    void Grow(Citizen& npc, int experience);
    void Ecology(double dt, const World& world, LevelOne& level);
    std::filesystem::path path_;
    std::ofstream events_;
    std::ofstream states_;
    std::uint64_t seed_ = 0, nextId_ = 1, selected_ = 0, attackTarget_ = 0, eventId_ = 0;
    std::mt19937 random_;
    std::vector<Citizen> people_;
    std::vector<CitizenLoot> loot_;
    std::vector<SocialStrike> strikes_;
    std::deque<std::wstring> news_;
    std::array<int, 2> wildStock_{{12, 12}};
    int playerGold_ = 30, playerGoods_ = 0, crime_ = 0;
    double time_ = 0, accumulator_ = 0, saveTimer_ = 0, ecologyTimer_ = 0, stockTimer_ = 0;
    double logTimer_ = 0;
    std::string error_;
};
