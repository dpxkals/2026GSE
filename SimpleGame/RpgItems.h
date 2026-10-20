#pragma once
#include <array>
#include <string>

enum class ItemSlot
{
    Weapon,
    Armor,
    Accessory,
    Consumable,
    Treasure
};
constexpr int RpgItemCount = 18;
constexpr int RpgQuestCount = 6;

struct ItemDefinition
{
    const wchar_t* name;
    ItemSlot slot;
    int requiredLevel, price, damage, health, defense;
    double haste, speed;
};

const std::array<ItemDefinition, RpgItemCount>& ItemCatalog();
std::wstring ItemDescription(int id);

struct RpgProgress
{
    std::array<int, RpgItemCount> inventory{};
    std::array<int, 3> equipped{{-1, -1, -1}};
    std::array<bool, RpgQuestCount> claimed{};
    int hunts = 0, finds = 0, trades = 0, bosses = 0, visited = 0;
    int bountyBaseline = 0, bountyRound = 0;
    double bossTimer = 180;
    int bossVillage = -1, lastBossVillage = -1;
};
