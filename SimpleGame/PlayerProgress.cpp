#include "stdafx.h"
#include "GameplayTypes.h"
#include <algorithm>
#include <cmath>

int PlayerProgress::ExperienceNeeded() const
{
    return 24 + (level - 1) * 16;
}

double PlayerProgress::MaxHealth() const
{
    return 100.0 + (level - 1) * 15.0;
}

double PlayerProgress::Damage() const
{
    return 12.0 + (level - 1) * 2.5 + weaponRank * 4.0;
}

double PlayerProgress::ShotCooldown() const
{
    return (std::max)(0.24, 0.85 * std::pow(0.91, level - 1) * std::pow(0.96, weaponRank));
}

double PlayerProgress::MoveSpeed() const
{
    return 4.5 + (std::min)(0.9, (level - 1) * 0.06);
}

double PlayerProgress::PickupRange() const
{
    return (std::min)(4.0, 2.2 + (level - 1) * 0.15);
}

double PlayerProgress::AttackRange() const
{
    return 6.0;
}
