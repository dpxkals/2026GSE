#include "stdafx.h"
#include "SocietyView.h"
#include "VillageLayout.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

void SocietyView::CitizenBody(
    SceneRenderer& r, const Citizen& npc, Point2 p, float zoom, bool selected)
{
    Color colors[] = {{0.29f, 0.49f, 0.28f, 1},
                      {0.56f, 0.20f, 0.17f, 1},
                      {0.68f, 0.49f, 0.20f, 1},
                      {0.34f, 0.36f, 0.65f, 1},
                      {0.47f, 0.46f, 0.40f, 1},
                      {0.56f, 0.38f, 0.23f, 1},
                      {0.32f, 0.62f, 0.65f, 1}};
    auto tendency = WorldSimulation::Tendency(npc);
    Color color = colors[int(tendency)];
    auto key = "citizen:v2:" + std::to_string(int(tendency));
    if (r.BeginMesh(key, p, zoom))
    {
        r.Ellipse(p, 13 * zoom, 5 * zoom, {0, 0, 0, 0.35f});
        r.Triangle({p.x, p.y - 31 * zoom}, {p.x - 11 * zoom, p.y}, {p.x + 11 * zoom, p.y}, color);
        r.Ellipse({p.x, p.y - 34 * zoom}, 7 * zoom, 8 * zoom, {0.65f, 0.53f, 0.39f, 1});
        r.Rect(p.x - 8 * zoom, p.y - 42 * zoom, 16 * zoom, 5 * zoom, color);
        r.Line({p.x + 12 * zoom, p.y},
               {p.x + 12 * zoom, p.y - 30 * zoom},
               2 * zoom,
               {0.62f, 0.57f, 0.40f, 1});
        r.EndMesh();
    }
    r.DrawMesh(key, p, zoom);
    if (selected || npc.hostility > 0 || npc.health < npc.maxHealth)
    {
        r.Rect(p.x - 17 * zoom, p.y - 50 * zoom, 34 * zoom, 4 * zoom, {0.07f, 0.08f, 0.08f, 1});
        r.Rect(p.x - 17 * zoom,
               p.y - 50 * zoom,
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
        r.Rect(20, top, 304, 210, ink);
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
                     L"성향은 행동 결과에 따라 변합니다.\nCtrl+좌클릭: 공격   우클릭: 해제",
                     text,
                     12,
                     280);
        r.KoreanText(32,
                     top + 183,
                     sim.AttackingSelected() ? L"공격 대상으로 지정됨"
                                             : L"관찰 중 · 중립 대상 공격 안 함",
                     sim.AttackingSelected() ? Color{1, 0.4f, 0.3f, 1} : gold,
                     13,
                     280);
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
