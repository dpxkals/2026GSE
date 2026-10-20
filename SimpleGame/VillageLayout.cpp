#include "stdafx.h"
#include "VillageLayout.h"
#include <algorithm>
#include <cmath>
#include <queue>
#include <stdexcept>

namespace
{
    constexpr int Roads[][2] = {{0, 1}, {0, 2}, {0, 3}, {0, 4}, {1, 5}, {3, 5}, {2, 6}, {4, 6}};

    std::uint64_t MixLayout(std::uint64_t value)
    {
        value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
        return value ^ (value >> 31);
    }
}

const std::array<VillageSite, VillageCount>& Villages()
{
    static const std::array<VillageSite, VillageCount> sites{
        {VillageSite{{0.5, 0.5}, L"잿빛 왕성", VillageKind::Castle, 28, 28},
         VillageSite{{90.5, 0.5}, L"등불 장터", VillageKind::Market, 32, 28},
         VillageSite{{-89.5, 0.5}, L"보리들 마을", VillageKind::Farming, 32, 28},
         VillageSite{{0.5, 90.5}, L"새벽 수도원", VillageKind::Abbey, 28, 32},
         VillageSite{{0.5, -89.5}, L"솔바람 촌락", VillageKind::Woodland, 30, 30},
         VillageSite{{90.5, 90.5}, L"검은돌 광산촌", VillageKind::Mining, 30, 28},
         VillageSite{{-89.5, -89.5}, L"물안개 나루", VillageKind::Riverside, 34, 28}}};
    return sites;
}

const wchar_t* VillageKindName(VillageKind kind)
{
    static const wchar_t* names[] = {
        L"왕성", L"교역 마을", L"농촌", L"수도원", L"숲 마을", L"광산촌", L"강변 마을"};
    return names[int(kind)];
}

bool VillageContains(int town, double x, double y, double margin)
{
    const auto& site = Villages().at(town);
    double dx = std::abs(x - site.center.x), dy = std::abs(y - site.center.y);
    double w = site.halfWidth - margin, h = site.halfHeight - margin;
    if (w <= 0 || h <= 0 || dx > w || dy > h)
    {
        return false;
    }
    if (site.kind == VillageKind::Market || site.kind == VillageKind::Woodland ||
        site.kind == VillageKind::Riverside)
    {
        return dx * dx / (w * w) + dy * dy / (h * h) <= 1;
    }
    if (site.kind == VillageKind::Abbey || site.kind == VillageKind::Mining)
    {
        return dx / w + dy / h < 1.65;
    }
    return true;
}

int VillageAt(WorldPoint point)
{
    for (int i = 0; i < VillageCount; ++i)
    {
        if (VillageContains(i, point.x, point.y, 1.5))
        {
            return i;
        }
    }
    return -1;
}

bool VillageRoad(double x, double y)
{
    for (const auto& edge : Roads)
    {
        auto a = Villages()[edge[0]].center, b = Villages()[edge[1]].center;
        double dx = b.x - a.x, dy = b.y - a.y;
        double t = std::clamp(((x - a.x) * dx + (y - a.y) * dy) / (dx * dx + dy * dy), 0.0, 1.0);
        if (std::hypot(x - a.x - t * dx, y - a.y - t * dy) <= 1.7)
        {
            return true;
        }
    }
    return false;
}

int VillageNextHop(int from, int to)
{
    if (from < 0 || from >= VillageCount || to < 0 || to >= VillageCount || from == to)
    {
        return to;
    }
    std::array<int, VillageCount> previous;
    previous.fill(-1);
    std::queue<int> queue;
    queue.push(from);
    previous[from] = from;
    while (!queue.empty() && previous[to] < 0)
    {
        int current = queue.front();
        queue.pop();
        for (const auto& edge : Roads)
        {
            int next = edge[0] == current ? edge[1] : edge[1] == current ? edge[0] : -1;
            if (next >= 0 && previous[next] < 0)
            {
                previous[next] = current;
                queue.push(next);
            }
        }
    }
    int next = to;
    while (previous[next] != from && previous[next] >= 0)
    {
        next = previous[next];
    }
    return next;
}

double VillageHuntingRadius(int town)
{
    const auto& site = Villages().at(town);
    return (std::max)(site.halfWidth, site.halfHeight) + 8.0;
}

Prop VillageWall(int town, int x, int y)
{
    const auto& site = Villages().at(town);
    bool north = y == -site.halfHeight && std::abs(x) <= site.halfWidth;
    bool south =
        site.kind == VillageKind::Castle && y == site.halfHeight && std::abs(x) <= site.halfWidth;
    bool side = site.kind == VillageKind::Castle && std::abs(x) == site.halfWidth &&
                std::abs(y) <= site.halfHeight;
    bool fortified = site.kind == VillageKind::Castle || site.kind == VillageKind::Abbey ||
                     site.kind == VillageKind::Mining;
    if (!fortified || (!north && !south && !side))
    {
        return Prop::None;
    }
    int along = side ? y : x;
    if (std::abs(along) <= 2)
    {
        return Prop::None;
    }
    return std::abs(along) == 3 || std::abs(x) == site.halfWidth ? Prop::WallTower
           : side                                                ? Prop::WallY
                                                                 : Prop::WallX;
}

const std::vector<VillageBuilding>& VillageBuildings(int town, std::uint64_t seed)
{
    // Bounded shared cache keeps homes and terrain on exactly the same plan.
    static std::uint64_t cachedSeed = 0;
    static bool ready = false;
    static std::array<std::vector<VillageBuilding>, VillageCount> plans;
    if (!ready || cachedSeed != seed)
    {
        for (int village = 0; village < VillageCount; ++village)
        {
            auto& plan = plans[village];
            plan.clear();

            struct Candidate
            {
                int x, y;
                std::uint64_t order;
            };

            std::vector<Candidate> candidates;
            for (int y : {-21, -15, -9, -3, 3, 9, 15, 21})
            {
                for (int x : {-21, -15, -9, -3, 3, 9, 15, 21})
                {
                    const auto& site = Villages()[village];
                    bool fits = true;
                    for (int sy : {-2, 2})
                    {
                        for (int sx : {-2, 2})
                        {
                            if (!VillageContains(
                                    village, site.center.x + x + sx, site.center.y + y + sy, 2))
                            {
                                fits = false;
                            }
                        }
                    }
                    if (fits)
                    {
                        candidates.push_back({x,
                                              y,
                                              MixLayout(seed ^ (std::uint64_t(village + 1) * 977) ^
                                                        std::uint64_t((x + 32) * 97 + y + 32))});
                    }
                }
            }
            std::sort(candidates.begin(),
                      candidates.end(),
                      [](const Candidate& a, const Candidate& b)
                      {
                          return a.order < b.order;
                      });
            constexpr int needed = VillageHomeCount + VillageGuideHomes + 3;
            if (candidates.size() < needed)
            {
                throw std::runtime_error("Village plan cannot fit all homes");
            }
            for (int i = 0; i < needed; ++i)
            {
                const auto& c = candidates[i];
                Prop prop = Prop::Cottage;
                if (i == needed - 3)
                {
                    prop = Villages()[village].kind == VillageKind::Castle ||
                                   Villages()[village].kind == VillageKind::Mining
                               ? Prop::Watchtower
                               : Prop::Chapel;
                }
                if (i == needed - 2)
                {
                    prop = Prop::Well;
                }
                if (i == needed - 1)
                {
                    prop = Prop::Graves;
                }
                plan.push_back({c.x, c.y, prop, i < VillageHomeCount + VillageGuideHomes ? i : -1});
            }
        }
        cachedSeed = seed;
        ready = true;
    }
    return plans.at(town);
}

WorldPoint VillageHome(int town, int home, std::uint64_t seed)
{
    const auto& building = VillageBuildings(town, seed).at(home);
    const auto& center = Villages().at(town).center;
    return {center.x + building.x, center.y + building.y};
}

WorldPoint VillageHomeDoor(int town, int home, std::uint64_t seed)
{
    auto point = VillageHome(town, home, seed);
    point.y += 2.0;
    return point;
}

WorldPoint VillageApproach(int town)
{
    int other = VillageNextHop(town, town == 0 ? 1 : 0);
    auto center = Villages()[town].center, neighbor = Villages()[other].center;
    double distance = std::hypot(neighbor.x - center.x, neighbor.y - center.y);
    double radius = VillageHuntingRadius(town);
    return {center.x + (neighbor.x - center.x) / distance * radius,
            center.y + (neighbor.y - center.y) / distance * radius};
}
