#include "stdafx.h"
#include "LevelOne.h"
#include <Windows.h>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <set>
#include <stdexcept>

namespace
{
    void WriteLife(std::ostream& out, const LifeRecord& life)
    {
        out << life.seconds << ' ' << life.distance << ' ' << life.damageTaken << ' ' << life.shots
            << ' ' << life.kills << ' ' << life.souls << ' ' << life.heals << ' '
            << life.conversations << '\n';
    }

    void ReadLife(std::istream& in, LifeRecord& life)
    {
        if (!(in >> life.seconds >> life.distance >> life.damageTaken >> life.shots >> life.kills >>
              life.souls >> life.heals >> life.conversations))
        {
            throw std::runtime_error("Invalid life record");
        }
        for (double value : {life.seconds, life.distance, life.damageTaken})
        {
            if (!std::isfinite(value) || value < 0.0 || value > 1.0e12)
            {
                throw std::runtime_error("Invalid life measurements");
            }
        }
        for (int count : {life.shots, life.kills, life.souls, life.heals, life.conversations})
        {
            if (count < 0 || count > 1000000000)
            {
                throw std::runtime_error("Invalid life counters");
            }
        }
    }

    bool ValidPoint(WorldPoint point)
    {
        return std::isfinite(point.x) && std::isfinite(point.y) && std::abs(point.x) < 1.0e12 &&
               std::abs(point.y) < 1.0e12;
    }
}

bool LevelOne::Save()
{
    if (!error_.empty())
    {
        return false;
    }
    try
    {
        auto temporary = savePath_;
        temporary += L".tmp";
        std::ofstream out(temporary, std::ios::trunc);
        out << std::setprecision(17);
        out << "GSE_LEVEL_ONE 2 " << seed_ << '\n';
        out << player_.position.x << ' ' << player_.position.y << ' ' << player_.level << ' '
            << player_.experience << ' ' << player_.health << ' ' << player_.weaponRank << ' '
            << player_.kills << ' ' << player_.retreats << ' ' << player_.souls << ' '
            << player_.upgrades << ' ' << player_.potions << ' ' << player_.magnets << '\n';
        out << int(phase_) << ' ' << nextId_ << ' ' << magnetTime_ << '\n';
        out << enemies_.size() << '\n';
        for (const Enemy& enemy : enemies_)
        {
            out << enemy.id << ' ' << int(enemy.kind) << ' ' << enemy.position.x << ' '
                << enemy.position.y << ' ' << enemy.health << ' ' << enemy.maxHealth << '\n';
        }
        out << drops_.size() << '\n';
        for (const ItemDrop& drop : drops_)
        {
            out << drop.id << ' ' << int(drop.kind) << ' ' << drop.position.x << ' '
                << drop.position.y << ' ' << drop.amount << '\n';
        }
        WriteLife(out, life_);
        out << soulNpcs_.size() << '\n';
        for (const SoulNpc& soul : soulNpcs_)
        {
            out << soul.id << ' ' << soul.deathNumber << ' ' << soul.position.x << ' '
                << soul.position.y << ' ' << soul.inheritedLevel << ' ' << soul.inheritedWeapon
                << ' ' << soul.meetings << ' ' << soul.feeling << '\n';
            WriteLife(out, soul.life);
        }
        out << random_ << '\n';
        out.flush();
        if (!out)
        {
            throw std::runtime_error("Cannot write level checkpoint");
        }
        out.close();
        if (!out || !MoveFileExW(temporary.c_str(),
                                 savePath_.c_str(),
                                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            throw std::runtime_error("Cannot commit level checkpoint");
        }
        return true;
    }
    catch (const std::exception& e)
    {
        error_ = std::string("LEVEL SAVE: ") + e.what();
        return false;
    }
}

void LevelOne::Load()
{
    try
    {
        if (!std::filesystem::exists(savePath_))
        {
            Save();
            return;
        }
        std::ifstream in(savePath_);
        std::string magic;
        int version = 0;
        std::uint64_t seed = 0;
        if (!(in >> magic >> version >> seed) || magic != "GSE_LEVEL_ONE" ||
            (version != 1 && version != 2) || seed != seed_)
        {
            throw std::runtime_error("Invalid level header; original preserved");
        }

        PlayerProgress player;
        if (!(in >> player.position.x >> player.position.y >> player.level >> player.experience >>
              player.health >> player.weaponRank >> player.kills >> player.retreats >>
              player.souls >> player.upgrades >> player.potions >> player.magnets) ||
            !ValidPoint(player.position) || player.level < 1 || player.level > 30 ||
            player.experience < 0 || player.experience >= player.ExperienceNeeded() ||
            !std::isfinite(player.health) || player.health <= 0.0 ||
            player.health > player.MaxHealth() || player.weaponRank < 0 || player.weaponRank > 20)
        {
            throw std::runtime_error("Invalid player data; original preserved");
        }
        for (int count : {player.kills,
                          player.retreats,
                          player.souls,
                          player.upgrades,
                          player.potions,
                          player.magnets})
        {
            if (count < 0 || count > 1000000)
            {
                throw std::runtime_error("Invalid player counters; original preserved");
            }
        }

        int phase = 0;
        std::uint64_t nextId = 0;
        double magnet = 0.0;
        if (!(in >> phase >> nextId >> magnet) || phase < 0 || phase > 2 || nextId == 0 ||
            !std::isfinite(magnet) || magnet < 0.0 || magnet > 8.0)
        {
            throw std::runtime_error("Invalid level progress; original preserved");
        }
        std::vector<Enemy> enemies;
        std::vector<ItemDrop> drops;
        std::set<std::uint64_t> ids;
        size_t count = 0;
        int bosses = 0;
        if (!(in >> count) || count > 1000000)
        {
            throw std::runtime_error("Invalid enemy count");
        }
        for (size_t i = 0; i < count; ++i)
        {
            Enemy enemy;
            int kind = 0;
            if (!(in >> enemy.id >> kind >> enemy.position.x >> enemy.position.y >> enemy.health >>
                  enemy.maxHealth) ||
                kind < 0 || kind > 2 || !ValidPoint(enemy.position) ||
                !std::isfinite(enemy.health) || !std::isfinite(enemy.maxHealth) ||
                enemy.health <= 0.0 || enemy.maxHealth > 10000.0 ||
                enemy.health > enemy.maxHealth || enemy.id == 0 || enemy.id >= nextId ||
                !ids.insert(enemy.id).second)
            {
                throw std::runtime_error("Invalid enemy data; original preserved");
            }
            enemy.kind = static_cast<EnemyKind>(kind);
            enemy.attackTimer = 2.0;
            enemy.attackPoint = enemy.position;
            bosses += enemy.kind == EnemyKind::Boss ? 1 : 0;
            enemies.push_back(enemy);
        }
        if ((phase == int(LevelPhase::BossFight) && bosses != 1) ||
            (phase != int(LevelPhase::BossFight) && bosses != 0))
        {
            throw std::runtime_error("Inconsistent boss state; original preserved");
        }
        if (!(in >> count) || count > 1000000)
        {
            throw std::runtime_error("Invalid drop count");
        }
        for (size_t i = 0; i < count; ++i)
        {
            ItemDrop drop;
            int kind = 0;
            if (!(in >> drop.id >> kind >> drop.position.x >> drop.position.y >> drop.amount) ||
                kind < 0 || kind > 3 || !ValidPoint(drop.position) || drop.amount <= 0 ||
                drop.amount > 10000 || drop.id == 0 || drop.id >= nextId ||
                !ids.insert(drop.id).second)
            {
                throw std::runtime_error("Invalid item data; original preserved");
            }
            drop.kind = static_cast<DropKind>(kind);
            drops.push_back(drop);
        }
        LifeRecord life;
        std::vector<SoulNpc> souls;
        if (version >= 2)
        {
            ReadLife(in, life);
            if (!(in >> count) || count > 1000000)
            {
                throw std::runtime_error("Invalid soul count");
            }
            std::set<int> deaths;
            for (size_t i = 0; i < count; ++i)
            {
                SoulNpc soul;
                if (!(in >> soul.id >> soul.deathNumber >> soul.position.x >> soul.position.y >>
                      soul.inheritedLevel >> soul.inheritedWeapon >> soul.meetings >>
                      soul.feeling) ||
                    soul.id == 0 || soul.id >= nextId || !ids.insert(soul.id).second ||
                    soul.deathNumber <= 0 || soul.deathNumber > player.retreats ||
                    !deaths.insert(soul.deathNumber).second || !ValidPoint(soul.position) ||
                    soul.inheritedLevel < 1 || soul.inheritedLevel > 30 ||
                    soul.inheritedWeapon < 0 || soul.inheritedWeapon > 20 || soul.meetings < 0 ||
                    soul.meetings > 1000000000 || soul.feeling < 0 || soul.feeling > 2)
                {
                    throw std::runtime_error("Invalid soul identity; original preserved");
                }
                ReadLife(in, soul.life);
                souls.push_back(soul);
            }
        }
        std::mt19937 random;
        if (!(in >> random))
        {
            throw std::runtime_error("Invalid random generator state; original preserved");
        }

        if (version == 1)
        {
            auto backup = savePath_;
            backup += L".v1.bak";
            if (!std::filesystem::exists(backup))
            {
                std::filesystem::copy_file(savePath_, backup);
            }
        }
        player_ = player;
        enemies_ = std::move(enemies);
        drops_ = std::move(drops);
        phase_ = static_cast<LevelPhase>(phase);
        nextId_ = nextId;
        magnetTime_ = magnet;
        random_ = random;
        life_ = life;
        soulNpcs_ = std::move(souls);
        invulnerable_ = 2.0;
    }
    catch (const std::exception& e)
    {
        error_ = std::string("LEVEL LOAD: ") + e.what();
    }
}
