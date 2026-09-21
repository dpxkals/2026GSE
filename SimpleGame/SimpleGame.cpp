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

namespace
{
    SceneRenderer renderer;
    std::unique_ptr<World> world;
    std::unique_ptr<NpcSystem> npcs;
    std::unique_ptr<LevelOne> levelOne;
    int speakingNpc = -1;
    int speakingSoul = -1;
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
    double stepPhase = 0.0;
    float zoom = 1.0f;

    bool keys[256] = {};
    bool arrows[4] = {};

    bool showChunks = false;
    bool showHelp = true;
    bool moving = false;
    bool running = true;
    bool paused = false;
    bool showAttackRange = true;

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

    void Player(Point2 p)
    {
        float bob = moving ? float(std::sin(stepPhase)) * 1.5f * zoom : 0;
        renderer.Ellipse(p, 13 * zoom, 5 * zoom, {0.0f, 0.015f, 0.02f, 0.45f});
        renderer.Ellipse({p.x, p.y + zoom}, 17 * zoom, 7 * zoom, {0.63f, 0.68f, 0.55f, 0.12f});
        float stride = moving ? float(std::sin(stepPhase)) * 3 * zoom : 0;
        renderer.Rect(
            p.x - 6 * zoom, p.y - 6 * zoom + stride, 4 * zoom, 7 * zoom, {0.12f, 0.12f, 0.11f, 1});
        renderer.Rect(
            p.x + 2 * zoom, p.y - 6 * zoom - stride, 4 * zoom, 7 * zoom, {0.12f, 0.12f, 0.11f, 1});
        renderer.Triangle({p.x, p.y - 31 * zoom + bob},
                          {p.x - 12 * zoom, p.y - 5 * zoom + bob},
                          {p.x + 10 * zoom, p.y - 5 * zoom + bob},
                          {0.49f, 0.37f, 0.25f, 1});
        renderer.Triangle({p.x, p.y - 31 * zoom + bob},
                          {p.x, p.y - 5 * zoom + bob},
                          {p.x + 10 * zoom, p.y - 5 * zoom + bob},
                          {0.31f, 0.26f, 0.21f, 1});
        renderer.Ellipse(
            {p.x, p.y - 33 * zoom + bob}, 7 * zoom, 8 * zoom, {0.59f, 0.48f, 0.32f, 1});
        renderer.Ellipse(
            {p.x + 2 * zoom, p.y - 32 * zoom + bob}, 4 * zoom, 4 * zoom, {0.16f, 0.17f, 0.16f, 1});
        renderer.Line({p.x + 12 * zoom, p.y - 2 * zoom},
                      {p.x + 15 * zoom, p.y - 31 * zoom + bob},
                      2 * zoom,
                      gold);
        renderer.Ellipse({p.x + 15 * zoom, p.y - 30 * zoom + bob},
                         3 * zoom,
                         4 * zoom,
                         {0.99f, 0.78f, 0.38f, 1, 3.5f});
    }

    void DrawNpc(Point2 p, const Npc& npc)
    {
        float breath = float(std::sin(elapsed * 1.7 + npc.id)) * 0.8f * zoom;
        Color cloak =
            npc.style == 0 ? Color{0.30f, 0.43f, 0.36f, 1} : Color{0.39f, 0.32f, 0.49f, 1};
        renderer.Ellipse(p, 14 * zoom, 5 * zoom, {0.01f, 0.02f, 0.02f, 0.4f});
        renderer.Rect(p.x - 6 * zoom, p.y - 6 * zoom, 4 * zoom, 7 * zoom, {0.15f, 0.13f, 0.12f, 1});
        renderer.Rect(p.x + 2 * zoom, p.y - 6 * zoom, 4 * zoom, 7 * zoom, {0.15f, 0.13f, 0.12f, 1});
        renderer.Triangle({p.x, p.y - 33 * zoom + breath},
                          {p.x - 13 * zoom, p.y - 5 * zoom},
                          {p.x + 12 * zoom, p.y - 5 * zoom},
                          cloak);
        renderer.Triangle({p.x, p.y - 33 * zoom + breath},
                          {p.x, p.y - 5 * zoom},
                          {p.x + 12 * zoom, p.y - 5 * zoom},
                          Shade(cloak, 0.72f));
        renderer.Ellipse(
            {p.x, p.y - 35 * zoom + breath}, 7 * zoom, 8 * zoom, {0.64f, 0.54f, 0.42f, 1});
        renderer.Ellipse({p.x, p.y - 40 * zoom + breath}, 8 * zoom, 5 * zoom, Shade(cloak, 0.75f));
        if (npc.style == 0)
        {
            renderer.Line({p.x - 13 * zoom, p.y},
                          {p.x - 15 * zoom, p.y - 35 * zoom},
                          2 * zoom,
                          {0.49f, 0.37f, 0.23f, 1});
            renderer.Rect(p.x + 5 * zoom,
                          p.y - 20 * zoom + breath,
                          7 * zoom,
                          9 * zoom,
                          {0.73f, 0.49f, 0.23f, 1});
        }
        else
        {
            renderer.Rect(p.x - 4 * zoom,
                          p.y - 22 * zoom + breath,
                          15 * zoom,
                          11 * zoom,
                          {0.69f, 0.62f, 0.45f, 1});
            renderer.Line({p.x + 3 * zoom, p.y - 21 * zoom + breath},
                          {p.x + 3 * zoom, p.y - 12 * zoom + breath},
                          zoom,
                          {0.35f, 0.26f, 0.18f, 1});
        }
    }

    bool Speaking()
    {
        return speakingNpc >= 0 || speakingSoul >= 0;
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
        renderer.Rect(0, 0, float(width), 92, {0.035f, 0.05f, 0.06f, 0.94f});
        renderer.Rect(24, 22, 3, 45, gold);
        renderer.Text(40, 22, "GSE / LEVEL 01 - ASHEN FIELDS", ivory, 2);
        const auto& effects = renderer.Effects();
        std::ostringstream effectStatus;
        effectStatus << (renderer.PostProcessingAvailable()
                             ? (effects.enabled ? "HDR ON" : "POST OFF")
                             : "POST UNAVAILABLE")
                     << "   BLOOM " << (effects.bloom ? "ON" : "OFF") << "   VIGNETTE "
                     << (effects.vignette ? "ON" : "OFF") << "   BLUR "
                     << (effects.edgeBlur ? "ON" : "OFF") << "   EXP " << std::fixed
                     << std::setprecision(1) << effects.exposure;
        renderer.Text(40, 47, effectStatus.str(), muted, 1.3f);
        std::ostringstream status;
        status << std::fixed << std::setprecision(1) << "X " << playerX << "  Y " << playerY
               << "   CHUNKS " << world->Chunks().size() << "   QUEUED " << world->Pending()
               << "   ACTORS " << levelOne->Scene().Size() << "   MESH CACHE "
               << renderer.MeshCount() << "   SEED " << world->Seed();
        renderer.Text(40, 70, status.str(), muted, 1.3f);
        float mx = float(width) - 156, my = 109;
        renderer.Rect(mx - 10, my - 7, 142, 138, {0.025f, 0.045f, 0.05f, 0.88f});
        renderer.Text(mx, my, "STREAM MAP", muted, 1.5f);
        WorldInt cx = World::ChunkOf(WorldInt(std::floor(playerX)));
        WorldInt cy = World::ChunkOf(WorldInt(std::floor(playerY)));
        for (int y = -4; y <= 4; ++y)
        {
            for (int x = -4; x <= 4; ++x)
            {
                bool loaded = world->Chunks().find({cx + x, cy + y}) != world->Chunks().end();
                Color c = loaded ? Color{0.24f, 0.39f, 0.34f, 1} : Color{0.09f, 0.14f, 0.15f, 1};
                if (x == 0 && y == 0)
                {
                    c = gold;
                }
                renderer.Rect(mx + (x + 4) * 13, my + 24 + (y + 4) * 10, 11, 8, c);
            }
        }
        renderer.Rect(0, float(height) - 64, float(width), 64, {0.025f, 0.045f, 0.05f, 0.94f});
        if (showHelp)
        {
            renderer.Text(
                24,
                float(height) - 46,
                "WASD MOVE   AUTO FIRE   R RANGE   P PAUSE   F TALK   E BEACON   WHEEL ZOOM",
                ivory,
                1.5f);
            renderer.Text(
                24,
                float(height) - 25,
                "F1 CHUNKS  F2 POST  F3 BLOOM  F4 VIGNETTE  F5 BLUR  F6 RESET  H HELP  HOME ORIGIN  ESC EXIT",
                muted,
                1.2f);
        }
        else
        {
            renderer.Text(24, float(height) - 38, "H  SHOW CONTROLS", muted, 1.5f);
        }
        LevelOneView::HUD(renderer, *levelOne, width, height, paused, Speaking(), Project);
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
        else if (!Speaking() && elapsed < messageUntil)
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
    }

    bool VisibleActor(const Actor& actor)
    {
        auto position = actor.Position();
        Point2 p = Project(position.x, position.y);
        return p.x > -120 && p.x < width + 120 && p.y > -20 && p.y < height + 180;
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

    void ConfigureActorRenderers()
    {
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
                std::string meshKey =
                    "terrain:" + std::to_string(key.first) + ":" + std::to_string(key.second);
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
                std::string meshKey = "prop:" + std::to_string(int(tile->prop)) + ":" +
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
                        default:
                            break;
                    }
                    renderer.EndMesh();
                }
                Point2 body = Project(playerX, playerY, 20);
                Point2 head = Project(playerX, playerY, 36);
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
                              if (levelOne->InvulnerableTime() <= 0.0 || int(elapsed * 12) % 2 == 0)
                              {
                                  Player(Project(p.x, p.y));
                              }
                          });
        scene.SetRenderer("npc",
                          [](Actor& actor)
                          {
                              if (const auto* npc = Resident(actor.recordId))
                              {
                                  if (VisibleActor(actor) && npcs->IsPresent(*npc, *world))
                                  {
                                      auto p = actor.Position();
                                      DrawNpc(Project(p.x, p.y), *npc);
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
                                                              enemy->id == levelOne->Target());
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
        scene.SetRenderer(
            "soul",
            [](Actor& actor)
            {
                if (const auto* soul = levelOne->FindSoul(actor.recordId))
                {
                    auto p = actor.Position();
                    if (VisibleActor(actor) &&
                        world->Find(WorldInt(std::floor(p.x)), WorldInt(std::floor(p.y))))
                    {
                        LevelOneView::DrawSoul(renderer, *soul, Project(p.x, p.y), zoom, elapsed);
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
            meshKeys.insert("terrain:" + std::to_string(key.first) + ":" +
                            std::to_string(key.second));
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
        FrameProfiler::Instance().BeginFrame();
        SynchronizeScene();
        renderer.Begin(width, height);
        auto& scene = levelOne->Scene();
        scene.Render(RenderLayer::Ground);
        scene.Render(RenderLayer::GroundOverlay);
        scene.Render(RenderLayer::World, VisibleActor);
        scene.Render(RenderLayer::Effects);
        renderer.FinishWorld();
        scene.Render(RenderLayer::UI);
        renderer.Flush();
        glutSwapBuffers();
        FrameProfiler::Instance().EndFrame();
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
        bool simulate = !paused && !Speaking() && foregroundProcess == GetCurrentProcessId() &&
                        world->Error().empty() && npcs->Error().empty() &&
                        levelOne->Error().empty();
        if (length > 0 && simulate)
        {
            double speed = levelOne->Player().MoveSpeed();
            double dx = (horizontal + vertical) / length * 0.70710678 * speed * dt;
            double dy = (vertical - horizontal) / length * 0.70710678 * speed * dt;
            moving = levelOne->Move(dx, dy, *world, *npcs);
        }
        if (simulate)
        {
            levelOne->Update(dt, *world, *npcs);
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
        if (levelOne && !levelOne->Save())
        {
            std::cerr << levelOne->Error() << '\n';
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

    void SpecialDown(int key, int, int)
    {
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

    void Wheel(int, int direction, int, int)
    {
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
    glutCreateWindow("GSE - The Remaining Light | Level 01 - Ashen Fields");
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
    levelOne = std::make_unique<LevelOne>(savePath, world->Seed());
    ConfigureActorRenderers();
    playerX = cameraX = levelOne->Player().position.x;
    playerY = cameraY = levelOne->Player().position.y;
    std::wcout << L"World save directory: " << savePath.c_str() << L'\n';
    std::cout << "WASD / arrows: move | F: talk | E: light beacon | wheel / +/-: zoom\n"
              << "F1: chunk edges | H: help | Home: origin | Esc: exit\n"
              << "F2: post on/off | F3: bloom | F4: vignette | F5: edge blur | F6: reset FX\n"
              << "[ / ]: exposure down/up (0.1 to 4.0); HDR is tone-mapped to SDR output.\n"
              << "Level 01: auto fire within 6 tiles | R: range | P: pause | 24 kills: boss\n";
    world->Stream(playerX, playerY, 2);
    glutDisplayFunc(RenderScene);
    glutReshapeFunc(Resize);
    glutKeyboardFunc(KeyDown);
    glutKeyboardUpFunc(KeyUp);
    glutSpecialFunc(SpecialDown);
    glutSpecialUpFunc(SpecialUp);
    glutMouseWheelFunc(Wheel);
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
