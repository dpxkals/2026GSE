#include "stdafx.h"
#include "SocietyView.h"
#include "CharacterArt.h"
#include "VillageLayout.h"
#include "AssetRuntime.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

void SocietyView::CitizenBody(SceneRenderer& r,
                              const Citizen& npc,
                              Point2 p,
                              float zoom,
                              bool selected,
                              const ActorPose& pose)
{
    ActorPose actionPose = pose;
    if (npc.action == NpcAction::Gather || npc.action == NpcAction::Craft ||
        npc.action == NpcAction::Socialize || npc.action == NpcAction::Trade ||
        npc.action == NpcAction::Aid)
    {
        actionPose.work = pose.breath * (1 - pose.moving);
    }
    if (!AssetRuntime::Instance().DrawCitizen(r, npc.id, p, zoom, actionPose))
    {
        CharacterArt::Human(
            r, p, zoom, 3 + int(WorldSimulation::Tendency(npc)), npc.level, actionPose);
    }
    if (selected || npc.hostility > 0 || npc.health < npc.maxHealth)
    {
        r.Rect(p.x - 17 * zoom, p.y - 59 * zoom, 34 * zoom, 4 * zoom, {0.07f, 0.08f, 0.08f, 1});
        r.Rect(p.x - 17 * zoom,
               p.y - 59 * zoom,
               34 * zoom * float(npc.health / npc.maxHealth),
               4 * zoom,
               npc.hostility > 0 ? Color{0.95f, 0.22f, 0.15f, 1} : Color{0.25f, 0.8f, 0.4f, 1});
    }
    if (selected)
    {
        r.Ellipse({p.x, p.y + 3 * zoom}, 17 * zoom, 4 * zoom, {0.95f, 0.76f, 0.4f, 0.5f});
    }
}

void SocietyView::Panel(SceneRenderer& r, const WorldSimulation& sim, int width, int height)
{
    if (width < 700 || height < 560)
    {
        return;
    }
    const Color gold{0.83f, 0.70f, 0.44f, 1};
    const Color ink{0.035f, 0.045f, 0.045f, 0.90f};
    const Color text{0.82f, 0.83f, 0.76f, 1};
    float mapSize = (std::min)(std::clamp(width * 0.20f, 124.0f, 184.0f), float(height) - 250.0f);
    float purseTop = 54 + mapSize + 60;
    r.Rect(float(width) - mapSize - 34, purseTop, mapSize + 20, 54, ink);
    r.Rect(float(width) - mapSize - 34, purseTop, mapSize + 20, 1, gold);
    r.KoreanText(float(width) - mapSize - 24,
                 purseTop + 7,
                 L"금화  " + std::to_wstring(sim.PlayerGold()),
                 gold,
                 16,
                 int(mapSize));
    r.KoreanText(float(width) - mapSize - 24,
                 purseTop + 30,
                 L"재료  " + std::to_wstring(sim.PlayerGoods()),
                 text,
                 13,
                 int(mapSize));
    float top = 166;
    const auto* n = sim.Find(sim.Selected());
    if (n)
    {
        r.Rect(20, top, 304, 274, ink);
        r.Rect(20, top, 304, 1, gold);
        r.KoreanText(32,
                     top + 10,
                     WorldSimulation::Name(*n) + L" · " +
                         WorldSimulation::TendencyName(WorldSimulation::Tendency(*n)),
                     gold,
                     17,
                     280);
        std::wostringstream info;
        info << L"Lv " << n->level << L"   HP " << int(n->health) << L" / " << int(n->maxHealth)
             << L"\n행동  " << WorldSimulation::ActionName(n->action) << L"  ·  "
             << WorldSimulation::GoalName(n->goal) << L"\n금화 " << n->gold << L"   재료 "
             << n->goods << L"   깨달음 " << n->insight;
        r.KoreanText(32, top + 39, info.str(), text, 13, 280);
        auto best = std::max_element(n->learned.begin(), n->learned.end());
        r.KoreanText(
            32,
            top + 108,
            L"경험 편향  " +
                std::wstring(WorldSimulation::ActionName(NpcAction(best - n->learned.begin()))) +
                L"  " + std::to_wstring(int(*best)),
            gold,
            14,
            280);
        r.KoreanText(32,
                     top + 136,
                     L"집  " + sim.HomeAddress(*n) + L"\n" + sim.HeardNews(*n),
                     text,
                     12,
                     280);
        r.KoreanText(32,
                     top + 215,
                     n->travelVillage >= 0
                         ? std::wstring(n->reservedHouse >= 0 ? L"이사 중 → " : L"방문 중 → ") +
                               Villages()[n->travelVillage].name
                         : L"가까이 F 소식 듣기 · 우클릭 해제",
                     sim.AttackingSelected() ? Color{1, 0.4f, 0.3f, 1} : gold,
                     13,
                     280);
        r.KoreanText(32, top + 246, L"주택 소유·소문·이사는 모니터에서도 확인", gold, 12, 280);
    }
    float newsTop = float(height) - 176;
    r.Rect(20, newsTop, 340, 78, ink);
    r.Rect(20, newsTop, 2, 78, gold);
    r.KoreanText(32, newsTop + 6, L"마을 소식", gold, 13, 316);
    int row = 0;
    for (const auto& news : sim.News())
    {
        if (row >= 2)
        {
            break;
        }
        std::wstring brief = news.size() > 25 ? news.substr(0, 24) + L"…" : news;
        r.KoreanText(32, newsTop + 28 + row * 22, brief, text, 12, 316);
        ++row;
    }
    if (!sim.Error().empty())
    {
        r.KoreanText(32, 382, L"시뮬레이션 저장 오류: 콘솔 확인", {1, 0.3f, 0.2f, 1}, 12, 310);
    }
}
