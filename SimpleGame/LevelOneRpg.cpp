#include "stdafx.h"
#include "LevelOne.h"
#include "WorldSimulation.h"
#include "VillageLayout.h"
#include <algorithm>
#include <cmath>
#include <iostream>

void LevelOne::RebuildEquipment()
{
    player_.gearDamage = player_.gearHealth = player_.gearDefense = 0;
    player_.gearHaste = player_.gearSpeed = 0;
    for (int id : rpg_.equipped)
    {
        if (id < 0)
        {
            continue;
        }
        const auto& item = ItemCatalog().at(id);
        player_.gearDamage += item.damage;
        player_.gearHealth += item.health;
        player_.gearDefense += item.defense;
        player_.gearHaste += item.haste;
        player_.gearSpeed += item.speed;
    }
    player_.health = (std::min)(player_.health, player_.MaxHealth());
}

bool LevelOne::UseItem(int id)
{
    if (!error_.empty() || id < 0 || id >= RpgItemCount || rpg_.inventory[id] <= 0)
    {
        return false;
    }
    const auto& item = ItemCatalog()[id];
    if (player_.level < item.requiredLevel)
    {
        Announce(L"이 장비를 사용하기에는 레벨이 부족합니다.");
        return false;
    }
    if (int(item.slot) < 3)
    {
        int& equipped = rpg_.equipped[int(item.slot)];
        equipped = equipped == id ? -1 : id;
        RebuildEquipment();
        Announce(equipped == id ? std::wstring(item.name) + L" 착용" : L"장비를 해제했습니다.");
    }
    else if (id == 15 || id == 16)
    {
        if (id == 15 && player_.health >= player_.MaxHealth())
        {
            Announce(L"체력이 이미 가득합니다.");
            return false;
        }
        --rpg_.inventory[id];
        if (id == 15)
        {
            player_.health = (std::min)(player_.MaxHealth(), player_.health + 80);
            ++life_.heals;
        }
        else
        {
            magnetTime_ = 8;
        }
        Announce(std::wstring(item.name) + L" 사용");
    }
    else
    {
        Announce(L"이 유물은 주민에게 판매할 수 있습니다.");
        return false;
    }
    urgentSave_ = true;
    return Save();
}

bool LevelOne::BuyItem(int id, std::uint64_t merchant)
{
    if (!error_.empty() || !simulation_ || id < 0 || id >= 17 || rpg_.inventory[id] >= 999)
    {
        return false;
    }
    if (!simulation_->ExchangeItemGold(merchant, player_.position, id, true))
    {
        Announce(L"구매 실패: 금화 또는 거래 가능한 주민을 확인하세요.");
        return false;
    }
    ++rpg_.inventory[id];
    rpg_.trades = (std::min)(1000000000, rpg_.trades + 1);
    Announce(std::wstring(ItemCatalog()[id].name) + L" 구매");
    urgentSave_ = true;
    bool saved = Save();
    return simulation_->Save() && saved;
}

bool LevelOne::SellItem(int id, std::uint64_t merchant)
{
    if (!error_.empty() || !simulation_ || id < 0 || id >= RpgItemCount || rpg_.inventory[id] <= 0)
    {
        return false;
    }
    if (std::find(rpg_.equipped.begin(), rpg_.equipped.end(), id) != rpg_.equipped.end() &&
        rpg_.inventory[id] == 1)
    {
        Announce(L"착용 중인 마지막 장비는 해제한 뒤 판매하세요.");
        return false;
    }
    if (!simulation_->ExchangeItemGold(merchant, player_.position, id, false))
    {
        Announce(L"판매 실패: 주민의 금화 또는 거래 거리를 확인하세요.");
        return false;
    }
    --rpg_.inventory[id];
    rpg_.trades = (std::min)(1000000000, rpg_.trades + 1);
    Announce(std::wstring(ItemCatalog()[id].name) + L" 판매");
    urgentSave_ = true;
    bool saved = Save();
    return simulation_->Save() && saved;
}

int LevelOne::QuestTarget(int index) const
{
    const int goals[] = {5, 3, 3, 2, 2, 1, 20};
    return index >= 0 && index <= RpgQuestCount ? goals[index] : 0;
}

int LevelOne::QuestProgress(int index) const
{
    int equipment = 0, villages = 0;
    for (int id : rpg_.equipped)
    {
        equipment += id >= 0 ? 1 : 0;
    }
    for (int i = 0; i < VillageCount; ++i)
    {
        villages += (rpg_.visited >> i) & 1;
    }
    const int values[] = {rpg_.hunts,
                          rpg_.finds,
                          equipment,
                          villages,
                          rpg_.trades,
                          rpg_.bosses,
                          rpg_.hunts - rpg_.bountyBaseline};
    return index >= 0 && index <= RpgQuestCount ? (std::min)(QuestTarget(index), values[index]) : 0;
}

std::wstring LevelOne::QuestTitle(int index) const
{
    static const wchar_t* titles[] = {L"성문 밖의 위협 · 몬스터 5마리 처치",
                                      L"잿빛 속의 보물 · 드롭 아이템 3개 습득",
                                      L"여행자의 준비 · 무기/방어구/장신구 착용",
                                      L"이어지는 등불 · 서로 다른 마을 2곳 방문",
                                      L"시장의 약속 · 아이템 구매/판매 2회",
                                      L"성채의 수호자 · 마을 보스 1회 처치",
                                      L"반복 의뢰 · 몬스터 20마리 처치"};
    return index >= 0 && index <= RpgQuestCount ? titles[index] : L"";
}

std::wstring LevelOne::QuestReward(int index) const
{
    const int gold[] = {80, 80, 100, 150, 100, 350, 100}, xp[] = {60, 80, 120, 150, 180, 250, 120};
    if (index < 0 || index > RpgQuestCount)
    {
        return L"";
    }
    return L"금화 " + std::to_wstring(gold[index]) + L" / 경험치 " + std::to_wstring(xp[index]) +
           (index == 0   ? L" / 여행자의 지팡이"
            : index == 1 ? L" / 누빈 여행복"
            : index == 2 ? L" / 치유 물약"
            : index == 5 ? L" / 새벽의 성궁"
                         : L"");
}

bool LevelOne::ClaimQuest(int index)
{
    if (!error_.empty() || !simulation_ || !simulation_->Error().empty() || index < 0 ||
        index > RpgQuestCount || (index < RpgQuestCount && rpg_.claimed[index]))
    {
        return false;
    }
    if (QuestProgress(index) < QuestTarget(index))
    {
        Announce(L"아직 의뢰 목표를 달성하지 못했습니다.");
        return false;
    }
    const int gold[] = {80, 80, 100, 150, 100, 350, 100}, xp[] = {60, 80, 120, 150, 180, 250, 120};
    int item = index == 0 ? 0 : index == 1 ? 5 : index == 2 ? 15 : index == 5 ? 3 : -1;
    if (item >= 0 && rpg_.inventory[item] >= 999)
    {
        Announce(L"보상 아이템을 받을 공간이 부족합니다.");
        return false;
    }
    if (index < RpgQuestCount)
    {
        rpg_.claimed[index] = true;
    }
    else
    {
        rpg_.bountyBaseline += 20;
        rpg_.bountyRound = (std::min)(1000000000, rpg_.bountyRound + 1);
    }
    if (item >= 0)
    {
        ++rpg_.inventory[item];
    }
    simulation_->RewardGold(gold[index]);
    GainExperience(xp[index]);
    Announce(L"의뢰 완료! " + QuestReward(index));
    std::cout << "[Quest] claimed=" << index << " gold=" << gold[index] << " xp=" << xp[index]
              << '\n';
    urgentSave_ = true;
    bool saved = Save();
    return simulation_->Save() && saved;
}

void LevelOne::FinishBoss()
{
    phase_ = LevelPhase::Farming;
    rpg_.bossVillage = -1;
    rpg_.bossTimer = BossInterval;
    urgentSave_ = true;
}

void LevelOne::UpdateRpg(double dt, const World& world)
{
    int inside = VillageAt(player_.position);
    if (inside >= 0)
    {
        rpg_.visited |= 1 << inside;
    }
    rpg_.bossTimer = (std::max)(0.0, rpg_.bossTimer - dt);
    if (phase_ == LevelPhase::BossFight)
    {
        for (const auto& enemy : enemies_)
        {
            if (enemy.kind == EnemyKind::Boss && enemy.health <= 0)
            {
                return; // ResolveCombat must credit a kill before event expiry.
            }
        }
        if (rpg_.bossTimer > 0)
        {
            return;
        }
        for (const auto& enemy : enemies_)
        {
            if (enemy.kind == EnemyKind::Boss &&
                std::hypot(enemy.position.x - player_.position.x,
                           enemy.position.y - player_.position.y) < 18)
            {
                rpg_.bossTimer = 30; // Never disappear while the player is fighting nearby.
                return;
            }
        }
        enemies_.erase(std::remove_if(enemies_.begin(),
                                      enemies_.end(),
                                      [](const Enemy& e)
                                      {
                                          return e.kind == EnemyKind::Boss;
                                      }),
                       enemies_.end());
        phase_ = LevelPhase::Farming;
        rpg_.bossVillage = -1;
        std::cout << "[BossEvent] expired; moving to another village\n";
    }
    if (rpg_.bossTimer > 0)
    {
        return;
    }
    if (rpg_.bossVillage < 0)
    {
        int choice = int(random_() % (VillageCount - (rpg_.lastBossVillage >= 0 ? 1 : 0)));
        if (rpg_.lastBossVillage >= 0 && choice >= rpg_.lastBossVillage)
        {
            ++choice;
        }
        rpg_.bossVillage = choice;
    }
    auto center = Villages()[rpg_.bossVillage].center;
    // Approach along a guaranteed inter-village road, outside the safe walls.
    int direction = rpg_.bossVillage == 1   ? 2
                    : rpg_.bossVillage == 2 ? 0
                    : rpg_.bossVillage == 3 ? 3
                    : rpg_.bossVillage == 4 ? 1
                                            : int(random_() % 4);
    const int dx[] = {1, 0, -1, 0}, dy[] = {0, 1, 0, -1};
    for (int distance = 18; distance <= 25; ++distance)
    {
        WorldPoint point{center.x + dx[direction] * distance, center.y + dy[direction] * distance};
        const auto* tile = world.Find(WorldInt(std::floor(point.x)), WorldInt(std::floor(point.y)));
        if (tile && !world.CanWalk(point.x, point.y))
        {
            continue;
        }
        Enemy boss;
        boss.id = nextId_++;
        boss.kind = EnemyKind::Boss;
        boss.position = boss.attackPoint = point;
        boss.health = boss.maxHealth = VillageBossHealth;
        boss.attackTimer = 3;
        enemies_.push_back(boss);
        phase_ = LevelPhase::BossFight;
        rpg_.bossTimer = BossInterval;
        rpg_.lastBossVillage = rpg_.bossVillage;
        Announce(std::wstring(Villages()[rpg_.bossVillage].name) +
                 L" 성문 밖에 잿빛 파수꾼 등장! 권장 Lv 10");
        std::cout << "[BossEvent] village=" << rpg_.bossVillage << " hp=" << boss.health
                  << " position=" << point.x << "," << point.y << '\n';
        urgentSave_ = true;
        return;
    }
    rpg_.bossTimer = 5;
}
