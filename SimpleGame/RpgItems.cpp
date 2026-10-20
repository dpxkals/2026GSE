#include "stdafx.h"
#include "RpgItems.h"
#include <sstream>

const std::array<ItemDefinition, RpgItemCount>& ItemCatalog()
{
    static const std::array<ItemDefinition, RpgItemCount> items{
        {{L"여행자의 지팡이", ItemSlot::Weapon, 1, 50, 6, 0, 0, 0, 0},
         {L"사냥꾼의 쇠뇌", ItemSlot::Weapon, 4, 140, 12, 0, 0, .02, 0},
         {L"서리 룬 지팡이", ItemSlot::Weapon, 7, 280, 20, 0, 0, .03, 0},
         {L"새벽의 성궁", ItemSlot::Weapon, 10, 450, 30, 0, 0, .04, 0},
         {L"파수꾼의 유산", ItemSlot::Weapon, 15, 800, 45, 0, 0, .06, 0},
         {L"누빈 여행복", ItemSlot::Armor, 1, 45, 0, 10, 3, 0, 0},
         {L"사냥꾼의 가죽갑옷", ItemSlot::Armor, 4, 120, 0, 20, 6, 0, 0},
         {L"성채 사슬갑옷", ItemSlot::Armor, 7, 250, 0, 35, 10, 0, 0},
         {L"새벽 수호 갑옷", ItemSlot::Armor, 10, 420, 0, 55, 15, 0, 0},
         {L"잿빛 기사 갑옷", ItemSlot::Armor, 15, 750, 0, 90, 22, 0, 0},
         {L"작은 구리 반지", ItemSlot::Accessory, 1, 40, 0, 5, 0, .02, .1},
         {L"호박빛 목걸이", ItemSlot::Accessory, 4, 110, 0, 10, 0, .04, .2},
         {L"순례자의 인장", ItemSlot::Accessory, 7, 230, 0, 15, 0, .06, .3},
         {L"별빛 영혼 반지", ItemSlot::Accessory, 10, 400, 0, 25, 0, .08, .4},
         {L"여명의 심장", ItemSlot::Accessory, 15, 700, 0, 40, 0, .10, .5},
         {L"치유 물약", ItemSlot::Consumable, 1, 18, 0, 0, 0, 0, 0},
         {L"영혼 자석", ItemSlot::Consumable, 1, 35, 0, 0, 0, 0, 0},
         {L"빛바랜 유물", ItemSlot::Treasure, 1, 90, 0, 0, 0, 0, 0}}};
    return items;
}

std::wstring ItemDescription(int id)
{
    const auto& item = ItemCatalog().at(id);
    std::wostringstream out;
    out << L"요구 레벨 " << item.requiredLevel;
    if (item.damage)
    {
        out << L" · 공격 +" << item.damage;
    }
    if (item.health)
    {
        out << L" · 체력 +" << item.health;
    }
    if (item.defense)
    {
        out << L" · 방어 +" << item.defense;
    }
    if (item.haste)
    {
        out << L" · 발사 간격 -" << int(item.haste * 100) << L"%";
    }
    if (item.speed)
    {
        out << L" · 이동 +" << item.speed;
    }
    if (id == 15)
    {
        out << L" · 체력 80 회복";
    }
    if (id == 16)
    {
        out << L" · 8초 동안 자동 습득 범위 확대";
    }
    if (id == 17)
    {
        out << L" · 교역용 보물";
    }
    return out.str();
}
