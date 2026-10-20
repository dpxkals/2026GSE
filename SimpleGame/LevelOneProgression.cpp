#include "stdafx.h"
#include "LevelOne.h"
#include "GameplayBehaviors.h"
#include "WorldSimulation.h"
#include <algorithm>
#include <cmath>
#include <queue>

void LevelOne::GainExperience(int amount)
{
    if (player_.level >= MaxCharacterLevel)
    {
        return;
    }
    player_.experience += amount;
    while (player_.level < MaxCharacterLevel && player_.experience >= player_.ExperienceNeeded())
    {
        player_.experience -= player_.ExperienceNeeded();
        ++player_.level;
        if (player_.health > 0.0)
        {
            player_.health = (std::min)(player_.MaxHealth(), player_.health + 25.0);
        }
        Announce(L"레벨 업! 공격력 · 발사 속도 · 최대 체력 · 이동/습득 범위 증가");
    }
    if (player_.level == MaxCharacterLevel)
    {
        player_.experience = 0;
    }
}

void LevelOne::Drop(DropKind kind, WorldPoint position, int amount)
{
    drops_.push_back({nextId_++, kind, position, amount});
}

void LevelOne::Defeat(const Enemy& enemy)
{
    if (simulation_)
    {
        simulation_->PlayerMonsterKilled();
    }
    if (enemy.kind == EnemyKind::Boss)
    {
        FinishBoss();
        rpg_.bosses = (std::min)(1000000000, rpg_.bosses + 1);
        GainExperience(180);
        if (simulation_)
        {
            simulation_->RewardGold(180);
        }
        Drop(DropKind::Equipment, enemy.position, int(random_() % 3) * 5 + 4);
        Drop(DropKind::Soul, enemy.position, 80);
        Drop(DropKind::Upgrade, enemy.position, 3);
        Drop(DropKind::Health, enemy.position, 60);
        Drop(DropKind::Magnet, enemy.position, 1);
        Announce(L"마을 보스 처치! 장비와 금화 획득 · 5분 뒤 다른 마을에서 다시 출현합니다.");
    }
    else
    {
        ++player_.kills;
        ++life_.kills;
        rpg_.hunts = (std::min)(1000000000, rpg_.hunts + 1);
        if (rpg_.hunts == 1 || random_() % 100 < 25)
        {
            int tier = int(random_() % ((std::min)(4, (player_.level - 1) / 3) + 1));
            Drop(DropKind::Equipment, enemy.position, int(random_() % 3) * 5 + tier + 1);
        }
        if (random_() % 100 < 12)
        {
            Drop(DropKind::Equipment, enemy.position, 16 + int(random_() % 3));
        }
        GainExperience(4);
        Drop(DropKind::Soul, enemy.position, 8);
        if (player_.kills % 4 == 0)
        {
            Drop(DropKind::Upgrade, enemy.position, 1);
        }
        if (player_.kills % 6 == 0 || random_() % 100 < 12)
        {
            Drop(DropKind::Health, enemy.position, 30);
        }
        if (player_.kills % 9 == 0)
        {
            Drop(DropKind::Magnet, enemy.position, 1);
        }
    }
    urgentSave_ = true;
}

void LevelOne::Hurt(double amount)
{
    if (invulnerable_ > 0.0 || InCamp(player_.position) ||
        (phase_ == LevelPhase::Cleared && !simulation_))
    {
        return;
    }
    amount = player_.IncomingDamage(amount);
    life_.damageTaken += (std::min)(player_.health, amount);
    player_.health = (std::max)(0.0, player_.health - amount);
    invulnerable_ = 0.65;
    numbers_.push_back({player_.position, int(amount), true, 0.75});
}
