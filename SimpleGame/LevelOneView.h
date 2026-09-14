#pragma once

#include "LevelOne.h"
#include "SceneRenderer.h"
#include <functional>

class LevelOneView
{
  public:
    using Projection = std::function<Point2(double, double, float)>;

    static void Ground(SceneRenderer& renderer,
                       const LevelOne& level,
                       const Projection& project,
                       float zoom,
                       bool showRange);
    static void DrawEnemy(
        SceneRenderer& renderer, const Enemy& enemy, Point2 point, float zoom, bool targeted);
    static void DrawDrop(SceneRenderer& renderer, const ItemDrop& drop, Point2 point, float zoom);
    static void DrawShot(SceneRenderer& renderer, const Projectile& shot, Point2 point, float zoom);
    static void DrawSoul(
        SceneRenderer& renderer, const SoulNpc& soul, Point2 point, float zoom, double time);
    static void Numbers(SceneRenderer& renderer, const LevelOne& level, const Projection& project);
    static void HUD(SceneRenderer& renderer,
                    const LevelOne& level,
                    int width,
                    int height,
                    bool paused,
                    bool dialogue,
                    const Projection& project);
};
