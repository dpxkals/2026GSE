#include "stdafx.h"
#include "WorldSimulation.h"
#include <Windows.h>
#include <fstream>
#include <iomanip>
#include <cmath>
#include <set>
#include <stdexcept>
#include <iostream>
#include <algorithm>

namespace
{
    void WriteReport(std::ostream& out, const VillageReport& report)
    {
        out << report.revision << ' ' << report.observed << ' ' << report.safety << ' '
            << report.wealth << ' ' << report.wisdom << ' ';
    }

    bool Finite(double value, double low, double high)
    {
        return std::isfinite(value) && value >= low && value <= high;
    }

    void ReadReport(std::istream& in, VillageReport& report, double now, std::uint64_t revision)
    {
        if (!(in >> report.revision >> report.observed >> report.safety >> report.wealth >>
              report.wisdom) ||
            report.revision > revision || !Finite(report.observed, 0, now) ||
            !Finite(report.safety, 0, 1) || !Finite(report.wealth, 0, 1) ||
            !Finite(report.wisdom, 0, 1))
        {
            throw std::runtime_error("Invalid village report");
        }
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
        out << std::setprecision(17) << "GSE_SOCIETY 4 " << seed_ << '\n';
        out << nextId_ << ' ' << eventId_ << ' ' << time_ << ' ' << playerGold_ << ' '
            << playerGoods_ << ' ' << crime_;
        for (int stock : wildStock_)
        {
            out << ' ' << stock;
        }
        out << '\n';
        out << reportRevision_ << ' ' << reportTimer_ << ' ' << letterTimer_ << '\n';
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
            out << ' ' << n.house << ' ' << n.travelVillage << ' ' << n.travelStop << ' '
                << n.reservedHouse << ' ' << n.migrationCooldown << ' ' << n.newsCooldown << '\n';
            for (const auto& report : n.reports)
            {
                WriteReport(out, report);
            }
            out << '\n';
        }
        out << loot_.size() << '\n';
        for (const auto& bag : loot_)
        {
            out << bag.id << ' ' << bag.position.x << ' ' << bag.position.y << ' ' << bag.gold
                << ' ' << bag.goods << ' ' << bag.remaining << '\n';
        }
        for (const auto& board : bulletins_)
        {
            for (const auto& report : board)
            {
                WriteReport(out, report);
            }
            out << '\n';
        }
        out << letters_.size() << '\n';
        for (const auto& letter : letters_)
        {
            out << letter.from << ' ' << letter.to << ' ' << letter.origin << ' '
                << letter.remaining << ' ';
            WriteReport(out, letter.report);
            out << '\n';
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
            for (int i = 0; i < VillagePopulation; ++i)
            {
                people_.push_back(Create(i / CitizensPerVillage));
            }
            Save();
            return;
        }
        std::ifstream in(path_);
        std::string magic;
        int version;
        std::uint64_t seed;
        if (!(in >> magic >> version >> seed) || magic != "GSE_SOCIETY" ||
            (version < 1 || version > 4) || seed != seed_)
        {
            throw std::runtime_error("Invalid society header; original preserved");
        }
        if (!(in >> nextId_ >> eventId_ >> time_ >> playerGold_ >> playerGoods_ >> crime_) ||
            !Finite(time_, 0, 1e12) || playerGold_ < 0 || playerGoods_ < 0 || crime_ < 0)
        {
            throw std::runtime_error("Invalid society state");
        }
        int savedVillages = version < 3 ? 2 : version == 3 ? 5 : VillageCount;
        for (int i = 0; i < savedVillages; ++i)
        {
            if (!(in >> wildStock_[i]) || wildStock_[i] < 0 || wildStock_[i] > 12)
            {
                throw std::runtime_error("Invalid regional resource stock");
            }
        }
        if (version >= 4 && (!(in >> reportRevision_ >> reportTimer_ >> letterTimer_) ||
                             !Finite(reportTimer_, -.1, 30) || !Finite(letterTimer_, -.1, 45)))
        {
            throw std::runtime_error("Invalid settlement timers");
        }
        size_t count;
        if (!(in >> count) || count != (version < 3 ? 24 : version == 3 ? 60 : VillagePopulation))
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
                !n.id || n.id >= nextId_ || !ids.insert(n.id).second || n.home < 0 ||
                n.home >= savedVillages || job < 0 || job > (version == 1 ? 3 : 6) || goal < 0 ||
                goal > 2 || action < 0 || action >= (version == 1 ? 7 : int(NpcAction::Count)) ||
                kind < 0 || kind > 3 || !Finite(n.position.x, -1e12, 1e12) ||
                !Finite(n.position.y, -1e12, 1e12) || !Finite(n.destination.x, -1e12, 1e12) ||
                !Finite(n.destination.y, -1e12, 1e12) || !Finite(n.maxHealth, 1, 10000) ||
                !Finite(n.health, 0, n.maxHealth) || n.level < 1 || n.level > MaxCharacterLevel ||
                n.experience < 0 || n.gold < 0 || n.goods < 0 || n.insight < 0 ||
                !Finite(n.deadTime, -1, 60) || !Finite(n.hostility, 0, 60) ||
                !Finite(n.decision, -10, 300) || !Finite(n.actionTime, 0, 1e12) ||
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
            if (version >= 4)
            {
                if (!(in >> n.house >> n.travelVillage >> n.travelStop >> n.reservedHouse >>
                      n.migrationCooldown >> n.newsCooldown) ||
                    n.house < 0 || n.house >= VillageHomeCount || n.travelVillage < -1 ||
                    n.travelVillage >= VillageCount || n.travelStop < -1 ||
                    n.travelStop >= VillageCount || n.reservedHouse < -1 ||
                    n.reservedHouse >= VillageHomeCount || !Finite(n.migrationCooldown, 0, 300) ||
                    !Finite(n.newsCooldown, -.1, 20) ||
                    ((n.travelVillage >= 0) != (n.travelStop >= 0)) ||
                    (n.action == NpcAction::Travel && n.travelVillage < 0) ||
                    (n.travelVillage >= 0 && n.action != NpcAction::Travel) ||
                    (n.reservedHouse >= 0 && n.travelVillage < 0))
                {
                    throw std::runtime_error("Invalid home or journey");
                }
                for (auto& report : n.reports)
                {
                    ReadReport(in, report, time_, reportRevision_);
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
        if (version >= 4)
        {
            for (auto& board : bulletins_)
            {
                for (auto& report : board)
                {
                    ReadReport(in, report, time_, reportRevision_);
                }
            }
            if (!(in >> count) || count > VillageCount * VillageCount * VillageCount)
            {
                throw std::runtime_error("Invalid letter count");
            }
            for (size_t i = 0; i < count; ++i)
            {
                VillageLetter letter;
                if (!(in >> letter.from >> letter.to >> letter.origin >> letter.remaining) ||
                    letter.from < 0 || letter.from >= VillageCount || letter.to < 0 ||
                    letter.to >= VillageCount || letter.origin < 0 ||
                    letter.origin >= VillageCount || letter.from == letter.to ||
                    VillageNextHop(letter.from, letter.to) != letter.to ||
                    !Finite(letter.remaining, -.1, 60))
                {
                    throw std::runtime_error("Invalid letter route");
                }
                ReadReport(in, letter.report, time_, reportRevision_);
                letters_.push_back(letter);
            }
            std::set<std::pair<int, int>> homes;
            for (const auto& npc : people_)
            {
                if (!homes.emplace(npc.home, npc.house).second)
                {
                    throw std::runtime_error("Duplicate home ownership");
                }
                if (npc.reservedHouse >= 0 &&
                    !homes.emplace(npc.travelVillage, npc.reservedHouse).second)
                {
                    throw std::runtime_error("Duplicate migration reservation");
                }
            }
        }
        if (!(in >> random_))
        {
            throw std::runtime_error("Invalid society random state");
        }
        in.close();
        if (version < 4)
        {
            // Preserve the old layout/population before the first settlement v4 save.
            auto backup = path_;
            backup += L".before_settlements_v4";
            if (!std::filesystem::exists(backup))
            {
                std::filesystem::copy_file(path_, backup);
            }
            std::array<int, VillageCount> population{};
            for (auto& npc : people_)
            {
                if (population[npc.home] >= CitizensPerVillage)
                {
                    npc.home = int(std::min_element(population.begin(), population.end()) -
                                   population.begin());
                }
                npc.house = population[npc.home]++;
                double ratio = npc.health / npc.maxHealth;
                npc.maxHealth = 150 + (npc.level - 1) * 18;
                npc.health = npc.maxHealth * ratio;
                npc.position = HomeDoor(npc);
                npc.destination = npc.position;
                npc.target = 0;
                npc.targetKind = CombatKind::None;
                npc.action = NpcAction::Rest;
            }
            for (int home = 0; home < VillageCount; ++home)
            {
                for (int i = population[home]; i < CitizensPerVillage; ++i)
                {
                    people_.push_back(Create(home));
                }
            }
            std::cout << "[SettlementMigration] society v" << version
                      << " -> v4; population=" << people_.size()
                      << "; homes allocated; original backed up\n";
        }
    }
    catch (const std::exception& e)
    {
        people_.clear();
        loot_.clear();
        error_ = e.what();
    }
}
