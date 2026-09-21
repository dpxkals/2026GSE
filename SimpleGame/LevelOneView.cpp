#include "stdafx.h"
#include "LevelOneView.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace
{
    const Color TextColor{0.86f, 0.88f, 0.79f, 1.0f};
    const Color Gold{0.95f, 0.70f, 0.35f, 1.0f};

    void Ring(SceneRenderer& renderer, Point2 center, double radius, float zoom, Color color)
    {
        float rx = float(radius) * 50.9117f * zoom;
        float ry = float(radius) * 25.4558f * zoom;
        for (int i = 0; i < 64; ++i)
        {
            float a = i * 6.2831853f / 64;
            float b = (i + 1) * 6.2831853f / 64;
            renderer.Line({center.x + std::cos(a) * rx, center.y + std::sin(a) * ry},
                          {center.x + std::cos(b) * rx, center.y + std::sin(b) * ry},
                          1.4f,
                          color);
        }
    }

    void Bar(SceneRenderer& renderer,
             float x,
             float y,
             float width,
             float height,
             double fraction,
             Color fill)
    {
        renderer.Rect(x, y, width, height, {0.035f, 0.045f, 0.055f, 0.96f});
        renderer.Rect(
            x + 2, y + 2, (width - 4) * float(std::clamp(fraction, 0.0, 1.0)), height - 4, fill);
    }
}

void LevelOneView::Ground(SceneRenderer& renderer,
                          const LevelOne& level,
                          const Projection& project,
                          float zoom,
                          bool showRange)
{
    Ring(renderer, project(0.5, 0.5, 0), 3.8, zoom, {0.38f, 0.75f, 0.58f, 0.55f});
    const auto& player = level.Player();
    if (showRange)
    {
        Ring(renderer,
             project(player.position.x, player.position.y, 0),
             player.AttackRange(),
             zoom,
             {0.73f, 0.72f, 0.44f, 0.28f});
    }
    if (level.MagnetTime() > 0)
    {
        Ring(renderer,
             project(player.position.x, player.position.y, 0),
             12.0,
             zoom,
             {0.23f, 0.83f, 0.88f, 0.35f});
    }
    for (const Enemy& enemy : level.Enemies())
    {
        if (enemy.windup <= 0.0)
        {
            continue;
        }
        Point2 point = project(enemy.attackPoint.x, enemy.attackPoint.y, 0);
        float progress = float(std::clamp(1.0 - enemy.windup / 1.25, 0.0, 1.0));
        renderer.Ellipse(point, 117.1f * zoom, 58.55f * zoom, {0.9f, 0.10f, 0.055f, 0.15f});
        renderer.Ellipse(
            point, 117.1f * zoom * progress, 58.55f * zoom * progress, {1.0f, 0.18f, 0.06f, 0.20f});
        Ring(renderer, point, 2.3, zoom, {1.0f, 0.24f, 0.07f, 0.85f});
    }
}

void LevelOneView::DrawEnemy(
    SceneRenderer& renderer, const Enemy& enemy, Point2 p, float zoom, bool targeted)
{
    bool boss = enemy.kind == EnemyKind::Boss;
    float size = (boss ? 1.9f : 1.0f) * zoom;
    Color body = enemy.kind == EnemyKind::Hound ? Color{0.50f, 0.30f, 0.25f, 1}
                                                : Color{0.39f, 0.44f, 0.42f, 1};
    if (boss)
    {
        body = enemy.health <= enemy.maxHealth * 0.5 ? Color{0.65f, 0.23f, 0.19f, 1}
                                                     : Color{0.40f, 0.25f, 0.30f, 1};
    }
    if (enemy.hitFlash > 0.0)
    {
        body = {0.98f, 0.76f, 0.52f, 1, 1.5f};
    }
    if (targeted)
    {
        Ring(renderer, p, boss ? 0.65 : 0.38, zoom, {0.96f, 0.66f, 0.21f, 0.8f});
    }
    std::string key = "enemy:" + std::to_string(int(enemy.kind)) + ":" +
                      std::to_string(enemy.health <= enemy.maxHealth * 0.5) + ":" +
                      std::to_string(enemy.hitFlash > 0.0);
    if (renderer.BeginMesh(key, p, zoom))
    {
        renderer.Ellipse(p, 14 * size, 6 * size, {0.015f, 0.02f, 0.02f, 0.45f});
        if (enemy.kind == EnemyKind::Hound)
        {
            renderer.Quad({p.x - 18 * size, p.y - 13 * size},
                          {p.x + 8 * size, p.y - 19 * size},
                          {p.x + 16 * size, p.y - 6 * size},
                          {p.x - 12 * size, p.y - 3 * size},
                          body);
            renderer.Triangle({p.x + 6 * size, p.y - 16 * size},
                              {p.x + 10 * size, p.y - 30 * size},
                              {p.x + 17 * size, p.y - 11 * size},
                              body);
        }
        else
        {
            renderer.Quad({p.x - 11 * size, p.y - 28 * size},
                          {p.x + 11 * size, p.y - 28 * size},
                          {p.x + 14 * size, p.y - 4 * size},
                          {p.x - 13 * size, p.y - 4 * size},
                          body);
            renderer.Ellipse({p.x, p.y - 35 * size}, 8 * size, 10 * size, body);
            if (boss)
            {
                renderer.Triangle({p.x - 9 * size, p.y - 33 * size},
                                  {p.x - 15 * size, p.y - 54 * size},
                                  {p.x - 2 * size, p.y - 39 * size},
                                  Gold);
                renderer.Triangle({p.x + 9 * size, p.y - 33 * size},
                                  {p.x + 15 * size, p.y - 54 * size},
                                  {p.x + 2 * size, p.y - 39 * size},
                                  Gold);
            }
        }
        float eyeHeight = enemy.kind == EnemyKind::Hound ? 15 : 36;
        renderer.Ellipse({p.x + 3 * size, p.y - eyeHeight * size},
                         3 * size,
                         2 * size,
                         {1.0f, 0.22f, 0.06f, 1, 2.0f});
        renderer.EndMesh();
    }
    renderer.DrawMesh(key, p, zoom);
    Bar(renderer,
        p.x - 19 * size,
        p.y - (boss ? 60 : 50) * size,
        38 * size,
        5,
        enemy.health / enemy.maxHealth,
        {0.80f, 0.19f, 0.12f, 1});
}

void LevelOneView::DrawDrop(SceneRenderer& renderer, const ItemDrop& drop, Point2 p, float zoom)
{
    std::string key = "drop:" + std::to_string(int(drop.kind));
    if (renderer.BeginMesh(key, p, zoom))
    {
        Color color;
        switch (drop.kind)
        {
            case DropKind::Soul:
                color = {0.40f, 0.68f, 1.0f, 1, 1.9f};
                break;
            case DropKind::Upgrade:
                color = {1.0f, 0.61f, 0.19f, 1, 1.8f};
                break;
            case DropKind::Health:
                color = {0.95f, 0.24f, 0.32f, 1, 1.3f};
                break;
            case DropKind::Magnet:
                color = {0.18f, 0.95f, 0.79f, 1, 1.8f};
                break;
        }
        renderer.Ellipse(p, 9 * zoom, 4 * zoom, {color.r, color.g, color.b, 0.16f});
        // Offset different drop types visually even when the pickup positions coincide.
        float offset = (int(drop.kind) - 1.5f) * 4 * zoom;
        float x = p.x + offset;
        renderer.Quad({x, p.y - 14 * zoom},
                      {x + 5 * zoom, p.y - 8 * zoom},
                      {x, p.y - 2 * zoom},
                      {x - 5 * zoom, p.y - 8 * zoom},
                      color);
        if (drop.kind == DropKind::Health)
        {
            renderer.Rect(x - zoom, p.y - 12 * zoom, 2 * zoom, 7 * zoom, TextColor);
            renderer.Rect(x - 3 * zoom, p.y - 10 * zoom, 6 * zoom, 2 * zoom, TextColor);
        }
        renderer.EndMesh();
    }
    renderer.DrawMesh(key, p, zoom);
}

void LevelOneView::DrawShot(SceneRenderer& renderer, const Projectile& shot, Point2 p, float zoom)
{
    Color color =
        shot.hostile ? Color{1.0f, 0.24f, 0.08f, 1, 3.0f} : Color{1.0f, 0.81f, 0.43f, 1, 4.0f};
    float dx = float(shot.velocityX - shot.velocityY);
    float dy = float(shot.velocityX + shot.velocityY) * 0.5f;
    float length = (std::max)(0.01f, std::hypot(dx, dy));
    p.y -= 14 * zoom;
    renderer.Line(
        {p.x - dx / length * 13 * zoom, p.y - dy / length * 13 * zoom}, p, 2 * zoom, color);
    renderer.Ellipse(p, 3.5f * zoom, 3.5f * zoom, color);
}

void LevelOneView::DrawSoul(
    SceneRenderer& renderer, const SoulNpc& soul, Point2 p, float zoom, double time)
{
    p.y -= float(3.0 + std::sin(time * 1.6 + double(soul.id % 100)) * 2.0) * zoom;
    std::string key = "soul:" + std::to_string(soul.feeling);
    if (renderer.BeginMesh(key, p, zoom))
    {
        Color color = soul.feeling == 1   ? Color{0.58f, 0.87f, 0.62f, 0.58f, 1.6f}
                      : soul.feeling == 2 ? Color{0.89f, 0.64f, 0.38f, 0.58f, 1.6f}
                                          : Color{0.46f, 0.72f, 0.94f, 0.58f, 1.6f};
        renderer.Ellipse(p, 18 * zoom, 7 * zoom, {color.r, color.g, color.b, 0.18f, 2.0f});
        renderer.Triangle(
            {p.x, p.y - 38 * zoom}, {p.x - 14 * zoom, p.y}, {p.x + 14 * zoom, p.y}, color);
        renderer.Ellipse({p.x, p.y - 36 * zoom}, 7 * zoom, 9 * zoom, color);
        renderer.Ellipse({p.x - 2 * zoom, p.y - 37 * zoom},
                         2 * zoom,
                         2 * zoom,
                         {0.85f, 0.96f, 1.0f, 0.9f, 2.5f});
        renderer.EndMesh();
    }
    renderer.DrawMesh(key, p, zoom);
}

void LevelOneView::Numbers(SceneRenderer& renderer,
                           const LevelOne& level,
                           const Projection& project)
{
    for (const FloatingNumber& number : level.Numbers())
    {
        Point2 p =
            project(number.position.x, number.position.y, float(35 + (0.75 - number.life) * 30));
        renderer.Text(p.x - 8,
                      p.y,
                      std::to_string(number.amount),
                      number.playerHit ? Color{1.0f, 0.32f, 0.22f, 1} : Gold,
                      1.5f);
    }
}

void LevelOneView::DrawNumber(SceneRenderer& renderer,
                              const FloatingNumber& number,
                              const Projection& project)
{
    Point2 p = project(number.position.x, number.position.y, float(35 + (0.75 - number.life) * 30));
    renderer.Text(p.x - 8,
                  p.y,
                  std::to_string(number.amount),
                  number.playerHit ? Color{1.0f, 0.32f, 0.22f, 1} : Gold,
                  1.5f);
}

void LevelOneView::HUD(SceneRenderer& renderer,
                       const LevelOne& level,
                       int width,
                       int height,
                       bool paused,
                       bool dialogue,
                       const Projection& project)
{
    const auto& player = level.Player();
    renderer.Rect(20, 101, 320, 150, {0.025f, 0.045f, 0.055f, 0.94f});
    renderer.KoreanText(32, 108, L"레벨 1 · 잿빛 들판", Gold, 20, 296);
    Bar(renderer, 32, 139, 294, 17, player.health / player.MaxHealth(), {0.74f, 0.19f, 0.22f, 1});
    renderer.Text(40,
                  143,
                  "HP " + std::to_string(int(std::ceil(player.health))) + " / " +
                      std::to_string(int(player.MaxHealth())),
                  TextColor,
                  1.2f);
    Bar(renderer,
        32,
        162,
        294,
        13,
        player.level >= 30 ? 1.0 : double(player.experience) / player.ExperienceNeeded(),
        {0.24f, 0.55f, 0.86f, 1});
    std::ostringstream stats;
    stats << "LV " << player.level << "   XP " << player.experience << '/'
          << player.ExperienceNeeded() << "   WEAPON +" << player.weaponRank;
    renderer.Text(32, 184, stats.str(), TextColor, 1.2f);
    stats.str("");
    stats.clear();
    stats << "DMG " << int(player.Damage()) << "   CD " << std::fixed << std::setprecision(2)
          << player.ShotCooldown() << "   RANGE 6";
    renderer.Text(32, 203, stats.str(), TextColor, 1.2f);
    Bar(renderer, 32, 223, 294, 6, level.ShotReady(), Gold);
    if (level.MagnetTime() > 0.0)
    {
        renderer.Text(32,
                      235,
                      "MAGNET " + std::to_string(int(std::ceil(level.MagnetTime()))) + " SEC",
                      {0.23f, 0.88f, 0.77f, 1},
                      1.2f);
    }
    renderer.KoreanText(24, 260, level.Objective(), Gold, 18, (std::max)(180, width - 210));
    if (!dialogue)
    {
        renderer.KoreanText(24, 289, level.Hint(), TextColor, 16, (std::max)(180, width - 210));
    }

    for (const Enemy& enemy : level.Enemies())
    {
        if (enemy.kind != EnemyKind::Boss)
        {
            continue;
        }
        float barWidth = (std::min)(440.0f, float(width) - 540.0f);
        if (barWidth >= 150)
        {
            float left = 360;
            renderer.Rect(left, 102, barWidth, 61, {0.06f, 0.025f, 0.035f, 0.93f});
            renderer.KoreanText(
                left + 12, 107, L"잿빛 파수꾼", {1.0f, 0.45f, 0.26f, 1}, 20, int(barWidth) - 24);
            Bar(renderer,
                left + 12,
                138,
                barWidth - 24,
                13,
                enemy.health / enemy.maxHealth,
                {0.84f, 0.16f, 0.12f, 1});
        }
        Point2 p = project(enemy.position.x, enemy.position.y, 0);
        if (p.x < 20 || p.x > width - 20 || p.y < 95 || p.y > height - 70)
        {
            p.x = std::clamp(p.x, 35.0f, (std::max)(35.0f, float(width) - 95));
            p.y = std::clamp(p.y, 330.0f, (std::max)(330.0f, float(height) - 170));
            renderer.Text(p.x, p.y, "BOSS", {1.0f, 0.35f, 0.18f, 1}, 1.7f);
        }
    }
    if (!level.Error().empty())
    {
        renderer.Rect(
            20, float(height) - 208, float(width) - 40, 42, {0.34f, 0.055f, 0.05f, 0.95f});
        renderer.KoreanText(32,
                            float(height) - 201,
                            L"전투 저장 오류 — 진행을 멈췄습니다. 콘솔을 확인하세요.",
                            TextColor,
                            18,
                            width - 64);
    }
    else if (!dialogue && !level.Notice().empty())
    {
        int wrap = (std::max)(120, width - 100);
        float textHeight = renderer.KoreanTextHeight(level.Notice(), 18, wrap);
        renderer.Rect(30,
                      float(height) - 84 - textHeight,
                      float(width) - 60,
                      textHeight + 12,
                      {0.045f, 0.065f, 0.075f, 0.94f});
        renderer.KoreanText(46, float(height) - 78 - textHeight, level.Notice(), Gold, 18, wrap);
    }
    if (!dialogue && !level.Souls().empty())
    {
        const auto& soul = level.Souls().back();
        Point2 p = project(soul.position.x, soul.position.y, 50);
        float distance = float(
            std::hypot(soul.position.x - player.position.x, soul.position.y - player.position.y));
        p.x = std::clamp(p.x, 24.0f, (std::max)(24.0f, float(width) - 180));
        p.y = std::clamp(p.y, 325.0f, (std::max)(325.0f, float(height) - 230));
        renderer.Text(p.x,
                      p.y,
                      "SOUL " + std::to_string(soul.deathNumber) + " / " +
                          std::to_string(int(distance)) + " TILES",
                      {0.50f, 0.85f, 1.0f, 1},
                      1.2f);
    }
    if (paused && !dialogue)
    {
        renderer.Rect(
            width * 0.5f - 160, height * 0.5f - 30, 320, 60, {0.03f, 0.05f, 0.07f, 0.96f});
        renderer.KoreanText(
            width * 0.5f - 143, height * 0.5f - 20, L"일시정지 · P 키로 계속", TextColor, 22, 290);
    }
}
