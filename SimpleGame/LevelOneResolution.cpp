#include "stdafx.h"
#include "LevelOne.h"
#include "WorldSimulation.h"
#include "GameplayBehaviors.h"
#include <algorithm>

void LevelOne::ResolveCombat()
{
    for (auto it = enemies_.begin(); it != enemies_.end();)
    {
        if (it->health <= 0.0)
        {
            if (simulation_ && it->lastHitNpc)
            {
                simulation_->MonsterKilled(it->lastHitNpc, *it);
                if (it->playerDamage > 0 &&
                    Gameplay::Distance(player_.position, it->position) <= 30)
                {
                    Defeat(*it);
                }
                else if (it->kind == EnemyKind::Boss)
                {
                    FinishBoss();
                    Announce(L"주민이 마을 보스를 처치했습니다. 5분 뒤 다른 마을에서 출현합니다.");
                }
                urgentSave_ = true;
            }
            else
            {
                Defeat(*it);
            }
            it = enemies_.erase(it);
        }
        else
        {
            ++it;
        }
    }
    // Include the final frame's kills in the life snapshot; XP never revives zero HP.
    if (player_.health <= 0.0)
    {
        Die();
    }
    if (phase_ == LevelPhase::Cleared && !simulation_)
    {
        projectiles_.clear();
    }
}

void LevelOne::Collect(const ItemDrop& drop)
{
    switch (drop.kind)
    {
        case DropKind::Equipment:
        {
            int item = drop.amount - 1;
            if (item < 0 || item >= RpgItemCount || rpg_.inventory[item] >= 999)
            {
                return;
            }
            ++rpg_.inventory[item];
            rpg_.finds = (std::min)(1000000000, rpg_.finds + 1);
            Announce(std::wstring(ItemCatalog()[item].name) + L" 획득 · Tab으로 확인");
            break;
        }
        case DropKind::Soul:
            GainExperience(drop.amount);
            ++player_.souls;
            ++life_.souls;
            break;
        case DropKind::Upgrade:
            player_.weaponRank = (std::min)(20, player_.weaponRank + drop.amount);
            ++player_.upgrades;
            Announce(L"무기 강화 +" + std::to_wstring(player_.weaponRank) +
                     L" — 공격력과 발사 속도 증가");
            break;
        case DropKind::Health:
            player_.health = (std::min)(player_.MaxHealth(), player_.health + drop.amount);
            ++player_.potions;
            ++life_.heals;
            Announce(L"회복 물약 자동 습득 — 체력을 회복했습니다.");
            break;
        case DropKind::Magnet:
            magnetTime_ = 8.0;
            ++player_.magnets;
            Announce(L"영혼의 자석 — 8초 동안 주변 아이템을 끌어옵니다.");
            break;
    }

    urgentSave_ = true;
}
