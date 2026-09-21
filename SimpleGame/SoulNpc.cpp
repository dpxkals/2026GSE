#include "stdafx.h"
#include "LevelOne.h"
#include <cmath>

std::wstring SoulNpc::Name() const
{
    return L"잔향 " + std::to_wstring(deathNumber);
}

std::wstring SoulNpc::Role() const
{
    if (life.distance > 120.0 && life.distance > life.kills * 12.0)
    {
        return L"길을 기억하는 영혼";
    }
    if (life.conversations >= 2)
    {
        return L"목소리를 기억하는 영혼";
    }
    if (life.souls >= 6)
    {
        return L"불씨를 모으는 영혼";
    }
    if (life.kills >= 4)
    {
        return L"싸움을 기억하는 영혼";
    }
    return L"깨어난 영혼";
}

void LevelOne::Die()
{
    SoulNpc soul;
    soul.id = nextId_++;
    soul.deathNumber = ++player_.retreats;
    soul.position = player_.position;
    soul.life = life_;
    soul.inheritedLevel = player_.level;
    soul.inheritedWeapon = player_.weaponRank;
    soul.feeling = life_.heals >= 2 ? 1 : (life_.kills >= 6 ? 2 : 0);
    soulNpcs_.push_back(soul);

    // Birth, life snapshot and resurrection share ONE atomic checkpoint.
    // Souls are non-solid; repeated deaths never block a corridor or spawn point.
    life_ = {};
    SetPlayerPosition({});
    player_.health = player_.MaxHealth();
    invulnerable_ = 2.0;
    projectiles_.clear();
    target_ = 0;
    routeTimer_ = 0.0;
    urgentSave_ = true;
    Announce(L"죽음이 하나의 생명이 되었습니다. 사망한 장소의 '" + soul.Name() +
             L"'에게 돌아가 F로 말을 걸어보세요.");
}

int LevelOne::NearestSoul(double x, double y, const World& world) const
{
    int result = -1;
    double nearest = 2.2;
    for (size_t i = 0; i < soulNpcs_.size(); ++i)
    {
        const auto& soul = soulNpcs_[i];
        double distance = std::hypot(soul.position.x - x, soul.position.y - y);
        bool closer = distance < nearest - 0.25;
        bool lessKnown = result >= 0 && std::abs(distance - nearest) <= 0.25 &&
                         soul.meetings < soulNpcs_[result].meetings;
        if (distance < 2.2 && (result < 0 || closer || lessKnown) &&
            world.CanWalk(soul.position.x, soul.position.y) &&
            world.ClearLine(x, y, soul.position.x, soul.position.y))
        {
            result = int(i);
            nearest = distance;
        }
    }
    return result;
}

void LevelOne::RecordConversation()
{
    ++life_.conversations;
    urgentSave_ = true;
}

std::vector<std::wstring> LevelOne::SoulConversation(int index)
{
    if (index < 0 || index >= int(soulNpcs_.size()) || !error_.empty())
    {
        return {};
    }
    auto& soul = soulNpcs_[index];
    bool met = soul.meetings > 0;
    ++soul.meetings;
    RecordConversation();
    std::vector<std::wstring> pages;
    pages.push_back(
        met ? L"또 왔구나. 네가 떠난 뒤에도 나는 이곳에 남아 있었어. 우리는 더는 같은 삶을 살지 않겠지."
            : L"네 숨이 끊어진 자리에서 내가 눈을 떴어. 네가 되돌아온다고 해서, 내가 사라지는 것은 아니었구나.");
    pages.push_back(L"나는 " + soul.Role() + L". " + std::to_wstring(int(soul.life.distance)) +
                    L"타일을 걸었던 발걸음, " + std::to_wstring(soul.life.shots) +
                    L"번 활을 놓았던 손끝이 남아 있어. 그것이 내 첫 기억이야.");
    if (soul.life.conversations > 0)
    {
        pages.push_back(
            L"누군가에게 말을 걸었던 " + std::to_wstring(soul.life.conversations) +
            L"번의 순간도 남아 있어. 무슨 뜻이었는지는 몰라도, 침묵을 깨려 했다는 건 기억해.");
    }
    if (soul.life.souls > 0)
    {
        pages.push_back(L"우리는 작은 영혼석을 " + std::to_wstring(soul.life.souls) +
                        L"개 모았지. 그것들을 쥐었을 때의 온기는 누구의 것이었을까?");
    }
    pages.push_back(
        soul.feeling == 1
            ? L"차가운 기억 사이로 회복의 온기가 남았어. 나는 그것을 희망이라고 불러보려 해. 네 마음도 그랬는지는 모르겠어."
        : soul.feeling == 2
            ? L"멈추지 않았던 손의 떨림이 남았어. 내 안에서는 결의처럼 느껴져. 네가 싸운 까닭까지 안다는 뜻은 아니야."
            : L"눈을 뜨자 낯선 두려움이 밀려왔어. 네 감정을 읽은 것은 아니야. 네 마지막 흔적을 받은 내가 느끼는 감정이지.");
    pages.push_back(L"몸은 " + std::to_wstring(soul.inheritedLevel) + L"번째 성장과 무기 +" +
                    std::to_wstring(soul.inheritedWeapon) +
                    L"의 기억을 남겼어. 그런데 돌아온 네 그림자에는 왜 빈자리가 없지?");
    // Persist the meeting even if the window closes during the paused dialogue.
    Save();
    return pages;
}
