#include "stdafx.h"
#include "LevelOne.h"
#include "GameplayBehaviors.h"
#include <algorithm>
#include <cmath>
#include <queue>

void LevelOne::GainExperience(int amount)
{
    if (player_.level >= 30)
    {
        return;
    }
    player_.experience += amount;
    while (player_.level < 30 && player_.experience >= player_.ExperienceNeeded())
    {
        player_.experience -= player_.ExperienceNeeded();
        ++player_.level;
        if (player_.health > 0.0)
        {
            player_.health = (std::min)(player_.MaxHealth(), player_.health + 25.0);
        }
        Announce(L"레벨 업! 공격력 · 발사 속도 · 최대 체력 · 이동/습득 범위 증가");
    }
    if (player_.level == 30)
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
    if (enemy.kind == EnemyKind::Boss)
    {
        phase_ = LevelPhase::Cleared;
        GainExperience(100);
        Drop(DropKind::Soul, enemy.position, 80);
        Drop(DropKind::Upgrade, enemy.position, 3);
        Drop(DropKind::Health, enemy.position, 60);
        Drop(DropKind::Magnet, enemy.position, 1);
        Announce(L"레벨 1 완료! 잿빛 파수꾼이 쓰러졌습니다. 남은 보상을 수집하세요.");
    }
    else
    {
        ++player_.kills;
        ++life_.kills;
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
    if (invulnerable_ > 0.0 || InCamp(player_.position) || phase_ == LevelPhase::Cleared)
    {
        return;
    }
    life_.damageTaken += (std::min)(player_.health, amount);
    player_.health = (std::max)(0.0, player_.health - amount);
    invulnerable_ = 0.65;
    numbers_.push_back({player_.position, int(amount), true, 0.75});
}
