#include "stdafx.h"
#include "WorldSimulation.h"
#include <Windows.h>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <set>
#include <stdexcept>

namespace
{
    bool Finite(double value, double low, double high)
    {
        return std::isfinite(value) && value >= low && value <= high;
    }
}

bool WorldSimulation::Save()
{
    if (!error_.empty())
    {
        return false;
    }
    try
    {
        if (events_.is_open())
        {
            events_.flush();
        }
        if (states_.is_open())
        {
            states_.flush();
        }
        auto temp = path_;
        temp += L".tmp";
        std::ofstream out(temp);
        out << std::setprecision(17) << "GSE_SOCIETY 2 " << seed_ << '\n';
        out << nextId_ << ' ' << eventId_ << ' ' << time_ << ' ' << playerGold_ << ' '
            << playerGoods_ << ' ' << crime_ << ' ' << wildStock_[0] << ' ' << wildStock_[1]
            << '\n';
        out << people_.size() << '\n';
        for (const auto& n : people_)
        {
            out << n.id << ' ' << n.home << ' ' << int(Tendency(n)) << ' ' << int(n.goal) << ' '
                << n.position.x << ' ' << n.position.y << ' ' << n.health << ' ' << n.maxHealth
                << ' ' << n.level << ' ' << n.experience << ' ' << n.gold << ' ' << n.goods << ' '
                << n.insight << ' ' << n.deadTime << ' ' << n.hostility << ' ' << int(n.action)
                << ' ' << n.decision << ' ' << n.actionTime << ' ' << n.cooldown << ' '
                << int(n.targetKind) << ' ' << n.target << ' ' << n.destination.x << ' '
                << n.destination.y;
            for (double value : n.personality)
            {
                out << ' ' << value;
            }
            for (double value : n.learned)
            {
                out << ' ' << value;
            }
            out << '\n';
        }
        out << loot_.size() << '\n';
        for (const auto& bag : loot_)
        {
            out << bag.id << ' ' << bag.position.x << ' ' << bag.position.y << ' ' << bag.gold
                << ' ' << bag.goods << ' ' << bag.remaining << '\n';
        }
        out << random_ << '\n';
        out.flush();
        out.close();
        if (!out || !MoveFileExW(temp.c_str(),
                                 path_.c_str(),
                                 MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            throw std::runtime_error("Cannot commit society save");
        }
        return true;
    }
    catch (const std::exception& e)
    {
        error_ = e.what();
        return false;
    }
}

void WorldSimulation::Load()
{
    try
    {
        if (!std::filesystem::exists(path_))
        {
            for (int i = 0; i < 24; ++i)
            {
                people_.push_back(Create(i / 12));
            }
            Save();
            return;
        }
        std::ifstream in(path_);
        std::string magic;
        int version;
        std::uint64_t seed;
        if (!(in >> magic >> version >> seed) || magic != "GSE_SOCIETY" ||
            (version != 1 && version != 2) || seed != seed_)
        {
            throw std::runtime_error("Invalid society header; original preserved");
        }
        if (!(in >> nextId_ >> eventId_ >> time_ >> playerGold_ >> playerGoods_ >> crime_ >>
              wildStock_[0] >> wildStock_[1]) ||
            !Finite(time_, 0, 1e12) || playerGold_ < 0 || playerGoods_ < 0 || crime_ < 0 ||
            wildStock_[0] < 0 || wildStock_[0] > 12 || wildStock_[1] < 0 || wildStock_[1] > 12)
        {
            throw std::runtime_error("Invalid society state");
        }
        size_t count;
        if (!(in >> count) || count != 24)
        {
            throw std::runtime_error("Invalid society population");
        }
        std::set<std::uint64_t> ids;
        for (size_t i = 0; i < count; ++i)
        {
            Citizen n;
            int job, goal, action, kind;
            if (!(in >> n.id >> n.home >> job >> goal >> n.position.x >> n.position.y >> n.health >>
                  n.maxHealth >> n.level >> n.experience >> n.gold >> n.goods >> n.insight >>
                  n.deadTime >> n.hostility >> action >> n.decision >> n.actionTime >> n.cooldown >>
                  kind >> n.target >> n.destination.x >> n.destination.y) ||
                !n.id || n.id >= nextId_ || !ids.insert(n.id).second || n.home < 0 || n.home > 1 ||
                job < 0 || job > (version == 1 ? 3 : 6) || goal < 0 || goal > 2 || action < 0 ||
                action >= (version == 1 ? 7 : int(NpcAction::Count)) || kind < 0 || kind > 3 ||
                !Finite(n.position.x, -1e12, 1e12) || !Finite(n.position.y, -1e12, 1e12) ||
                !Finite(n.destination.x, -1e12, 1e12) || !Finite(n.destination.y, -1e12, 1e12) ||
                !Finite(n.maxHealth, 1, 10000) || !Finite(n.health, 0, n.maxHealth) ||
                n.level < 1 || n.level > 20 || n.experience < 0 || n.gold < 0 || n.goods < 0 ||
                n.insight < 0 || !Finite(n.deadTime, -1, 60) || !Finite(n.hostility, 0, 60) ||
                !Finite(n.decision, -10, 60) || !Finite(n.actionTime, 0, 1e12) ||
                !Finite(n.cooldown, -1e12, 60))
            {
                throw std::runtime_error("Invalid citizen record");
            }
            n.goal = NpcGoal(goal);
            n.action = NpcAction(action);
            n.targetKind = CombatKind(kind);
            for (auto& value : n.personality)
            {
                if (!(in >> value) || !Finite(value, 0, 1))
                {
                    throw std::runtime_error("Invalid personality");
                }
            }
            const size_t learnedCount = version == 1 ? 7 : n.learned.size();
            for (size_t index = 0; index < learnedCount; ++index)
            {
                auto& value = n.learned[index];
                if (!(in >> value) || !Finite(value, -100, 100))
                {
                    throw std::runtime_error("Invalid learned weights");
                }
            }
            // Perception and routes must be reevaluated against the loaded world.
            n.dirty = true;
            people_.push_back(n);
        }
        if (!(in >> count) || count > 1000)
        {
            throw std::runtime_error("Invalid loot count");
        }
        for (size_t i = 0; i < count; ++i)
        {
            CitizenLoot bag;
            if (!(in >> bag.id >> bag.position.x >> bag.position.y >> bag.gold >> bag.goods >>
                  bag.remaining) ||
                !bag.id || bag.id >= nextId_ || !ids.insert(bag.id).second ||
                !Finite(bag.position.x, -1e12, 1e12) || !Finite(bag.position.y, -1e12, 1e12) ||
                !Finite(bag.remaining, -1, 120) || bag.gold < 0 || bag.goods < 0)
            {
                throw std::runtime_error("Invalid loot record");
            }
            loot_.push_back(bag);
        }
        if (!(in >> random_))
        {
            throw std::runtime_error("Invalid society random state");
        }
        if (version == 1)
        {
            // Keep a recoverable copy before the next automatic v2 save.
            auto backup = path_;
            backup += L".before_v2";
            if (!std::filesystem::exists(backup))
            {
                std::filesystem::copy_file(path_, backup);
            }
        }
    }
    catch (const std::exception& e)
    {
        people_.clear();
        loot_.clear();
        error_ = e.what();
    }
}
