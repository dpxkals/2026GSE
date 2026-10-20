#include "stdafx.h"
#include "RpgPanel.h"
#include "LevelOne.h"
#include "WorldSimulation.h"
#include "SceneRenderer.h"
#include <algorithm>

namespace
{
    struct Layout
    {
        float x, y, w, h, list, detail;
        int rows;

        Layout(int width, int height)
        {
            w = float((std::min)(1000, width - 32));
            h = float((std::min)(660, height - 32));
            x = (width - w) / 2;
            y = (height - h) / 2;
            list = w * .58f;
            detail = x + list + 30;
            rows = std::clamp(int((h - 265) / 42), 2, 9);
        }
    };

    const Color Gold{.87f, .73f, .44f, 1}, Text{.86f, .88f, .84f, 1}, Muted{.57f, .65f, .63f, 1};
}

void RpgPanel::Open(int mode, std::uint64_t merchant)
{
    mode_ = mode;
    merchant_ = merchant;
    selected_ = page_ = 0;
}

std::vector<int> RpgPanel::Entries(const LevelOne& level) const
{
    std::vector<int> result;
    if (mode_ == 3)
    {
        for (int i = 0; i <= RpgQuestCount; ++i)
        {
            result.push_back(i);
        }
    }
    else
    {
        for (int i = 0; i < RpgItemCount; ++i)
        {
            if ((mode_ == 2 && i < 17) || (mode_ == 1 && level.Rpg().inventory[i] > 0))
            {
                result.push_back(i);
            }
        }
    }
    return result;
}

void RpgPanel::Key(unsigned char key, LevelOne& level, WorldSimulation& society)
{
    if (key == 27 || key == '\t' || key == 'q')
    {
        Close();
        return;
    }
    if (key >= '1' && key <= '3')
    {
        int mode = key - '0';
        if (mode != 2 || merchant_)
        {
            mode_ = mode;
            selected_ = page_ = 0;
        }
        return;
    }
    if (key == 'm' && merchant_ && level.Error().empty() && society.PlayerGoods() > 0 &&
        society.CanTrade(merchant_, level.Player().position))
    {
        society.Select(merchant_);
        society.TradePlayer(level, false);
        society.Save();
        level.Save();
        return;
    }
    auto entries = Entries(level);
    if (entries.empty())
    {
        return;
    }
    if (key == 'w')
    {
        selected_ = (std::max)(0, selected_ - 1);
    }
    if (key == 's')
    {
        selected_ = (std::min)(int(entries.size()) - 1, selected_ + 1);
    }
    if (key == 'a')
    {
        selected_ = (std::max)(0, selected_ - rows_);
    }
    if (key == 'd')
    {
        selected_ = (std::min)(int(entries.size()) - 1, selected_ + rows_);
    }
    selected_ = std::clamp(selected_, 0, int(entries.size()) - 1);
    page_ = selected_ / rows_;
    int id = entries[selected_];
    if (mode_ == 3 && (key == '\r' || key == 'e'))
    {
        level.ClaimQuest(id);
    }
    if (mode_ == 1 && (key == '\r' || key == 'e'))
    {
        level.UseItem(id);
    }
    if (mode_ == 2 && (key == '\r' || key == 'b'))
    {
        level.BuyItem(id, merchant_);
    }
    if (mode_ == 1 && key == 'x' && merchant_)
    {
        level.SellItem(id, merchant_);
    }
}

void RpgPanel::Click(int x, int y, int width, int height, LevelOne& level, WorldSimulation& society)
{
    if (!IsOpen() || width < 760 || height < 600)
    {
        return;
    }
    Layout p(width, height);
    rows_ = p.rows;
    if (y >= p.y + 57 && y <= p.y + 92)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (x >= p.x + 18 + i * 125 && x <= p.x + 135 + i * 125)
            {
                Key(static_cast<unsigned char>('1' + i), level, society);
            }
        }
        return;
    }
    auto entries = Entries(level);
    if (x >= p.x + 18 && x <= p.x + p.list && y >= p.y + 132 && y < p.y + 132 + rows_ * 42)
    {
        int index = page_ * rows_ + int((y - p.y - 132) / 42);
        if (index < int(entries.size()))
        {
            selected_ = index;
        }
        return;
    }
    if (y >= p.y + p.h - 126 && y <= p.y + p.h - 98)
    {
        if (x >= p.x + 18 && x <= p.x + 110)
        {
            Key('a', level, society);
        }
        if (x >= p.x + 120 && x <= p.x + 212)
        {
            Key('d', level, society);
        }
    }
    if (y >= p.y + p.h - 88 && y <= p.y + p.h - 54)
    {
        if (x >= p.x + 18 && x <= p.x + 198)
        {
            Key(mode_ == 2 ? 'b' : 'e', level, society);
        }
        if (x >= p.x + 208 && x <= p.x + 388)
        {
            Key('x', level, society);
        }
        if (x >= p.x + 398 && x <= p.x + 578)
        {
            Key('m', level, society);
        }
    }
}

void RpgPanel::Draw(
    SceneRenderer& r, LevelOne& level, WorldSimulation& society, int width, int height)
{
    if (!IsOpen())
    {
        if (width >= 760 && height >= 600 && !society.Find(society.Selected()))
        {
            int quest = 0;
            while (quest < RpgQuestCount && level.Rpg().claimed[quest])
            {
                ++quest;
            }
            r.Rect(20, 166, 304, 106, {.035f, .045f, .052f, .94f});
            r.KoreanText(32, 176, L"진행 의뢰  ·  Q", Gold, 16, 280);
            r.KoreanText(32, 202, level.QuestTitle(quest), Text, 14, 280);
            r.KoreanText(32,
                         246,
                         std::to_wstring(level.QuestProgress(quest)) + L" / " +
                             std::to_wstring(level.QuestTarget(quest)) +
                             (level.QuestProgress(quest) >= level.QuestTarget(quest)
                                  ? L" · Q에서 보상 받기"
                                  : L""),
                         Gold,
                         14,
                         280);
        }
        return;
    }
    if (width < 760 || height < 600)
    {
        r.Rect(0, 0, float(width), float(height), {.02f, .03f, .04f, .98f});
        r.KoreanText(20,
                     30,
                     L"가방을 보려면 창을 760 × 600 이상으로 늘려주세요.\nTab / Esc 닫기",
                     Text,
                     18,
                     (std::max)(100, width - 40));
        return;
    }
    Layout p(width, height);
    rows_ = p.rows;
    r.Rect(0, 0, float(width), float(height), {0, 0, 0, .68f});
    r.Rect(p.x, p.y, p.w, p.h, {.035f, .045f, .052f, .99f});
    r.Rect(p.x, p.y, p.w, 2, Gold);
    auto text = [&](float x, float y, const std::wstring& s, Color c, int size, int wrap)
    {
        r.KoreanText(x, y, s, c, size, wrap);
    };
    text(p.x + 20, p.y + 16, L"여행자의 기록  ·  세계 일시정지", Gold, 22, int(p.w) - 280);
    text(p.x + p.w - 245,
         p.y + 20,
         L"금화 " + std::to_wstring(society.PlayerGold()) + L"  ·  Tab 닫기",
         Text,
         16,
         230);
    const wchar_t* tabs[] = {L"1  인벤토리", L"2  상점", L"3  퀘스트"};
    for (int i = 0; i < 3; ++i)
    {
        r.Rect(p.x + 18 + i * 125,
               p.y + 57,
               117,
               35,
               mode_ == i + 1 ? Color{.23f, .25f, .20f, 1} : Color{.09f, .12f, .13f, 1});
        text(p.x + 27 + i * 125, p.y + 63, tabs[i], i == 1 && !merchant_ ? Muted : Gold, 16, 110);
    }
    const auto* trader = society.Find(merchant_);
    text(p.x + 20,
         p.y + 103,
         trader ? WorldSimulation::Name(*trader) + L" · 보유 금화 " + std::to_wstring(trader->gold)
                : L"주민과의 거래는 마을에서 가까이 다가가 T",
         Muted,
         14,
         int(p.w) - 40);
    auto entries = Entries(level);
    selected_ = entries.empty() ? 0 : std::clamp(selected_, 0, int(entries.size()) - 1);
    page_ = selected_ / rows_;
    for (int row = 0; row < rows_ && page_ * rows_ + row < int(entries.size()); ++row)
    {
        int index = page_ * rows_ + row, id = entries[index];
        float y = p.y + 132 + row * 42;
        r.Rect(p.x + 18,
               y,
               p.list - 18,
               38,
               index == selected_ ? Color{.18f, .24f, .25f, 1} : Color{.07f, .09f, .10f, 1});
        std::wstring label;
        Color color = Text;
        if (mode_ == 3)
        {
            bool done = id < RpgQuestCount && level.Rpg().claimed[id];
            label = (done ? L"[완료] " : L"") + level.QuestTitle(id);
            color = done ? Muted : Gold;
        }
        else
        {
            bool worn = std::find(level.Rpg().equipped.begin(), level.Rpg().equipped.end(), id) !=
                        level.Rpg().equipped.end();
            label = (worn ? L"[착용] " : L"") + std::wstring(ItemCatalog()[id].name) +
                    (mode_ == 1 ? L" ×" + std::to_wstring(level.Rpg().inventory[id])
                                : L" · " + std::to_wstring(society.ItemPrice(id, merchant_, true)) +
                                      L" 금화");
            if (worn)
            {
                color = Gold;
            }
        }
        text(p.x + 28, y + 8, label, color, 15, int(p.list) - 40);
    }
    if (entries.empty())
    {
        text(p.x + 28,
             p.y + 148,
             L"가방이 비어 있습니다.\n몬스터를 처치하거나 퀘스트 보상을 받으세요.",
             Muted,
             16,
             int(p.list) - 40);
    }
    int wrap = int(p.w - p.list) - 50;
    if (!entries.empty())
    {
        int id = entries[selected_];
        if (mode_ == 3)
        {
            text(p.detail, p.y + 140, level.QuestTitle(id), Gold, 18, wrap);
            text(p.detail,
                 p.y + 208,
                 L"진행 " + std::to_wstring(level.QuestProgress(id)) + L" / " +
                     std::to_wstring(level.QuestTarget(id)),
                 Text,
                 18,
                 wrap);
            text(p.detail, p.y + 245, level.QuestReward(id), Muted, 16, wrap);
            text(
                p.detail,
                p.y + 315,
                L"목표 달성 후 [보상 받기]\n보스는 직접 공격하고 30타일 안에서 처치를 지켜보면 공동 처치도 인정됩니다.",
                Muted,
                14,
                wrap);
        }
        else
        {
            text(p.detail, p.y + 138, ItemCatalog()[id].name, Gold, 20, wrap);
            text(p.detail, p.y + 178, ItemDescription(id), Text, 16, wrap);
            if (trader)
            {
                text(p.detail,
                     p.y + 252,
                     L"구매 " + std::to_wstring(society.ItemPrice(id, merchant_, true)) +
                         L" / 판매 " + std::to_wstring(society.ItemPrice(id, merchant_, false)) +
                         L" 금화",
                     Muted,
                     15,
                     wrap);
            }
            std::wstring equipment = L"현재 착용\n";
            for (int equipped : level.Rpg().equipped)
            {
                equipment +=
                    (equipped < 0 ? L"없음" : ItemCatalog()[equipped].name) + std::wstring(L"\n");
            }
            text(p.detail, p.y + 295, equipment, Muted, 15, wrap);
        }
    }
    auto button = [&](float x, float y, float w, const std::wstring& label)
    {
        r.Rect(x, y, w, 30, {.19f, .20f, .16f, 1});
        text(x + 8, y + 5, label, Gold, 14, int(w) - 12);
    };
    button(p.x + 18, p.y + p.h - 126, 92, L"A 이전");
    button(p.x + 120, p.y + p.h - 126, 92, L"D 다음");
    text(p.x + 228,
         p.y + p.h - 120,
         std::to_wstring(page_ + 1) + L" 페이지 · W/S 선택",
         Muted,
         14,
         260);
    button(p.x + 18,
           p.y + p.h - 88,
           180,
           mode_ == 3   ? L"E 보상 받기"
           : mode_ == 2 ? L"B 구매"
                        : L"E 착용 / 해제 / 사용");
    if (merchant_ && mode_ == 1)
    {
        button(p.x + 208, p.y + p.h - 88, 180, L"X 선택 아이템 판매");
    }
    if (merchant_)
    {
        button(p.x + 398, p.y + p.h - 88, 180, L"M 사냥 재료 판매");
    }
    text(p.x + 20,
         p.y + p.h - 44,
         level.Notice().empty() ? L"클릭으로 선택 · Enter 실행 · Esc 닫기" : level.Notice(),
         Gold,
         14,
         int(p.w) - 40);
}
