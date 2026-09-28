#pragma once
#include "GameplayTypes.h"
#include <array>
#include <cmath>

struct VillageSite
{
    WorldPoint center;
    const wchar_t* name;
};

inline const std::array<VillageSite, 2>& Villages()
{
    static const std::array<VillageSite, 2> sites{
        {VillageSite{{0.5, 0.5}, L"잿빛 마을"}, VillageSite{{26.5, 10.5}, L"등불 교역소"}}};
    return sites;
}

inline int VillageAt(WorldPoint point)
{
    for (int i = 0; i < int(Villages().size()); ++i)
    {
        auto center = Villages()[i].center;
        if (std::hypot(point.x - center.x, point.y - center.y) <= 4.5)
        {
            return i;
        }
    }
    return -1;
}
