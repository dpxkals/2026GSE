#include "stdafx.h"
#include "WorldSimulation.h"
#include <algorithm>
#include <cmath>
#include <iostream>

int WorldSimulation::FreeHome(int town) const
{
    for (int home = 0; home < VillageHomeCount; ++home)
    {
        bool used = false;
        for (const auto& npc : people_)
        {
            if ((npc.home == town && npc.house == home) ||
                (npc.travelVillage == town && npc.reservedHouse == home))
            {
                used = true;
            }
        }
        if (!used)
        {
            return home;
        }
    }
    return -1;
}

WorldPoint WorldSimulation::HomePosition(const Citizen& npc) const
{
    return VillageHome(npc.home, npc.house, seed_);
}

WorldPoint WorldSimulation::HomeDoor(const Citizen& npc) const
{
    return VillageHomeDoor(npc.home, npc.house, seed_);
}

std::wstring WorldSimulation::HomeAddress(const Citizen& npc) const
{
    return std::wstring(Villages()[npc.home].name) + L" · " + std::to_wstring(npc.house + 1) +
           L"번 집";
}

int WorldSimulation::OccupiedHomes(int town) const
{
    return int(std::count_if(people_.begin(),
                             people_.end(),
                             [town](const Citizen& npc)
                             {
                                 return npc.home == town;
                             }));
}

int WorldSimulation::KnownReports(int town) const
{
    int count = 0;
    for (int i = 0; i < VillageCount; ++i)
    {
        if (i != town && bulletins_[town][i].revision &&
            time_ - bulletins_[town][i].observed <= 600)
        {
            ++count;
        }
    }
    return count;
}

void WorldSimulation::ObserveVillage(int town)
{
    int living = 0, dead = 0, hostile = 0;
    double hp = 0, gold = 0, insight = 0;
    for (const auto& npc : people_)
    {
        if (npc.home != town)
        {
            continue;
        }
        if (npc.health <= 0)
        {
            ++dead;
            continue;
        }
        ++living;
        hp += npc.health / npc.maxHealth;
        gold += npc.gold;
        insight += npc.insight;
        hostile += npc.hostility > 0 ? 1 : 0;
    }
    double base = Villages()[town].kind == VillageKind::Castle  ? .95
                  : Villages()[town].kind == VillageKind::Abbey ? .85
                                                                : .72;
    VillageReport report;
    report.revision = ++reportRevision_;
    report.observed = time_;
    report.safety =
        std::clamp(base - (dead + hostile * 1.5) / (double((std::max)(1, living + dead))) +
                       (living ? hp / living - .8 : -.4),
                   0.0,
                   1.0);
    report.wealth = std::clamp((living ? gold / living : 0) / 150.0 +
                                   (Villages()[town].kind == VillageKind::Market ? .3 : .05),
                               0.0,
                               1.0);
    report.wisdom = std::clamp((living ? insight / living : 0) / 80.0 +
                                   (Villages()[town].kind == VillageKind::Abbey ? .35 : .05),
                               0.0,
                               1.0);
    bulletins_[town][town] = report;
}

void WorldSimulation::ExchangeNews(Citizen& npc, int town)
{
    for (int i = 0; i < VillageCount; ++i)
    {
        auto& posted = bulletins_[town][i];
        auto& heard = npc.reports[i];
        if (heard.revision > posted.revision)
        {
            posted = heard;
            if (i != town)
            {
                Record("traveler_news",
                       npc.id,
                       0,
                       i,
                       Name(npc) + L"가 " + Villages()[i].name + L"의 소식을 전했다.");
            }
        }
        if (posted.revision > heard.revision)
        {
            heard = posted;
        }
    }
}

void WorldSimulation::ShareReports(Citizen& a, Citizen& b)
{
    for (int i = 0; i < VillageCount; ++i)
    {
        if (a.reports[i].revision > b.reports[i].revision)
        {
            b.reports[i] = a.reports[i];
        }
        else if (b.reports[i].revision > a.reports[i].revision)
        {
            a.reports[i] = b.reports[i];
        }
    }
}

std::wstring WorldSimulation::HeardNews(const Citizen& npc) const
{
    int best = -1;
    for (int i = 0; i < VillageCount; ++i)
    {
        if (i != npc.home && npc.reports[i].revision && time_ - npc.reports[i].observed < 600 &&
            (best < 0 || npc.reports[i].observed > npc.reports[best].observed))
        {
            best = i;
        }
    }
    if (npc.reservedHouse >= 0 && npc.travelVillage >= 0 &&
        npc.reports[npc.travelVillage].revision &&
        time_ - npc.reports[npc.travelVillage].observed < 600)
    {
        best = npc.travelVillage;
    }
    if (best < 0)
    {
        return L"아직 다른 마을 소식을 듣지 못했어요.";
    }
    const auto& report = npc.reports[best];
    return std::wstring(Villages()[best].name) + L" 소식을 들었어요. " +
           (report.safety > .75 ? L"평온하고 " : L"위험도 있지만 ") +
           (report.wealth > .4   ? L"장사가 활발하대요."
            : report.wisdom > .3 ? L"배울 것이 많대요."
                                 : L"새 주민을 기다린대요.");
}

bool WorldSimulation::StartJourney(Citizen& npc, int town, bool relocate)
{
    if (town < 0 || town >= VillageCount || town == VillageAt(npc.position))
    {
        return false;
    }
    int reserved = relocate ? FreeHome(town) : -1;
    if (relocate && reserved < 0)
    {
        return false;
    }
    int from = VillageAt(npc.position);
    if (from < 0)
    {
        from = npc.home;
        for (int i = 0; i < VillageCount; ++i)
        {
            if (std::hypot(npc.position.x - Villages()[i].center.x,
                           npc.position.y - Villages()[i].center.y) <
                std::hypot(npc.position.x - Villages()[from].center.x,
                           npc.position.y - Villages()[from].center.y))
            {
                from = i;
            }
        }
    }
    npc.travelVillage = town;
    npc.travelStop = from; // Gather at the local square before following a road through a gate.
    npc.reservedHouse = reserved;
    npc.action = NpcAction::Travel;
    npc.destination = Villages()[town].center;
    npc.target = 0;
    npc.targetKind = CombatKind::None;
    npc.decision = 300;
    npc.actionTime = 0;
    npc.route.clear();
    npc.pathTimer = 0;
    npc.dirty = false;
    Record(relocate ? "migration_depart" : "visit_depart",
           npc.id,
           0,
           town,
           Name(npc) + (relocate ? L"가 소식을 듣고 " : L"가 소식을 전하러 ") +
               Villages()[town].name + (relocate ? L"로 이사를 떠났다." : L"로 향한다."));
    return true;
}

WorldPoint WorldSimulation::JourneyWaypoint(const Citizen& npc) const
{
    return Villages().at(npc.travelStop).center;
}

void WorldSimulation::ArriveJourney(Citizen& npc)
{
    int arrived = npc.travelStop;
    ExchangeNews(npc, arrived);
    if (arrived != npc.travelVillage)
    {
        npc.travelStop = VillageNextHop(arrived, npc.travelVillage);
        npc.route.clear();
        npc.pathTimer = 0;
        return;
    }
    if (npc.reservedHouse >= 0)
    {
        int oldHome = npc.home;
        npc.home = arrived;
        npc.house = npc.reservedHouse;
        Record("migration_arrive",
               npc.id,
               oldHome,
               arrived,
               Name(npc) + L"가 " + HomeAddress(npc) + L"에 정착했다.");
    }
    else
    {
        Record("visit_arrive", npc.id, 0, arrived, Name(npc) + L"가 편지와 소문을 나눴다.");
    }
    npc.travelVillage = npc.travelStop = npc.reservedHouse = -1;
    npc.migrationCooldown = 180 + double(random_() % 120);
    ++npc.insight;
    Learn(npc, NpcAction::Travel, 3);
    npc.action = NpcAction::Socialize;
    npc.destination = Villages()[arrived].center;
    npc.decision = 12;
    npc.actionTime = 0;
    npc.route.clear();
    npc.dirty = false;
}

void WorldSimulation::ConsiderMigration(Citizen& npc)
{
    if (npc.migrationCooldown > 0 || npc.action == NpcAction::Travel || npc.hostility > 0 ||
        npc.health < npc.maxHealth * .6 || VillageAt(npc.position) != npc.home)
    {
        return;
    }
    npc.migrationCooldown = 90 + double(random_() % 90);
    auto score = [&npc](const VillageReport& report)
    {
        const auto& p = npc.personality;
        return report.safety * (1.4 - p[3] + p[4]) +
               report.wealth * (.4 + p[1] + npc.learned[size_t(NpcAction::Trade)] * .025) +
               report.wisdom * (.3 + p[5] + npc.learned[size_t(NpcAction::Meditate)] * .025);
    };
    if (!npc.reports[npc.home].revision)
    {
        return;
    }
    double best = score(npc.reports[npc.home]) + .08;
    int target = -1;
    for (int i = 0; i < VillageCount; ++i)
    {
        const auto& heard = npc.reports[i];
        if (i == npc.home || !heard.revision || time_ - heard.observed > 600)
        {
            continue;
        }
        double value = score(heard);
        if (value > best)
        {
            best = value;
            target = i;
        }
    }
    if (target >= 0 && double(random_() % 100) / 100.0 < .25 + npc.personality[5] * .45)
    {
        StartJourney(npc, target, true);
    }
}

void WorldSimulation::UpdateSettlements(double dt)
{
    reportTimer_ -= dt;
    letterTimer_ -= dt;
    if (reportTimer_ <= 0)
    {
        reportTimer_ = 30;
        for (int town = 0; town < VillageCount; ++town)
        {
            ObserveVillage(town);
        }
    }
    for (auto it = letters_.begin(); it != letters_.end();)
    {
        it->remaining -= dt;
        if (it->remaining > 0)
        {
            ++it;
            continue;
        }
        auto& posted = bulletins_[it->to][it->origin];
        if (it->report.revision > posted.revision)
        {
            posted = it->report;
            Record("letter_arrive",
                   0,
                   it->to,
                   it->origin,
                   std::wstring(Villages()[it->to].name) + L"에 " + Villages()[it->origin].name +
                       L"의 편지가 도착했다.");
        }
        it = letters_.erase(it);
    }
    if (letterTimer_ <= 0)
    {
        letterTimer_ = 45;
        for (int from = 0; from < VillageCount; ++from)
        {
            for (int to = 0; to < VillageCount; ++to)
            {
                if (from == to || VillageNextHop(from, to) != to)
                {
                    continue;
                }
                for (int origin = 0; origin < VillageCount; ++origin)
                {
                    const auto& report = bulletins_[from][origin];
                    if (!report.revision || time_ - report.observed > 600)
                    {
                        continue;
                    }
                    // Postal service only knows the sender's board. Receivers discard old copies.
                    bool pending = std::any_of(letters_.begin(),
                                               letters_.end(),
                                               [&](const VillageLetter& letter)
                                               {
                                                   return letter.from == from && letter.to == to &&
                                                          letter.origin == origin;
                                               });
                    if (pending)
                    {
                        continue;
                    }
                    auto a = Villages()[from].center, b = Villages()[to].center;
                    letters_.push_back(
                        {from, to, origin, std::hypot(a.x - b.x, a.y - b.y) / 2.6, report});
                }
            }
        }
    }
}
