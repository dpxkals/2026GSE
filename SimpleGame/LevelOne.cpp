#include "stdafx.h"
#include "LevelOne.h"
#include "GameplayBehaviors.h"
#include <algorithm>
#include <cmath>

bool LevelOne::InCamp(WorldPoint position)
{
    return std::hypot(position.x - 0.5, position.y - 0.5) < 3.8;
}

void LevelOne::Announce(const std::wstring& text)
{
    notice_ = text;
    noticeTime_ = 4.0;
}

std::wstring LevelOne::Objective() const
{
    if (phase_ == LevelPhase::Cleared)
    {
        return L"레벨 1 완료 — 잿빛 파수꾼을 쓰러뜨렸습니다.";
    }
    if (phase_ == LevelPhase::BossFight)
    {
        return L"보스: 잿빛 파수꾼 — 붉은 공격 예고를 피하세요.";
    }
    return L"파밍: 적 " + std::to_wstring(player_.kills) + L" / 24 처치 후 보스 등장";
}

std::wstring LevelOne::Hint() const
{
    if (phase_ == LevelPhase::Cleared)
    {
        return L"파밍 훈련을 마쳤습니다. 남은 보상을 줍거나 거점으로 돌아가세요.";
    }
    if (InCamp(player_.position))
    {
        return L"안전 거점: 자동 회복 중. 원 밖으로 이동하면 자동 공격합니다.";
    }
    if (player_.souls == 0)
    {
        return L"적에게 6타일 이내로 접근하세요. 영혼석은 가까이 가면 자동 습득합니다.";
    }
    if (player_.upgrades == 0)
    {
        return L"주황색 강화석을 모으세요. 공격력과 발사 속도가 증가합니다.";
    }
    if (player_.magnets == 0)
    {
        return L"청록색 자석은 8초 동안 12타일 이내의 아이템을 끌어옵니다.";
    }
    return L"붉은 물약은 체력 회복, 영혼석은 성장. R: 사거리 표시 / P: 일시정지";
}

double LevelOne::ShotReady() const
{
    return 1.0 - std::clamp(shotTimer_ / player_.ShotCooldown(), 0.0, 1.0);
}

LevelOne::LevelOne(const std::filesystem::path& directory, std::uint64_t seed)
    : random_(static_cast<std::uint32_t>(seed ^ (seed >> 32))),
      seed_(seed),
      savePath_(directory / L"level_one.dat")
{
    Load();
    Announce(L"레벨 1 · 잿빛 들판 — 거점 밖으로 나가 파밍을 시작하세요.");
    SynchronizeActors();
}

void LevelOne::Update(double dt, const World& world, const NpcSystem& npcs)
{
    const auto* root = scene_.Find("gameplay");
    if (!error_.empty() || !world.Error().empty() || (root && !scene_.IsActive(root->GetId())))
    {
        return;
    }
    if (!world.Find(WorldInt(std::floor(player_.position.x)),
                    WorldInt(std::floor(player_.position.y))))
    {
        return;
    }
    if (!npcs.CanWalk(player_.position.x, player_.position.y, world))
    {
        SetPlayerPosition({});
        routeTimer_ = 0.0;
        Announce(L"이동 가능한 거점으로 위치를 복구했습니다.");
        return;
    }
    noticeTime_ -= dt;
    life_.seconds += dt;
    if (noticeTime_ <= 0.0)
    {
        notice_.clear();
    }
    invulnerable_ = (std::max)(0.0, invulnerable_ - dt);
    magnetTime_ = (std::max)(0.0, magnetTime_ - dt);
    shotTimer_ = (std::max)(0.0, shotTimer_ - dt);
    activeWorld_ = &world;
    activeNpcs_ = &npcs;
    SynchronizeActors();
    scene_.Update(UpdatePhase::Passive, dt);
    scene_.Update(UpdatePhase::Effects, dt);

    routeTimer_ -= dt;
    if (routeTimer_ <= 0.0)
    {
        RebuildRoutes(world, npcs);
        routeTimer_ = 0.25;
    }
    spawnTimer_ -= dt;
    if (phase_ == LevelPhase::Farming && !InCamp(player_.position))
    {
        if (player_.kills >= BossKillRequirement)
        {
            if (Spawn(EnemyKind::Boss, world))
            {
                phase_ = LevelPhase::BossFight;
                Announce(L"잿빛 파수꾼 등장! 붉은 원이 사라지기 전에 피하세요.");
                urgentSave_ = true;
            }
        }
        else if (spawnTimer_ <= 0.0)
        {
            int activeCount = 0;
            for (const Enemy& enemy : enemies_)
            {
                activeCount += Gameplay::Distance(enemy.position, player_.position) < 20.0 ? 1 : 0;
            }
            int limit = (std::min)(12, 4 + player_.kills / 4);
            if (activeCount < limit)
            {
                Spawn(random_() % 3 == 0 ? EnemyKind::Hound : EnemyKind::Husk, world);
            }
            spawnTimer_ = (std::max)(0.8, 2.2 - player_.kills * 0.045);
        }
    }
    if (InCamp(player_.position))
    {
        player_.health = (std::min)(player_.MaxHealth(), player_.health + 5.0 * dt);
    }
    if (phase_ != LevelPhase::Cleared)
    {
        SynchronizeActors();
        scene_.Update(UpdatePhase::Player, dt);
        scene_.Update(UpdatePhase::Enemies, dt);
        SynchronizeActors();
        scene_.Update(UpdatePhase::Projectiles, dt);
        ResolveCombat();
    }
    if (player_.health <= 0.0)
    {
        Die();
    }
    SynchronizeActors();
    scene_.Update(UpdatePhase::Items, dt);
    SynchronizeActors();
    saveTimer_ += dt;
    if (urgentSave_ || saveTimer_ >= 5.0)
    {
        Save();
        urgentSave_ = false;
        saveTimer_ = 0.0;
    }
}
