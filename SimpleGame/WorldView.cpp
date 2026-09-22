#include "stdafx.h"
#include "WorldView.h"
#include "LevelOne.h"
#include "NpcSystem.h"
#include <algorithm>
#include <cmath>

namespace
{
    Color Shade(Color color, float amount)
    {
        return {color.r * amount, color.g * amount, color.b * amount, color.a, color.energy};
    }

    Color MapColor(const Tile& tile)
    {
        if (tile.prop == Prop::Tree)
        {
            return {0.12f, 0.27f, 0.16f, 1};
        }
        if (tile.prop == Prop::Beacon)
        {
            return tile.lit ? Color{1.0f, 0.80f, 0.30f, 1} : Color{0.60f, 0.46f, 0.20f, 1};
        }
        if (tile.prop >= Prop::Cottage)
        {
            return {0.66f, 0.49f, 0.34f, 1};
        }
        if (tile.prop == Prop::Ruin || tile.prop == Prop::Rock)
        {
            return {0.46f, 0.47f, 0.43f, 1};
        }
        switch (tile.ground)
        {
            case Ground::Dirt:
                return {0.58f, 0.47f, 0.30f, 1};
            case Ground::Stone:
                return {0.34f, 0.37f, 0.36f, 1};
            case Ground::Water:
                return {0.13f, 0.32f, 0.46f, 1};
            default:
                return {0.27f, 0.36f, 0.23f, 1};
        }
    }
}

void WorldView::Structure(
    SceneRenderer& renderer, Point2 origin, float zoom, Prop prop, int variation)
{
    // Large landmarks use the enlarged collision footprint in World::PropHalfExtent.
    const bool building = prop == Prop::Cottage || prop == Prop::Chapel || prop == Prop::Watchtower;
    zoom *= building ? 2.2f : 1.3f;
    auto p = [=](float x, float y)
    {
        return Point2{origin.x + x * zoom, origin.y + y * zoom};
    };
    auto box = [&](float x, float y, float w, float d, float h, Color color)
    {
        renderer.Quad(
            p(x - w, y - d), p(x, y), p(x, y - h), p(x - w, y - d - h), Shade(color, 0.65f));
        renderer.Quad(
            p(x, y), p(x + w, y - d), p(x + w, y - d - h), p(x, y - h), Shade(color, 0.85f));
        renderer.Quad(
            p(x, y - h), p(x + w, y - d - h), p(x, y - 2 * d - h), p(x - w, y - d - h), color);
    };
    auto roof = [&](float base, float ridge, Color color)
    {
        renderer.Quad(
            p(-29, base), p(0, base + 14), p(0, ridge), p(-29, ridge - 14), Shade(color, 0.70f));
        renderer.Quad(p(0, base + 14), p(29, base), p(29, ridge - 14), p(0, ridge), color);
    };
    Color stone{0.43f, 0.46f, 0.42f, 1};
    Color wood{0.38f, 0.28f, 0.20f, 1};
    Color roofColor = variation % 2 ? Color{0.37f, 0.25f, 0.20f, 1} : Color{0.25f, 0.30f, 0.31f, 1};
    renderer.Ellipse(p(0, 1), 30 * zoom, 13 * zoom, {0.015f, 0.02f, 0.02f, 0.35f});
    switch (prop)
    {
        case Prop::Cottage:
            box(0, 12, 25, 12, 35, {0.54f, 0.48f, 0.37f, 1});
            roof(-35, -53, roofColor);
            box(-15, -46, 4, 3, 20, stone);
            renderer.Quad(p(3, 9), p(12, 5), p(12, -15), p(3, -11), Shade(wood, 0.48f));
            renderer.Quad(
                p(-19, -22), p(-9, -17), p(-9, -8), p(-19, -13), {0.79f, 0.55f, 0.24f, 1, 1.3f});
            renderer.Line(p(-14, -20), p(-14, -10), zoom, wood);
            break;
        case Prop::Chapel:
            box(0, 12, 26, 13, 48, stone);
            roof(-47, -69, roofColor);
            box(-14, -46, 7, 5, 26, Shade(stone, 1.1f));
            renderer.Line(p(-14, -79), p(-14, -96), 2 * zoom, stone);
            renderer.Line(p(-19, -89), p(-9, -89), 2 * zoom, stone);
            renderer.Quad(p(2, 10), p(12, 5), p(12, -22), p(2, -17), Shade(wood, 0.5f));
            renderer.Ellipse(p(-12, -36), 4 * zoom, 7 * zoom, {0.50f, 0.67f, 0.71f, 1, 1.2f});
            break;
        case Prop::Watchtower:
            box(0, 10, 17, 9, 72, stone);
            box(0, -57, 24, 12, 8, Shade(stone, 0.85f));
            for (int i = -1; i <= 1; ++i)
            {
                box(float(i * 14), -62 - float(std::abs(i) * 6), 5, 3, 13, stone);
            }
            renderer.Quad(p(3, -43), p(8, -46), p(8, -28), p(3, -25), {0.08f, 0.10f, 0.10f, 1});
            renderer.Line(p(-5, -80), p(-5, -100), 2 * zoom, wood);
            renderer.Triangle(p(-4, -99), p(15, -94), p(-4, -86), {0.42f, 0.18f, 0.16f, 1});
            break;
        case Prop::Well:
            renderer.Ellipse(p(0, -2), 18 * zoom, 10 * zoom, stone);
            box(0, 7, 18, 9, 12, stone);
            renderer.Ellipse(p(0, -14), 16 * zoom, 7 * zoom, Shade(stone, 1.15f));
            renderer.Ellipse(p(0, -14), 11 * zoom, 4 * zoom, {0.06f, 0.13f, 0.15f, 1});
            renderer.Line(p(-15, -8), p(-15, -39), 3 * zoom, wood);
            renderer.Line(p(15, -8), p(15, -39), 3 * zoom, wood);
            renderer.Line(p(-16, -38), p(16, -38), 4 * zoom, wood);
            renderer.Line(p(0, -38), p(0, -14), zoom, {0.60f, 0.53f, 0.34f, 1});
            break;
        case Prop::Graves:
            for (int i = -1; i <= 1; ++i)
            {
                float x = float(i * 14), y = float(std::abs(i) * 4);
                box(x, y, 5, 3, 15 + float((variation + i + 3) % 3) * 3, stone);
                renderer.Line(p(x - 3, y - 12), p(x + 3, y - 12), zoom, Shade(stone, 0.45f));
                renderer.Line(p(x, y - 16), p(x, y - 7), zoom, Shade(stone, 0.45f));
            }
            break;
        default:
            break;
    }
    if (building)
    {
        // Timber framing / stone courses, recessed doors and warm lanterns.
        const float wallHeight = prop == Prop::Watchtower ? 58.0f
                                 : prop == Prop::Chapel   ? 40.0f
                                                          : 29.0f;
        for (float y = -8; y > -wallHeight; y -= 8)
        {
            renderer.Line(p(-23, y - 10), p(0, y), 0.65f * zoom, Shade(stone, 0.5f));
            renderer.Line(p(0, y), p(23, y - 10), 0.65f * zoom, Shade(stone, 0.6f));
        }
        for (float x : {-21.0f, -10.0f, 10.0f, 21.0f})
        {
            float base = 10 - std::abs(x) * 0.48f;
            renderer.Line(p(x, base),
                          p(x, base - wallHeight),
                          1.25f * zoom,
                          prop == Prop::Cottage ? wood : Shade(stone, 0.7f));
        }
        box(6, 13, 10, 4, 3, Shade(stone, 0.8f));
        box(6, 15, 12, 4, 2, stone);
        renderer.Line(p(17, -13), p(17, -25), zoom, wood);
        renderer.Ellipse(p(17, -17), 2.0f * zoom, 3.3f * zoom, {1.0f, 0.68f, 0.24f, 1, 2.2f});
    }
}

void WorldView::MiniMap(SceneRenderer& renderer,
                        const World& world,
                        const LevelOne& level,
                        const NpcSystem& npcs,
                        int width,
                        int height,
                        double dt)
{
    if (width < 600 || height < 400)
    {
        return;
    }
    constexpr int radius = 20;
    constexpr int cells = radius * 2 + 1;
    float size = (std::min)(std::clamp(width * 0.20f, 124.0f, 184.0f), float(height) - 250.0f);
    float left = width - size - 24, top = 135, cell = size / cells;
    Color light{0.86f, 0.89f, 0.81f, 1};
    renderer.Rect(left - 10, top - 34, size + 20, size + 83, {0.025f, 0.04f, 0.05f, 0.96f});
    renderer.KoreanText(left, top - 30, L"주변 지도", light, 16, int(size - 20));
    renderer.Text(left + size - 12, top - 23, "N", light, 1.3f);
    const auto& player = level.Player().position;

    struct MapState
    {
        bool ready = false;
        std::uint64_t seed = 0;
        double x = 0, y = 0;
        std::map<ChunkKey, float> chunkOpacity;
    };

    static MapState state;
    if (!state.ready || state.seed != world.Seed() ||
        std::hypot(player.x - state.x, player.y - state.y) > 8.0)
    {
        state.ready = true;
        state.seed = world.Seed();
        state.x = player.x;
        state.y = player.y;
        state.chunkOpacity.clear();
    }
    dt = std::clamp(dt, 0.0, 0.1);
    double follow = 1.0 - std::exp(-12.0 * dt);
    state.x += (player.x - state.x) * follow;
    state.y += (player.y - state.y) * follow;
    for (const auto& chunk : world.Chunks())
    {
        float& opacity = state.chunkOpacity[chunk.first];
        opacity = (std::min)(1.0f, opacity + float(dt / 0.35));
    }
    for (auto it = state.chunkOpacity.begin(); it != state.chunkOpacity.end();)
    {
        if (!world.Chunks().count(it->first))
        {
            it = state.chunkOpacity.erase(it);
        }
        else
        {
            ++it;
        }
    }
    auto cx = WorldInt(std::floor(state.x)), cy = WorldInt(std::floor(state.y));
    for (int y = -radius - 1; y <= radius + 1; ++y)
    {
        for (int x = -radius - 1; x <= radius + 1; ++x)
        {
            const auto* tile = world.Find(cx + x, cy + y);
            Color color = tile ? MapColor(*tile) : Color{0.055f, 0.065f, 0.08f, 1};
            if (tile)
            {
                float fade = state.chunkOpacity[{World::ChunkOf(cx + x), World::ChunkOf(cy + y)}];
                color.r = 0.055f + (color.r - 0.055f) * fade;
                color.g = 0.065f + (color.g - 0.065f) * fade;
                color.b = 0.08f + (color.b - 0.08f) * fade;
            }
            float px = left + size * 0.5f + float(double(cx) - state.x + x) * cell;
            float py = top + size * 0.5f + float(double(cy) - state.y + y) * cell;
            float x0 = (std::max)(left, px), y0 = (std::max)(top, py);
            float x1 = (std::min)(left + size, px + cell), y1 = (std::min)(top + size, py + cell);
            if (x1 > x0 && y1 > y0)
            {
                renderer.Rect(x0, y0, x1 - x0, y1 - y0, color);
            }
        }
    }
    auto marker = [&](double x, double y, Color color, float extent)
    {
        float px = left + size * 0.5f + float(x - state.x) * cell;
        float py = top + size * 0.5f + float(y - state.y) * cell;
        if (px < left + extent + 1 || px > left + size - extent - 1 || py < top + extent + 1 ||
            py > top + size - extent - 1)
        {
            return;
        }
        if (!world.Find(WorldInt(std::floor(x)), WorldInt(std::floor(y))))
        {
            return;
        }
        renderer.Rect(px - extent - 1,
                      py - extent - 1,
                      extent * 2 + 2,
                      extent * 2 + 2,
                      {0.02f, 0.025f, 0.03f, 1});
        renderer.Rect(px - extent, py - extent, extent * 2, extent * 2, color);
    };
    marker(0.5, 0.5, {0.94f, 0.73f, 0.25f, 1}, 3);
    for (const auto& npc : npcs.Residents())
    {
        if (npcs.IsPresent(npc, world))
        {
            marker(npc.x, npc.y, {0.25f, 0.95f, 0.84f, 1}, 2);
        }
    }
    for (const auto& soul : level.Souls())
    {
        marker(soul.position.x, soul.position.y, {0.52f, 0.66f, 1.0f, 1}, 2);
    }
    for (const auto& enemy : level.Enemies())
    {
        if (enemy.health <= 0)
        {
            continue;
        }
        bool boss = enemy.kind == EnemyKind::Boss;
        marker(enemy.position.x,
               enemy.position.y,
               boss ? Color{0.95f, 0.28f, 0.85f, 1} : Color{0.94f, 0.25f, 0.20f, 1},
               boss ? 3.0f : 1.5f);
    }
    marker(player.x, player.y, {1, 1, 1, 1}, 3);
    renderer.KoreanText(left, top + size + 5, L"흰색: 나  청록: 주민", light, 12, int(size));
    renderer.KoreanText(left, top + size + 24, L"빨강: 적  금색: 캠프", light, 12, int(size));
}
