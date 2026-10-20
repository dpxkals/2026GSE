#pragma once
#include "GameplayTypes.h"
#include <array>
#include <cmath>

struct VillageSite
{
    WorldPoint center;
    const wchar_t* name;
};

constexpr int VillageCount = 5;
constexpr int CitizensPerVillage = 12;
constexpr int VillagePopulation = VillageCount * CitizensPerVillage;
constexpr int VillageWallRadius = 12;

inline const std::array<VillageSite, VillageCount>& Villages()
{
    static const std::array<VillageSite, VillageCount> sites{{{{0.5, 0.5}, L"잿빛 성채"},
                                                              {{112.5, 0.5}, L"등불 교역성"},
                                                              {{-111.5, 0.5}, L"서리감시 요새"},
                                                              {{0.5, 112.5}, L"새벽 수도원"},
                                                              {{0.5, -111.5}, L"붉은재 변경성"}}};
    return sites;
}

inline int VillageAt(WorldPoint point)
{
    for (int i = 0; i < int(Villages().size()); ++i)
    {
        auto center = Villages()[i].center;
        if (std::abs(point.x - center.x) < VillageWallRadius - 0.5 &&
            std::abs(point.y - center.y) < VillageWallRadius - 0.5)
        {
            return i;
        }
    }
    return -1;
}

inline bool VillageRoad(double x, double y)
{
    // Gates on the cardinal axes connect to the central crossroads, independent of chunks.
    return (std::abs(y) <= 1.5 && std::abs(x) <= 126) || (std::abs(x) <= 1.5 && std::abs(y) <= 126);
}
