/*
Copyright 2022 Lee Taek Hee (Tech University of Korea)

This program is free software: you can redistribute it and/or modify
it under the terms of the What The Hell License. Do it plz.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY.
*/

#include "stdafx.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "SceneRenderer.h"
#include "FrameProfiler.h"
#include "World.h"
#include "NpcSystem.h"
#include "LevelOne.h"
#include "LevelOneView.h"
#include "WorldActors.h"
#include "WorldView.h"
#include "WorldSimulation.h"
#include "WorldMonitor.h"
#include "RpgPanel.h"
#include "SocietyView.h"
#include "CharacterArt.h"
#include "VillageLayout.h"
#include "Dependencies/freeglut.h"
#include <Windows.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <vector>
#include <stdexcept>

namespace
{
    SceneRenderer renderer;
    std::unique_ptr<World> world;
    std::unique_ptr<NpcSystem> npcs;
    std::unique_ptr<LevelOne> levelOne;
    std::unique_ptr<WorldSimulation> society;
    std::unique_ptr<WorldMonitor> monitor;
    RpgPanel rpgPanel;
    std::filesystem::path gameplaySaveDirectory;
    int speakingNpc = -1;
    int speakingSoul = -1;
    bool transactionDialogue = false;
    double renderDelta = 0.016;
    size_t dialoguePage = 0;
    std::vector<std::wstring> dialoguePages;
    int width = 1280;
    int height = 800;

    double playerX = 0.5;
    double playerY = 0.5;
    double cameraX = 0.5;
    double cameraY = 0.5;

    double elapsed = 0.0;
    double animationClock = 0.0;
    double stepPhase = 0.0;
    float zoom = 1.0f;

    bool keys[256] = {};
    bool arrows[4] = {};

    bool showChunks = false;
    bool showHelp = false;
    bool showDiagnostics = false;
    bool moving = false;
    bool running = true;
    bool paused = false;
    bool showAttackRange = false;

    std::string message;
    double messageUntil = 0.0;
    std::chrono::steady_clock::time_point lastTick;
    const Color ivory{0.83f, 0.83f, 0.72f, 1};
    const Color muted{0.45f, 0.56f, 0.54f, 1};
    const Color gold{0.93f, 0.65f, 0.29f, 1};

    Point2 Project(double x, double y, float elevation = 0)
    {
        // Subtract in double precision BEFORE converting to screen-space floats.
        double dx = x - cameraX, dy = y - cameraY;
        return {width * 0.5f + float(dx - dy) * 36 * zoom,
                height * 0.52f + float(dx + dy) * 18 * zoom - elevation * zoom};
    }

    Color Shade(Color c, float factor)
    {
        return {c.r * factor, c.g * factor, c.b * factor, c.a, c.energy};
    }

    void Box(Point2 p, float w, float depth, float h, Color color)
    {
        w *= zoom;
        depth *= zoom;
        h *= zoom;
        renderer.Quad({p.x - w, p.y - depth},
                      {p.x, p.y},
                      {p.x, p.y - h},
                      {p.x - w, p.y - depth - h},
                      Shade(color, 0.68f));
        renderer.Quad({p.x, p.y},
                      {p.x + w, p.y - depth},
                      {p.x + w, p.y - depth - h},
                      {p.x, p.y - h},
                      Shade(color, 0.87f));
        renderer.Quad({p.x, p.y - h},
                      {p.x + w, p.y - depth - h},
                      {p.x, p.y - depth * 2 - h},
                      {p.x - w, p.y - depth - h},
                      color);
    }

    void GroundTile(WorldInt x, WorldInt y, const Tile& tile, bool capture = false)
    {
        Point2 center = Project(x + 0.5, y + 0.5);
        if (!capture &&
            (center.x < -80 || center.x > width + 80 || center.y < -80 || center.y > height + 80))
        {
            return;
        }
        Color color{0.19f, 0.25f, 0.20f, 1};
        if (tile.ground == Ground::Dirt)
        {
            color = {0.32f, 0.29f, 0.23f, 1};
        }
        if (tile.ground == Ground::Stone)
        {
            color = {0.29f, 0.32f, 0.31f, 1};
        }
        if (tile.ground == Ground::Water)
        {
            color = {0.10f, 0.21f, 0.25f, 1};
        }
        color = Shade(color, 0.90f + tile.variation * 0.025f);
        auto a = Project(double(x), double(y));
        auto b = Project(double(x + 1), double(y));
        auto c = Project(double(x + 1), double(y + 1));
        auto d = Project(double(x), double(y + 1));
        renderer.Quad(a, b, c, d, color);
        renderer.Line(d, c, 0.7f * zoom, {0.05f, 0.09f, 0.08f, 0.16f});
        if (tile.ground != Ground::Water && tile.variation % 3 == 0 && tile.prop == Prop::None)
        {
            renderer.Line({center.x - 5 * zoom, center.y + 2 * zoom},
                          {center.x - 3 * zoom, center.y - 2 * zoom},
                          zoom,
                          Shade(color, 1.28f));
            renderer.Line({center.x + 4 * zoom, center.y + 4 * zoom},
                          {center.x + 7 * zoom, center.y + 2 * zoom},
                          zoom,
                          Shade(color, 0.72f));
        }
    }

    void Tree(Point2 p, int variation)
    {
        renderer.Ellipse(
            {p.x + 9 * zoom, p.y + 1 * zoom}, 25 * zoom, 9 * zoom, {0.01f, 0.03f, 0.03f, 0.34f});
        Box({p.x, p.y + 2 * zoom}, 3, 2, 29, {0.26f, 0.23f, 0.18f, 1});
        float treeHeight = (66 + variation * 4) * zoom;
        for (int layer = 0; layer < 3; ++layer)
        {
            float base = p.y - (18 + layer * 17) * zoom;
            float half = (27 - layer * 6) * zoom;
            float tip = p.y - treeHeight - layer * 3 * zoom;
            renderer.Triangle(
                {p.x - half, base},
                {p.x, tip},
                {p.x, base + 9 * zoom},
                {0.095f + layer * 0.015f, 0.18f + layer * 0.018f, 0.15f + layer * 0.014f, 1});
            renderer.Triangle(
                {p.x, base + 9 * zoom},
                {p.x, tip},
                {p.x + half, base},
                {0.13f + layer * 0.015f, 0.23f + layer * 0.018f, 0.19f + layer * 0.014f, 1});
        }
    }

    void Ruin(Point2 p)
    {
        renderer.Ellipse(
            {p.x + 12 * zoom, p.y + 4 * zoom}, 35 * zoom, 13 * zoom, {0.01f, 0.02f, 0.025f, 0.3f});
        Box(p, 27, 13, 8, {0.37f, 0.39f, 0.35f, 1});
        Box({p.x - 15 * zoom, p.y - 9 * zoom}, 8, 4, 58, {0.43f, 0.46f, 0.41f, 1});
        Box({p.x + 15 * zoom, p.y - 9 * zoom}, 8, 4, 43, {0.39f, 0.42f, 0.39f, 1});
        Box({p.x - 5 * zoom, p.y - 63 * zoom}, 18, 5, 9, {0.48f, 0.49f, 0.42f, 1});
        renderer.Line({p.x - 19 * zoom, p.y - 34 * zoom},
                      {p.x - 12 * zoom, p.y - 38 * zoom},
                      2 * zoom,
                      {0.14f, 0.20f, 0.17f, 1});
        renderer.Line({p.x + 12 * zoom, p.y - 23 * zoom},
                      {p.x + 20 * zoom, p.y - 26 * zoom},
                      2 * zoom,
                      {0.14f, 0.20f, 0.17f, 1});
        Box({p.x + 20 * zoom, p.y + 9 * zoom}, 9, 4, 6, {0.29f, 0.34f, 0.28f, 1});
    }

    void BeaconBase(Point2 p)
    {
        renderer.Ellipse(p, 24 * zoom, 10 * zoom, {0.015f, 0.025f, 0.02f, 0.4f});
        Box({p.x, p.y + 4 * zoom}, 17, 8, 8, {0.40f, 0.42f, 0.36f, 1});
        Box({p.x, p.y - 4 * zoom}, 6, 3, 28, {0.42f, 0.43f, 0.38f, 1});
        Box({p.x, p.y - 31 * zoom}, 12, 6, 5, {0.27f, 0.29f, 0.28f, 1});
        renderer.Ellipse({p.x, p.y - 38 * zoom}, 7 * zoom, 3 * zoom, {0.08f, 0.09f, 0.09f, 1});
    }

    void BeaconFlame(Point2 p)
    {
        Point2 flame{p.x, p.y - 44 * zoom};
        for (int i = 5; i >= 1; --i)
        {
            renderer.Ellipse(
                flame, (12 + i * 5) * zoom, (12 + i * 5) * zoom, {0.96f, 0.53f, 0.17f, 0.028f});
        }
        float flicker = float(std::sin(elapsed * 8 + p.x)) * 2 * zoom;
        renderer.Triangle({p.x - 8 * zoom, p.y - 38 * zoom},
                          {p.x + flicker, p.y - 65 * zoom},
                          {p.x + 8 * zoom, p.y - 38 * zoom},
                          {0.95f, 0.48f, 0.12f, 1, 6.0f});
        renderer.Triangle({p.x - 4 * zoom, p.y - 38 * zoom},
                          {p.x - flicker, p.y - 53 * zoom},
                          {p.x + 4 * zoom, p.y - 38 * zoom},
                          {1.0f, 0.87f, 0.44f, 1, 8.0f});
    }

    void Player(Point2 p, const ActorPose& pose)
    {
        CharacterArt::Human(renderer, p, zoom, 0, levelOne->Player().level, pose);
    }

    void DrawNpc(Point2 p, const Npc& npc, const ActorPose& pose)
    {
        CharacterArt::Human(renderer, p, zoom, npc.style == 0 ? 1 : 2, 1, pose);
    }

    bool Speaking()
    {
        return transactionDialogue || speakingNpc >= 0 || speakingSoul >= 0;
    }

    struct TalkTarget
    {
        int resident = -1;
        int soul = -1;
        std::wstring name;
        std::wstring role;
        double x = 0.0, y = 0.0;

        bool Valid() const
        {
            return resident >= 0 || soul >= 0;
        }
    };

    TalkTarget GetTalkTarget(bool active)
    {
        if (active && transactionDialogue)
        {
            return {-1, -1, L"마을 거래", L"거래 결과", playerX, playerY};
        }
        TalkTarget result;
        int resident = active ? speakingNpc : npcs->Nearest(playerX, playerY, *world);
        int soul = active ? speakingSoul : levelOne->NearestSoul(playerX, playerY, *world);
        if (resident >= 0 && soul >= 0)
        {
            const auto& npc = npcs->Residents()[resident];
            const auto& ghost = levelOne->Souls()[soul];
            if (std::hypot(npc.x - playerX, npc.y - playerY) <
                std::hypot(ghost.position.x - playerX, ghost.position.y - playerY))
            {
                soul = -1;
            }
            else
            {
                resident = -1;
            }
        }
        if (resident >= 0)
        {
            const auto& npc = npcs->Residents()[resident];
            result = {resident, -1, npc.name, npc.role, npc.x, npc.y};
        }
        else if (soul >= 0)
        {
            const auto& npc = levelOne->Souls()[soul];
            result = {-1, soul, npc.Name(), npc.Role(), npc.position.x, npc.position.y};
        }
        return result;
    }

    void DialogueHUD()
    {
        if (showHelp && !Speaking())
        {
            return;
        }
        if (!Speaking())
        {
            TalkTarget npc = GetTalkTarget(false);
            if (npc.Valid())
            {
                Point2 p = Project(npc.x, npc.y);
                renderer.Rect(p.x - 44, p.y - 72 * zoom, 88, 26, {0.03f, 0.055f, 0.06f, 0.90f});
                renderer.KoreanText(p.x - 35, p.y - 71 * zoom, npc.name, ivory, 18, 80);
                std::wstring prompt = L"F  " + npc.role + L" " + npc.name + L"와 대화";
                float panelWidth = (std::min)(440.0f, float(width) - 40);
                float left = (width - panelWidth) * 0.5f;
                renderer.Rect(
                    left, float(height) - 157, panelWidth, 34, {0.04f, 0.065f, 0.07f, 0.94f});
                renderer.KoreanText(
                    left + 14, float(height) - 153, prompt, gold, 18, int(panelWidth) - 28);
            }
            return;
        }
        TalkTarget npc = GetTalkTarget(true);
        float panelWidth = (std::max)(160.0f, (std::min)(900.0f, float(width) - 48));
        int fontSize = width >= 640 ? 21 : 16;
        int textWidth = int(panelWidth) - 40;
        std::wstring title = npc.name + L" · " + npc.role;
        std::wstring controls = (dialoguePage + 1 < dialoguePages.size())
                                    ? L"F / Enter / Space  다음 대사    Esc  대화 닫기"
                                    : L"F / Enter / Space  대화 마치기    Esc  닫기";
        float titleHeight = renderer.KoreanTextHeight(title, 22, textWidth);
        float bodyHeight =
            renderer.KoreanTextHeight(dialoguePages[dialoguePage], fontSize, textWidth);
        float controlsHeight = renderer.KoreanTextHeight(controls, 16, textWidth);
        float panelHeight = titleHeight + bodyHeight + controlsHeight + 64;
        float left = (width - panelWidth) * 0.5f;
        float top = (std::max)(8.0f, float(height) - panelHeight - 78);
        renderer.Rect(left - 2, top - 2, panelWidth + 4, panelHeight + 4, {0.47f, 0.40f, 0.25f, 1});
        renderer.Rect(left, top, panelWidth, panelHeight, {0.035f, 0.055f, 0.065f, 0.98f});
        renderer.Rect(left, top, 4, panelHeight, gold);
        renderer.KoreanText(left + 20, top + 12, title, gold, 22, textWidth);
        renderer.KoreanText(left + 20,
                            top + 24 + titleHeight,
                            dialoguePages[dialoguePage],
                            ivory,
                            fontSize,
                            textWidth);
        renderer.KoreanText(
            left + 20, top + 36 + titleHeight + bodyHeight, controls, muted, 16, textWidth);
        renderer.Text(left + 20,
                      top + panelHeight - 18,
                      std::to_string(dialoguePage + 1) + " / " +
                          std::to_string(dialoguePages.size()),
                      muted,
                      1.2f);
    }

    void HUD()
    {
        if (width >= 900 && !showDiagnostics)
        {
            renderer.KoreanText(350, 136, levelOne->Objective(), gold, 15, width - 610);
        }
        if (width >= 900)
        {
            int village = VillageAt(levelOne->Player().position);
            renderer.KoreanText(350,
                                24,
                                village >= 0 ? Villages()[village].name : L"잿빛 들판",
                                gold,
                                22,
                                width - 600);
        }
        if (showDiagnostics && width >= 900)
        {
            renderer.Rect(340, 136, float(width) - 570, 90, {0.025f, 0.035f, 0.04f, 0.95f});
            const auto& effects = renderer.Effects();
            std::ostringstream effectStatus;
            effectStatus << (renderer.PostProcessingAvailable()
                                 ? (effects.enabled ? "HDR ON" : "POST OFF")
                                 : "POST UNAVAILABLE")
                         << "   BLOOM " << (effects.bloom ? "ON" : "OFF") << "   VIGNETTE "
                         << (effects.vignette ? "ON" : "OFF") << "   BLUR "
                         << (effects.edgeBlur ? "ON" : "OFF") << "   EXP " << std::fixed
                         << std::setprecision(1) << effects.exposure;
            renderer.Text(350, 146, effectStatus.str(), muted, 0.8f);
            std::ostringstream status;
            status << std::fixed << std::setprecision(1) << "X " << playerX << "  Y " << playerY
                   << "   CHUNKS " << world->Chunks().size() << "   QUEUED " << world->Pending()
                   << "   ACTORS " << levelOne->Scene().Size() << "   MESH CACHE "
                   << renderer.MeshCount() << "   SEED " << world->Seed();
            renderer.Text(350, 166, status.str(), muted, 0.65f);
            renderer.Text(350, 188, "F1 CHUNKS  F2 POST  F3 BLOOM  F4 VIGNETTE", muted, 1);
            renderer.Text(350, 207, "F5 BLUR  F6 RESET FX  F8 CLOSE  F9 RESET SAVE", muted, 1);
        }
        WorldView::MiniMap(renderer, *world, *levelOne, *npcs, width, height, renderDelta);
        float barLeft = (std::max)(12.0f, (float(width) - 620) * 0.5f);
        float barWidth = (std::min)(620.0f, float(width) - 24);
        renderer.Rect(barLeft, float(height) - 84, barWidth, 72, {0.035f, 0.045f, 0.045f, 0.94f});
        renderer.Rect(barLeft, float(height) - 84, barWidth, 1, gold);
        const wchar_t* actions[] = {
            L"T  거래", L"Y  성장", L"G  습득", L"F  대화", L"P  정지", L"H  도움"};
        for (int i = 0; i < 6; ++i)
        {
            float x = barLeft + 10 + i * (barWidth - 20) / 6;
            renderer.Rect(
                x, float(height) - 73, (barWidth - 20) / 6 - 5, 30, {0.12f, 0.13f, 0.115f, 1});
            renderer.KoreanText(
                x + 8, float(height) - 68, actions[i], ivory, 14, int((barWidth - 20) / 6 - 12));
        }
        renderer.KoreanText(barLeft + 16,
                            float(height) - 34,
                            L"WASD 이동 · Tab 가방 · Q 의뢰 · T 상점 · Ctrl+클릭 공격",
                            muted,
                            12,
                            int(barWidth) - 32);
        if (showHelp)
        {
            renderer.Rect(
                barLeft, float(height) - 290, barWidth, 190, {0.025f, 0.04f, 0.045f, 0.97f});
            renderer.KoreanText(
                barLeft + 16,
                float(height) - 278,
                L"여행자의 안내\n주민은 경험에 따라 성향이 변합니다. 클릭해서 살펴보세요.\n"
                L"Ctrl+클릭하면 중립 주민도 공격합니다. 목격자에게 주의하세요.\n"
                L"마을 주민 근처 T: 상점 · 가방에서 장비 착용/판매 · M 재료 판매\n"
                L"Y: 경험치 구매 · G: 소지품 습득 · F: 안내 주민과 대화\n"
                L"Tab: 인벤토리 · Q: 퀘스트 보상 · 마을 보스 권장 레벨 10\n"
                L"F8: 개발 정보 · F9: 시드 유지 초기화 · H: 안내 닫기",
                ivory,
                15,
                int(barWidth) - 32);
        }
        LevelOneView::HUD(
            renderer, *levelOne, width, height, paused, Speaking() || showHelp, Project);
        if (society && !Speaking() && !showHelp)
        {
            SocietyView::Panel(renderer, *society, width, height);
        }
        if (!world->Error().empty() || !npcs->Error().empty())
        {
            renderer.Rect(20, 105, float(width) - 200, 56, {0.28f, 0.08f, 0.07f, 0.94f});
            renderer.Text(32,
                          118,
                          !world->Error().empty() ? "WORLD STORAGE ERROR"
                                                  : "NPC MEMORY STORAGE ERROR",
                          gold,
                          2);
            renderer.Text(32, 141, "EXISTING DATA PRESERVED - SEE CONSOLE", ivory, 1.3f);
        }
        else if (!Speaking() && !showHelp && elapsed < messageUntil)
        {
            float panelWidth = (std::min)(float(width) - 40, float(message.size()) * 9 + 32);
            renderer.Rect((width - panelWidth) * 0.5f,
                          float(height) - 115,
                          panelWidth,
                          34,
                          {0.05f, 0.075f, 0.075f, 0.92f});
            renderer.Text(
                (width - panelWidth) * 0.5f + 16, float(height) - 104, message, gold, 1.5f);
        }
        DialogueHUD();
        if (society && (rpgPanel.IsOpen() || (!Speaking() && !showHelp)))
        {
            rpgPanel.Draw(renderer, *levelOne, *society, width, height);
        }
    }

    bool VisibleActor(const Actor& actor)
    {
        auto position = actor.Position();
        Point2 p = Project(position.x, position.y);
        return p.x > -160 && p.x < width + 160 && p.y > -40 && p.y < height + 300;
    }

    const Npc* Resident(std::uint64_t id)
    {
        for (const Npc& npc : npcs->Residents())
        {
            if (npc.id == id)
            {
                return &npc;
            }
        }
        return nullptr;
    }

    std::string TerrainMeshKey(ChunkKey key, const Chunk& chunk)
    {
        // Include actual geometry inputs: editing/regenerating a chunk invalidates its cache.
        std::uint64_t signature = 14695981039346656037ull;
        for (const auto& tile : chunk.tiles)
        {
            for (auto value : {int(tile.ground), int(tile.prop), tile.variation})
            {
                signature = (signature ^ std::uint64_t(value)) * 1099511628211ull;
            }
        }
        return "terrain:" + std::to_string(key.first) + ":" + std::to_string(key.second) + ":" +
               std::to_string(chunk.generationVersion) + ":" + std::to_string(signature);
    }

    void ConfigureActorRenderers()
    {
        auto& societyScene = levelOne->Scene();
        societyScene.SetRenderer(
            "citizen",
            [](Actor& actor)
            {
                if (society)
                {
                    if (const auto* npc = society->Find(actor.recordId))
                    {
                        if (!VisibleActor(actor))
                        {
                            return;
                        }
                        bool combat =
                            npc->action == NpcAction::Hunt || npc->action == NpcAction::PK;
                        SocietyView::CitizenBody(
                            renderer,
                            *npc,
                            Project(npc->position.x, npc->position.y),
                            zoom,
                            society->Selected() == npc->id,
                            actor.Animate(animationClock, combat ? npc->cooldown : 0, npc->health));
                    }
                }
            });
        societyScene.SetRenderer("citizen_loot",
                                 [](Actor& actor)
                                 {
                                     auto position = actor.Position();
                                     auto p = Project(position.x, position.y);
                                     renderer.Ellipse(p, 7 * zoom, 5 * zoom, gold);
                                 });
        societyScene.SetRenderer("social_strikes",
                                 [](Actor&)
                                 {
                                     if (!society)
                                     {
                                         return;
                                     }
                                     for (const auto& strike : society->Strikes())
                                     {
                                         renderer.Line(Project(strike.from.x, strike.from.y, 18),
                                                       Project(strike.to.x, strike.to.y, 18),
                                                       2 * zoom,
                                                       {1, 0.6f, 0.25f, 0.8f});
                                     }
                                 });
        auto& scene = levelOne->Scene();
        scene.SetRenderer(
            "terrain",
            [](Actor& base)
            {
                auto& actor = static_cast<TerrainActor&>(base);
                auto key = actor.Source();
                auto found = world->Chunks().find(key);
                if (found == world->Chunks().end())
                {
                    return;
                }
                WorldInt bx = key.first * ChunkSize, by = key.second * ChunkSize;
                auto position = actor.Position();
                Point2 origin = Project(position.x, position.y);
                std::string meshKey = TerrainMeshKey(key, found->second);
                if (renderer.BeginMesh(meshKey, Project(double(bx), double(by)), zoom))
                {
                    for (int y = 0; y < ChunkSize; ++y)
                    {
                        for (int x = 0; x < ChunkSize; ++x)
                        {
                            GroundTile(
                                bx + x, by + y, found->second.tiles[y * ChunkSize + x], true);
                        }
                    }
                    renderer.EndMesh();
                }
                renderer.DrawMesh(meshKey, origin, zoom, actor.opacity);
                for (int y = 0; y < ChunkSize; ++y)
                {
                    for (int x = 0; x < ChunkSize; ++x)
                    {
                        const Tile& tile = found->second.tiles[y * ChunkSize + x];
                        if (tile.ground != Ground::Water)
                        {
                            continue;
                        }
                        Point2 p = Project(position.x + x + 0.5, position.y + y + 0.5);
                        if (p.x < -80 || p.x > width + 80 || p.y < -80 || p.y > height + 80)
                        {
                            continue;
                        }
                        float drift = float(std::sin(elapsed * 1.2 + tile.variation)) * 3 * zoom;
                        renderer.Line({p.x - 10 * zoom + drift, p.y},
                                      {p.x + 8 * zoom + drift, p.y},
                                      zoom,
                                      {0.32f, 0.47f, 0.48f, 0.35f});
                    }
                }
                if (showChunks)
                {
                    Point2 a = origin, b = Project(position.x + ChunkSize, position.y),
                           c = Project(position.x + ChunkSize, position.y + ChunkSize),
                           d = Project(position.x, position.y + ChunkSize);
                    for (auto edge : {std::pair<Point2, Point2>{a, b}, {b, c}, {c, d}, {d, a}})
                    {
                        renderer.Line(edge.first, edge.second, 1.4f, {0.65f, 0.80f, 0.52f, 0.65f});
                    }
                }
            });
        scene.SetRenderer(
            "prop",
            [](Actor& base)
            {
                auto& actor = static_cast<PropActor&>(base);
                auto source = actor.Source();
                const Tile* tile = world->Find(source.first, source.second);
                if (!tile || !VisibleActor(actor))
                {
                    return;
                }
                auto position = actor.Position();
                Point2 p = Project(position.x, position.y);
                std::string meshKey = "prop:rpg3:" + std::to_string(int(tile->prop)) + ":" +
                                      std::to_string(tile->variation);
                if (renderer.BeginMesh(meshKey, p, zoom))
                {
                    switch (tile->prop)
                    {
                        case Prop::Tree:
                            Tree(p, tile->variation);
                            break;
                        case Prop::Rock:
                            renderer.Ellipse(p, 20 * zoom, 7 * zoom, {0.01f, 0.03f, 0.03f, 0.3f});
                            Box(p, 14, 7, 10 + float(tile->variation), {0.38f, 0.42f, 0.38f, 1});
                            break;
                        case Prop::Ruin:
                            Ruin(p);
                            break;
                        case Prop::Beacon:
                            BeaconBase(p);
                            break;
                        case Prop::Cottage:
                        case Prop::Chapel:
                        case Prop::Watchtower:
                        case Prop::Well:
                        case Prop::Graves:
                        case Prop::WallX:
                        case Prop::WallY:
                        case Prop::WallTower:
                            WorldView::Structure(renderer, p, zoom, tile->prop, tile->variation);
                            break;
                        default:
                            break;
                    }
                    renderer.EndMesh();
                }
                Point2 body = Project(playerX, playerY, 20);
                Point2 head = Project(playerX, playerY, 49);
                bool inFront =
                    position.x + position.y > playerX + playerY ||
                    (position.x + position.y == playerX + playerY && position.x >= playerX);
                bool covers = inFront && (renderer.MeshCovers(meshKey, p, zoom, body) ||
                                          renderer.MeshCovers(meshKey, p, zoom, head));
                float& opacity = actor.opacity;
                float desired = covers ? 0.25f : 1.0f;
                opacity += (desired - opacity) * float(1.0 - std::exp(-renderDelta * 12.0));
                renderer.DrawMesh(meshKey, p, zoom, opacity);
                if (tile->prop == Prop::Beacon && tile->lit)
                {
                    BeaconFlame(p);
                }
            });
        scene.SetRenderer("player",
                          [](Actor& actor)
                          {
                              auto p = actor.Position();
                              const auto& player = levelOne->Player();
                              Player(
                                  Project(p.x, p.y),
                                  actor.Animate(animationClock,
                                                (1 - levelOne->ShotReady()) * player.ShotCooldown(),
                                                player.health));
                          });
        scene.SetRenderer("npc",
                          [](Actor& actor)
                          {
                              if (const auto* npc = Resident(actor.recordId))
                              {
                                  if (VisibleActor(actor) && npcs->IsPresent(*npc, *world))
                                  {
                                      auto p = actor.Position();
                                      DrawNpc(
                                          Project(p.x, p.y), *npc, actor.Animate(animationClock));
                                  }
                              }
                          });
        scene.SetRenderer("enemy",
                          [](Actor& actor)
                          {
                              if (const auto* enemy = levelOne->FindEnemy(actor.recordId))
                              {
                                  if (VisibleActor(actor))
                                  {
                                      auto p = actor.Position();
                                      LevelOneView::DrawEnemy(renderer,
                                                              *enemy,
                                                              Project(p.x, p.y),
                                                              zoom,
                                                              enemy->id == levelOne->Target(),
                                                              actor.Animate(animationClock,
                                                                            enemy->attackTimer,
                                                                            enemy->health));
                                  }
                              }
                          });
        scene.SetRenderer("drop",
                          [](Actor& actor)
                          {
                              if (const auto* drop = levelOne->FindDrop(actor.recordId))
                              {
                                  if (VisibleActor(actor))
                                  {
                                      auto p = actor.Position();
                                      LevelOneView::DrawDrop(
                                          renderer, *drop, Project(p.x, p.y), zoom);
                                  }
                              }
                          });
        scene.SetRenderer("projectile",
                          [](Actor& actor)
                          {
                              if (const auto* shot = levelOne->FindProjectile(actor.recordId))
                              {
                                  if (VisibleActor(actor))
                                  {
                                      auto p = actor.Position();
                                      LevelOneView::DrawShot(
                                          renderer, *shot, Project(p.x, p.y), zoom);
                                  }
                              }
                          });
        scene.SetRenderer("soul",
                          [](Actor& actor)
                          {
                              if (const auto* soul = levelOne->FindSoul(actor.recordId))
                              {
                                  auto p = actor.Position();
                                  if (VisibleActor(actor) && world->Find(WorldInt(std::floor(p.x)),
                                                                         WorldInt(std::floor(p.y))))
                                  {
                                      LevelOneView::DrawSoul(
                                          renderer, *soul, Project(p.x, p.y), zoom, animationClock);
                                  }
                              }
                          });
        scene.SetRenderer("number",
                          [](Actor& actor)
                          {
                              if (const auto* number = levelOne->FindNumber(actor.recordId))
                              {
                                  LevelOneView::DrawNumber(renderer, *number, Project);
                              }
                          });
        scene.SetRenderer("overlay",
                          [](Actor&)
                          {
                              LevelOneView::Ground(
                                  renderer, *levelOne, Project, zoom, showAttackRange);
                          });
        scene.SetRenderer("hud",
                          [](Actor&)
                          {
                              HUD();
                          });
        scene.SetRenderer(
            "beacon-glow",
            [](Actor& actor)
            {
                auto source = actor.Position();
                const Tile* tile =
                    world->Find(WorldInt(std::floor(source.x)), WorldInt(std::floor(source.y)));
                if (tile && tile->prop == Prop::Beacon && tile->lit && VisibleActor(actor))
                {
                    Point2 p = Project(source.x, source.y);
                    for (int i = 5; i >= 1; --i)
                    {
                        renderer.Ellipse(p,
                                         (25 + i * 10) * zoom,
                                         (12 + i * 5) * zoom,
                                         {0.91f, 0.59f, 0.19f, 0.035f});
                    }
                }
            });
    }

    void SynchronizeScene()
    {
        levelOne->SynchronizeActors();
        if (society)
        {
            society->SynchronizeActors(levelOne->Scene(), *world);
        }
        auto& scene = levelOne->Scene();
        Actor& root = scene.Ensure("world", "group");
        std::set<std::string> terrainKeys, propKeys, meshKeys, npcKeys;
        for (const auto& entry : world->Chunks())
        {
            auto key = entry.first;
            WorldInt bx = key.first * ChunkSize, by = key.second * ChunkSize;
            std::string terrainKey =
                "terrain/" + std::to_string(key.first) + "/" + std::to_string(key.second);
            terrainKeys.insert(terrainKey);
            meshKeys.insert(TerrainMeshKey(key, entry.second));
            Actor* terrain = scene.Find(terrainKey);
            if (!terrain)
            {
                terrain = &scene.Add(std::make_unique<TerrainActor>(key), root.GetId());
                scene.SetPosition(terrain->GetId(), {double(bx), double(by)});
            }
            for (int y = 0; y < ChunkSize; ++y)
            {
                for (int x = 0; x < ChunkSize; ++x)
                {
                    const auto& tile = entry.second.tiles[y * ChunkSize + x];
                    if (tile.prop == Prop::None)
                    {
                        continue;
                    }
                    std::string propKey =
                        "prop/" + std::to_string(bx + x) + "/" + std::to_string(by + y);
                    propKeys.insert(propKey);
                    if (!scene.Find(propKey))
                    {
                        Actor& prop = scene.Add(std::make_unique<PropActor>(bx + x, by + y),
                                                terrain->GetId());
                        scene.SetPosition(prop.GetId(), {bx + x + 0.5, by + y + 0.5});
                        if (tile.prop == Prop::Beacon)
                        {
                            auto& glow =
                                scene.Ensure(propKey + "/glow", "beacon-glow", prop.GetId());
                            glow.layer = RenderLayer::GroundOverlay;
                            scene.SetPosition(glow.GetId(), prop.Position());
                        }
                    }
                    if (tile.prop == Prop::Beacon)
                    {
                        propKeys.insert(propKey + "/glow");
                    }
                }
            }
        }
        scene.Retain("terrain/", terrainKeys);
        scene.Retain("prop/", propKeys);
        renderer.RetainTerrainMeshes(meshKeys);
        auto& residents = scene.Ensure("residents", "group");
        for (const auto& npc : npcs->Residents())
        {
            auto id = npc.id;
            auto key = "npc/" + std::to_string(id);
            npcKeys.insert(key);
            if (!scene.Find(key))
            {
                Actor& actor = scene.Ensure(key, "npc", residents.GetId());
                actor.recordId = id;
                actor.BindPosition(
                    [id]()
                    {
                        const auto* npc = Resident(id);
                        return npc ? ActorPosition{npc->x, npc->y} : ActorPosition{};
                    },
                    [id](ActorPosition p)
                    {
                        npcs->SetPosition(id, p.x, p.y);
                    });
            }
        }
        scene.Retain("npc/", npcKeys);
        scene.Ensure("overlay", "overlay").layer = RenderLayer::GroundOverlay;
        scene.Ensure("hud", "hud").layer = RenderLayer::UI;
    }

    void RenderScene()
    {
        if (!running || !world || !levelOne)
        {
            return;
        }
        auto& profile = FrameProfiler::Instance();
        profile.BeginFrame();
        {
            FrameProfiler::Scope timer(FrameProfiler::SyncMs);
            SynchronizeScene();
        }
        auto& scene = levelOne->Scene();
        {
            FrameProfiler::Scope timer(FrameProfiler::SubmitMs);
            renderer.Begin(width, height);
            profile.SetPass(FrameProfiler::GroundDraws);
            scene.Render(RenderLayer::Ground);
            renderer.Flush();
            profile.SetPass(FrameProfiler::OverlayDraws);
            scene.Render(RenderLayer::GroundOverlay);
            renderer.Flush();
            profile.SetPass(FrameProfiler::WorldDraws);
            scene.Render(RenderLayer::World, VisibleActor);
            renderer.Flush();
            profile.SetPass(FrameProfiler::EffectsDraws);
            scene.Render(RenderLayer::Effects);
            renderer.FinishWorld();
            profile.SetPass(FrameProfiler::UiDraws);
            scene.Render(RenderLayer::UI);
            renderer.Flush();
        }
        {
            FrameProfiler::Scope timer(FrameProfiler::SwapMs);
            glutSwapBuffers();
        }
        renderer.ReportMetrics();
        profile.Set(FrameProfiler::Actors, double(scene.Size()));
        profile.Set(FrameProfiler::Chunks, double(world->Chunks().size()));
        profile.Set(FrameProfiler::PendingChunks, world->Pending());
        profile.Set(FrameProfiler::Width, width);
        profile.Set(FrameProfiler::Height, height);
        profile.Set(FrameProfiler::Zoom, zoom);
        profile.Set(FrameProfiler::PostEnabled,
                    renderer.Effects().enabled && renderer.PostProcessingAvailable());
        profile.Set(FrameProfiler::BloomEnabled, renderer.Effects().bloom);
        profile.Set(FrameProfiler::EdgeBlurEnabled, renderer.Effects().edgeBlur);
        profile.Set(FrameProfiler::VignetteEnabled, renderer.Effects().vignette);
        profile.EndFrame();
    }

    void ClearInput()
    {
        std::fill(std::begin(keys), std::end(keys), false);
        std::fill(std::begin(arrows), std::end(arrows), false);
    }

    void Tick(int)
    {
        if (!running)
        {
            return;
        }
        auto now = std::chrono::steady_clock::now();
        double dt = (std::min)(std::chrono::duration<double>(now - lastTick).count(), 0.05);
        lastTick = now;
        renderDelta = dt;
        elapsed += dt;
        DWORD foregroundProcess = 0;
        GetWindowThreadProcessId(GetForegroundWindow(), &foregroundProcess);
        if (foregroundProcess != GetCurrentProcessId())
        {
            ClearInput();
        }
        int radius =
            2 + int(std::ceil((width / (144.0 * zoom) + height / (72.0 * zoom)) / ChunkSize));
        radius = (std::max)(2, (std::min)(radius, 10));
        world->Stream(playerX, playerY, radius);
        static std::string reported;
        std::string errors = world->Error() + " " + npcs->Error() + " " + levelOne->Error();
        if (errors != reported)
        {
            reported = errors;
            if (reported != "  ")
            {
                std::cerr << reported << '\n';
            }
        }
        double horizontal =
            (keys['d'] || arrows[1] ? 1.0 : 0.0) - (keys['a'] || arrows[0] ? 1.0 : 0.0);
        double vertical =
            (keys['s'] || arrows[3] ? 1.0 : 0.0) - (keys['w'] || arrows[2] ? 1.0 : 0.0);
        double length = std::sqrt(horizontal * horizontal + vertical * vertical);
        moving = false;
        bool simulate = !paused && !rpgPanel.IsOpen() && world->Error().empty() &&
                        npcs->Error().empty() && levelOne->Error().empty() &&
                        (!society || society->Error().empty());
        if (length > 0 && simulate && !Speaking())
        {
            double speed = levelOne->Player().MoveSpeed();
            double dx = (horizontal + vertical) / length * 0.70710678 * speed * dt;
            double dy = (vertical - horizontal) / length * 0.70710678 * speed * dt;
            moving = levelOne->Move(dx, dy, *world, *npcs);
        }
        if (simulate)
        {
            animationClock += dt;
            if (society)
            {
                society->Update(dt, *world, *levelOne, *npcs);
            }
            levelOne->Update(dt, *world, *npcs);
        }
        if (monitor && society)
        {
            monitor->Publish(*world,
                             *society,
                             *levelOne,
                             *npcs,
                             true,
                             paused || rpgPanel.IsOpen(),
                             foregroundProcess == GetCurrentProcessId());
        }
        WorldPoint position = levelOne->Player().position;
        if (std::hypot(playerX - position.x, playerY - position.y) > 8.0)
        {
            cameraX = position.x;
            cameraY = position.y;
        }
        playerX = position.x;
        playerY = position.y;
        if (moving)
        {
            stepPhase += dt * 11;
        }
        double follow = 1 - std::exp(-dt * 10);
        cameraX += (playerX - cameraX) * follow;
        cameraY += (playerY - cameraY) * follow;
        glutPostRedisplay();
        glutTimerFunc(16, Tick, 0);
    }

    void Close()
    {
        if (!running)
        {
            return;
        }
        running = false;
        if (society && !society->Save())
        {
            std::cerr << society->Error() << '\n';
        }
        if (levelOne && !levelOne->Save())
        {
            std::cerr << levelOne->Error() << '\n';
        }
        if (monitor && society)
        {
            monitor->Publish(*world, *society, *levelOne, *npcs, false, paused, false, true);
            monitor.reset();
        }
        renderer.Shutdown();
    }

    void KeyDown(unsigned char key, int, int)
    {
        if (key >= 'A' && key <= 'Z')
        {
            key += 'a' - 'A';
        }
        if (keys[key])
        {
            return;
        }
        keys[key] = true;
        if (rpgPanel.IsOpen() && society)
        {
            rpgPanel.Key(key, *levelOne, *society);
            ClearInput();
            return;
        }
        if ((key == '\t' || key == 'q') && !Speaking())
        {
            rpgPanel.Open(key == 'q' ? 3 : 1);
            showHelp = false;
            ClearInput();
            return;
        }
        // Exposure can also be adjusted while comparing effects behind a dialogue window.
        if (key == '[' || key == ']')
        {
            auto& effects = renderer.Effects();
            effects.exposure =
                std::clamp(effects.exposure + (key == ']' ? 0.1f : -0.1f), 0.1f, 4.0f);
            return;
        }
        if (Speaking())
        {
            bool close = key == 27;
            if (key == 'f' || key == '\r' || key == ' ')
            {
                close = ++dialoguePage >= dialoguePages.size();
            }
            if (close)
            {
                speakingNpc = -1;
                transactionDialogue = false;
                speakingSoul = -1;
                dialoguePages.clear();
                ClearInput();
            }
            return;
        }
        if (key == 27)
        {
            glutLeaveMainLoop();
            return;
        }
        if (key == 'p')
        {
            paused = !paused;
            ClearInput();
            return;
        }
        if (key == 'r')
        {
            showAttackRange = !showAttackRange;
            return;
        }
        if (paused)
        {
            return;
        }
        if (society && (key == 't' || key == 'y' || key == 'g'))
        {
            if (!society->Error().empty() || !levelOne->Error().empty())
            {
                return;
            }
            if (key == 't')
            {
                if (const auto* trader = society->NearbyTrader(levelOne->Player().position))
                {
                    rpgPanel.Open(2, trader->id);
                    ClearInput();
                    return;
                }
            }
            std::wstring text = key == 'g' ? society->LootPlayer(levelOne->Player().position)
                                           : society->TradePlayer(*levelOne, key == 'y');
            std::wcout << text << L'\n';
            society->Save();
            levelOne->Save();
            dialoguePages = {text};
            dialoguePage = 0;
            speakingNpc = speakingSoul = -1;
            transactionDialogue = true;
            ClearInput();
            return;
        }
        if (key == 'f')
        {
            TalkTarget target = GetTalkTarget(false);
            if (target.Valid())
            {
                dialoguePages = target.soul >= 0 ? levelOne->SoulConversation(target.soul)
                                                 : npcs->Conversation(target.resident, *world);
                if (!dialoguePages.empty())
                {
                    if (target.resident >= 0)
                    {
                        levelOne->RecordConversation();
                    }
                    speakingNpc = target.resident;
                    speakingSoul = target.soul;
                    dialoguePage = 0;
                    moving = false;
                    ClearInput();
                }
            }
            else
            {
                message = "NO ONE NEARBY - RETURN HOME TO FIND RESIDENTS";
                messageUntil = elapsed + 4;
            }
        }
        if (key == 'e')
        {
            message = world->LightNearest(playerX, playerY);
            messageUntil = elapsed + 5;
        }
        if (key == 'h')
        {
            showHelp = !showHelp;
        }
        if (key == '+' || key == '=')
        {
            zoom = (std::min)(1.8f, zoom + 0.1f);
        }
        if (key == '-')
        {
            zoom = (std::max)(0.65f, zoom - 0.1f);
        }
    }

    void KeyUp(unsigned char key, int, int)
    {
        if (key >= 'A' && key <= 'Z')
        {
            key += 'a' - 'A';
        }
        keys[key] = false;
    }

    int ArrowIndex(int key)
    {
        switch (key)
        {
            case GLUT_KEY_LEFT:
                return 0;
            case GLUT_KEY_RIGHT:
                return 1;
            case GLUT_KEY_UP:
                return 2;
            case GLUT_KEY_DOWN:
                return 3;
            default:
                return -1;
        }
    }

    void ResetPlayData()
    {
        if (!world || !levelOne || !npcs || gameplaySaveDirectory.empty())
        {
            return;
        }
        ClearInput();
        moving = false;
        const bool wasPaused = paused;
        paused = true;
        int answer = MessageBoxW(
            nullptr,
            L"플레이 진행을 초기화하고 거점에서 다시 시작할까요?\n\n"
            L"초기화: 플레이 성장, 적/아이템, 영혼 NPC, 대화 기록, 동적 NPC와 금화/재료\n"
            L"유지: 월드 시드, 생성된 지형과 건물, 봉화 점등 상태\n\n"
            L"기존 진행 파일은 Backups 폴더에 보관됩니다.",
            L"F9 · 테스트용 새 플레이",
            MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
        lastTick = std::chrono::steady_clock::now();
        if (answer != IDYES)
        {
            paused = wasPaused;
            return;
        }
        const wchar_t* files[] = {L"level_one.dat", L"residents_v1.dat", L"world_simulation.dat"};
        std::filesystem::path backup;
        bool originalsChanged = false;
        try
        {
            if (!world->Error().empty() || !npcs->Error().empty() || !levelOne->Save() ||
                (society && !society->Save()))
            {
                throw std::runtime_error("Resolve existing save errors before resetting.");
            }
            auto stamp = std::chrono::duration_cast<std::chrono::microseconds>(
                             std::chrono::system_clock::now().time_since_epoch())
                             .count();
            backup = gameplaySaveDirectory / L"Backups" / ("play_reset_" + std::to_string(stamp));
            std::filesystem::create_directories(backup);
            // Both backups must succeed before either active file is removed.
            for (const auto* file : files)
            {
                std::filesystem::copy_file(gameplaySaveDirectory / file, backup / file);
            }
            originalsChanged = true;
            for (const auto* file : files)
            {
                std::filesystem::remove(gameplaySaveDirectory / file);
            }
            auto freshNpcs = std::make_unique<NpcSystem>(gameplaySaveDirectory);
            auto freshLevel = std::make_unique<LevelOne>(gameplaySaveDirectory, world->Seed());
            auto freshSociety =
                std::make_unique<WorldSimulation>(gameplaySaveDirectory, world->Seed());
            if (!freshNpcs->Error().empty() || !freshLevel->Error().empty() ||
                !freshSociety->Error().empty())
            {
                throw std::runtime_error(freshNpcs->Error() + " " + freshLevel->Error() + " " +
                                         freshSociety->Error());
            }
            levelOne.swap(freshLevel);
            npcs.swap(freshNpcs);
            society.swap(freshSociety);
            levelOne->BindSimulation(society.get());
        }
        catch (const std::exception& error)
        {
            std::cerr << "[PlayReset] " << error.what() << '\n';
            if (originalsChanged)
            {
                for (const auto* file : files)
                {
                    std::error_code restoreError;
                    std::filesystem::copy_file(backup / file,
                                               gameplaySaveDirectory / file,
                                               std::filesystem::copy_options::overwrite_existing,
                                               restoreError);
                    if (restoreError)
                    {
                        std::cerr << "[PlayReset] Restore failed: " << restoreError.message()
                                  << '\n';
                    }
                }
            }
            paused = true;
            MessageBoxW(nullptr,
                        L"초기화에 실패했습니다. 콘솔과 Backups 폴더를 확인하세요.",
                        L"플레이 초기화 오류",
                        MB_OK | MB_ICONERROR);
            lastTick = std::chrono::steady_clock::now();
            return;
        }
        ConfigureActorRenderers();
        rpgPanel.Close();
        speakingNpc = speakingSoul = -1;
        transactionDialogue = false;
        dialoguePages.clear();
        dialoguePage = 0;
        playerX = cameraX = levelOne->Player().position.x;
        playerY = cameraY = levelOne->Player().position.y;
        elapsed = stepPhase = animationClock = 0.0;
        message = "NEW RUN - WORLD SEED PRESERVED";
        messageUntil = 6.0;
        paused = false;
        ClearInput();
        world->Stream(playerX, playerY, 2);
        lastTick = std::chrono::steady_clock::now();
        std::wcout << L"[PlayReset] Completed. Backup: " << backup.c_str() << L'\n';
        FrameProfiler::Instance().Event("play_reset", std::to_string(world->Seed()));
        glutPostRedisplay();
    }

    void SpecialDown(int key, int, int)
    {
        if (rpgPanel.IsOpen() && society)
        {
            if (key == GLUT_KEY_UP)
            {
                rpgPanel.Key('w', *levelOne, *society);
            }
            if (key == GLUT_KEY_DOWN)
            {
                rpgPanel.Key('s', *levelOne, *society);
            }
            if (key == GLUT_KEY_LEFT)
            {
                rpgPanel.Key('a', *levelOne, *society);
            }
            if (key == GLUT_KEY_RIGHT)
            {
                rpgPanel.Key('d', *levelOne, *society);
            }
            ClearInput();
            return;
        }
        if (key == GLUT_KEY_F8)
        {
            showDiagnostics = !showDiagnostics;
            return;
        }
        if (key == GLUT_KEY_F9)
        {
            ResetPlayData();
            return;
        }
        auto& effects = renderer.Effects();
        switch (key)
        {
            case GLUT_KEY_F2:
                effects.enabled = !effects.enabled;
                return;
            case GLUT_KEY_F3:
                effects.bloom = !effects.bloom;
                return;
            case GLUT_KEY_F4:
                effects.vignette = !effects.vignette;
                return;
            case GLUT_KEY_F5:
                effects.edgeBlur = !effects.edgeBlur;
                return;
            case GLUT_KEY_F6:
                effects = PostProcessSettings{};
                return;
            default:
                break;
        }
        if (Speaking())
        {
            return;
        }
        if (paused)
        {
            return;
        }
        int index = ArrowIndex(key);
        if (index >= 0)
        {
            arrows[index] = true;
        }
        if (key == GLUT_KEY_F1)
        {
            showChunks = !showChunks;
        }
        if (key == GLUT_KEY_HOME)
        {
            levelOne->ReturnToCamp();
            ClearInput();
        }
    }

    void SpecialUp(int key, int, int)
    {
        int index = ArrowIndex(key);
        if (index >= 0)
        {
            arrows[index] = false;
        }
    }

    void MouseButton(int button, int state, int x, int y)
    {
        if (rpgPanel.IsOpen() && society)
        {
            if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN)
            {
                rpgPanel.Click(x, y, width, height, *levelOne, *society);
            }
            return;
        }
        if (button == GLUT_LEFT_BUTTON && state == GLUT_DOWN && width >= 760 && height >= 600 &&
            society && !society->Find(society->Selected()) && x >= 20 && x <= 324 && y >= 166 &&
            y <= 272)
        {
            if (!Speaking() && !showHelp)
            {
                rpgPanel.Open(3);
            }
            ClearInput();
            return;
        }
        if (state != GLUT_DOWN || !society || paused || Speaking() || showHelp)
        {
            return;
        }
        if (button == GLUT_RIGHT_BUTTON)
        {
            society->Select(0);
            return;
        }
        bool societyVisible = width >= 700 && height >= 560;
        bool leftPanel =
            x >= 20 && x <= 324 &&
            ((y >= 20 && y <= 152) ||
             (societyVisible && society->Find(society->Selected()) && y >= 166 && y <= 376));
        bool newsPanel =
            societyVisible && x >= 20 && x <= 360 && y >= height - 176 && y <= height - 98;
        float mapSize =
            (std::min)(std::clamp(width * 0.20f, 124.0f, 184.0f), float(height) - 250.0f);
        bool mapPanel = width >= 600 && height >= 400 && x >= width - mapSize - 34 &&
                        x <= width - 14 && y >= 20 &&
                        y <= 54 + mapSize + (societyVisible ? 114 : 49);
        bool debugPanel =
            showDiagnostics && width >= 900 && x >= 340 && x <= width - 230 && y >= 136 && y <= 226;
        bool bossPanel = false;
        for (const auto& enemy : levelOne->Enemies())
        {
            if (enemy.kind == EnemyKind::Boss && enemy.health > 0)
            {
                bossPanel =
                    x >= 360 && x <= 360 + (std::min)(440, width - 540) && y >= 64 && y <= 125;
            }
        }
        if (button != GLUT_LEFT_BUTTON || y > height - 90 || leftPanel || newsPanel || mapPanel ||
            debugPanel || bossPanel)
        {
            return;
        }
        std::uint64_t selected = 0;
        double nearest = 30 * zoom;
        for (const auto& npc : society->People())
        {
            if (npc.health <= 0 || !world->Find(WorldInt(std::floor(npc.position.x)),
                                                WorldInt(std::floor(npc.position.y))))
            {
                continue;
            }
            auto point = Project(npc.position.x, npc.position.y, 27);
            double distance = std::hypot(double(point.x - x), double(point.y - y));
            if (distance < nearest)
            {
                nearest = distance;
                selected = npc.id;
            }
        }
        society->Select(selected, (glutGetModifiers() & GLUT_ACTIVE_CTRL) != 0);
    }

    void Wheel(int, int direction, int, int)
    {
        if (rpgPanel.IsOpen() && society)
        {
            rpgPanel.Key(direction > 0 ? 'a' : 'd', *levelOne, *society);
            return;
        }
        zoom = (std::max)(0.65f, (std::min)(1.8f, zoom + direction * 0.1f));
    }

    void Resize(int w, int h)
    {
        width = (std::max)(1, w);
        height = (std::max)(1, h);
        glutPostRedisplay();
    }
}

int main(int argc, char** argv)
{
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitContextVersion(3, 3);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitWindowSize(width, height);
    glutCreateWindow("GSE - The Remaining Light | Living World");
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_GLUTMAINLOOP_RETURNS);
    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK || !GLEW_VERSION_3_3)
    {
        std::cerr << "OpenGL 3.3 and successful GLEW initialization are required.\n";
        MessageBoxW(nullptr,
                    L"OpenGL 3.3 initialization failed. See console.",
                    L"GSE startup",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    // GLEW may leave an error while querying extensions in a core context.
    while (glGetError() != GL_NO_ERROR)
    {
    }
    if (!renderer.Initialize())
    {
        renderer.Shutdown();
        MessageBoxW(nullptr,
                    L"Renderer initialization failed. See console.",
                    L"GSE startup",
                    MB_OK | MB_ICONERROR);
        return 1;
    }
    wchar_t executable[32768] = {};
    DWORD length = GetModuleFileNameW(nullptr, executable, 32768);
    if (!length || length >= 32768)
    {
        std::cerr << "Cannot resolve save directory.\n";
        renderer.Shutdown();
        return 1;
    }
    bool naturalWorld = false;
    for (int i = 1; i < argc; ++i)
    {
        naturalWorld = naturalWorld || std::string(argv[i]) == "--natural-world";
    }
    auto savePath = std::filesystem::path(executable).parent_path() / L"SaveData" /
                    (naturalWorld ? L"Level01_natural_v2" : L"Level01_v1");
    gameplaySaveDirectory = savePath;
    world = std::make_unique<World>(savePath);
    if (!world->Error().empty())
    {
        std::cerr << world->Error() << '\n';
        MessageBoxW(nullptr,
                    L"월드 저장 정보를 불러오지 못했습니다. 콘솔을 확인하세요.",
                    L"레벨 1",
                    MB_OK | MB_ICONERROR);
        renderer.Shutdown();
        return 1;
    }
    npcs = std::make_unique<NpcSystem>(savePath);
    renderer.SetMeshCacheDirectory(savePath / L"MeshCache" / std::to_string(world->Seed()));
    FrameProfiler::Instance().OpenLog(
        std::filesystem::path(executable).parent_path() / L"Logs",
        "seed=" + std::to_string(world->Seed()) +
            (naturalWorld ? ";mode=Level01_natural_v2" : ";mode=Level01_v1") +
            ";terrain=v9;art=rig2;props=rpg3;rpg=1");
    levelOne = std::make_unique<LevelOne>(savePath, world->Seed());
    society = std::make_unique<WorldSimulation>(savePath, world->Seed());
    try
    {
        monitor = std::make_unique<WorldMonitor>(
            savePath, std::filesystem::path(executable).parent_path() / L"Monitor");
    }
    catch (const std::exception& e)
    {
        std::cerr << "[Monitor] Disabled: " << e.what() << '\n';
    }
    levelOne->BindSimulation(society.get());
    if (!society->Error().empty())
    {
        std::cerr << "[Simulation] " << society->Error() << '\n';
    }
    ConfigureActorRenderers();
    playerX = cameraX = levelOne->Player().position.x;
    playerY = cameraY = levelOne->Player().position.y;
    std::wcout << L"World save directory: " << savePath.c_str() << L'\n';
    std::cout
        << "WASD / arrows: move | F: talk | E: light beacon | wheel / +/-: zoom\n"
        << "F1: chunk edges | H: help | Home: origin | Esc: exit\n"
        << "Left click NPC: inspect | Ctrl+click: PK target | Right click: cancel | T: trade | Y: XP | G: loot\n"
        << "F8: diagnostics HUD (hidden by default)\n"
        << "Monitor: built-in server starts automatically; open the [Monitor HTTP] URL above.\n"
        << "Simulation continues in background. Press P in the game to pause safely.\n"
        << "F2: post on/off | F3: bloom | F4: vignette | F5: edge blur | F6: reset FX\n"
        << "F9: reset play data (confirmation + backup); keep world seed and terrain\n"
        << "[ / ]: exposure down/up (0.1 to 4.0); HDR is tone-mapped to SDR output.\n"
        << "Tab: inventory/equipment | Q: quests | T: nearby village shop\n"
        << "Village boss: first in 3 minutes, then 5 minutes after defeat (recommended level 10)\n";
    world->Stream(playerX, playerY, 2);
    glutDisplayFunc(RenderScene);
    glutReshapeFunc(Resize);
    glutKeyboardFunc(KeyDown);
    glutKeyboardUpFunc(KeyUp);
    glutSpecialFunc(SpecialDown);
    glutSpecialUpFunc(SpecialUp);
    glutMouseWheelFunc(Wheel);
    glutMouseFunc(MouseButton);
    glutCloseFunc(Close);
    glutIgnoreKeyRepeat(1);
    lastTick = std::chrono::steady_clock::now();
    glutTimerFunc(0, Tick, 0);
    glutMainLoop();
    // freeGLUT invokes Close before destroying the context; do not delete GL objects here.
    npcs.reset();
    levelOne.reset();
    world.reset();
    return 0;
}
