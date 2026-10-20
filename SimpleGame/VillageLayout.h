#pragma once
#include "GameplayTypes.h"
#include <array>
#include "World.h"
#include <vector>

enum class VillageKind
{
    Castle,
    Market,
    Farming,
    Abbey,
    Woodland,
    Mining,
    Riverside
};

struct VillageSite
{
    WorldPoint center;
    const wchar_t* name;
    VillageKind kind;
    int halfWidth, halfHeight;
};

constexpr int VillageCount = 7;
constexpr int CitizensPerVillage = 30;
constexpr int VillagePopulation = VillageCount * CitizensPerVillage;
constexpr int VillageHomeCount = 42;
constexpr int VillageGuideHomes = 2;
constexpr double VillageSpacing = 90;

struct VillageBuilding
{
    int x, y;
    Prop prop;
    int home = -1;
};

const std::array<VillageSite, VillageCount>& Villages();
const wchar_t* VillageKindName(VillageKind kind);
bool VillageContains(int town, double x, double y, double margin = 0);
int VillageAt(WorldPoint point);
bool VillageRoad(double x, double y);
int VillageNextHop(int from, int to);
double VillageHuntingRadius(int town);
Prop VillageWall(int town, int x, int y);
const std::vector<VillageBuilding>& VillageBuildings(int town, std::uint64_t seed);
WorldPoint VillageHome(int town, int home, std::uint64_t seed);
WorldPoint VillageHomeDoor(int town, int home, std::uint64_t seed);
WorldPoint VillageApproach(int town);
