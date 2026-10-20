#include "stdafx.h"
#include "CharacterArt.h"
#include "LevelOneView.h"
#include "VillageLayout.h"
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
    if (showRange)
    {
        for (const auto& village : Villages())
        {
            double radius = VillageWallRadius - 0.5;
            auto center = village.center;
            Point2 corners[] = {project(center.x - radius, center.y - radius, 0),
                                project(center.x + radius, center.y - radius, 0),
                                project(center.x + radius, center.y + radius, 0),
                                project(center.x - radius, center.y + radius, 0)};
            for (int i = 0; i < 4; ++i)
            {
                renderer.Line(
                    corners[i], corners[(i + 1) % 4], 1.4f * zoom, {.38f, .75f, .58f, .4f});
            }
        }
    }
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

void LevelOneView::DrawEnemy(SceneRenderer& renderer,
                             const Enemy& enemy,
                             Point2 p,
                             float zoom,
                             bool targeted,
                             const ActorPose& pose)
{
    bool boss = enemy.kind == EnemyKind::Boss, hound = enemy.kind == EnemyKind::Hound;
    float size = (boss ? 1.9f : 1.0f) * zoom;
    bool enraged = boss && enemy.health <= enemy.maxHealth * .5;
    Color body = hound  ? Color{.50f, .30f, .25f, 1}
                 : boss ? (enraged ? Color{.65f, .23f, .19f, 1} : Color{.40f, .25f, .30f, 1})
                        : Color{.39f, .44f, .42f, 1};
    Color bone{.67f, .65f, .50f, 1}, iron{.22f, .27f, .28f, 1}, rag{.27f, .19f, .20f, 1};
    float charge = float(std::clamp(enemy.windup / 1.25, 0.0, 1.0));
    if (targeted)
    {
        Ring(renderer, p, boss ? .65 : .38, zoom, {.96f, .66f, .21f, .8f});
    }
    // Shadow, four limbs, torso, head and weapon/tail. All geometry is model-space.
    for (int part = 0; part < 8; ++part)
    {
        std::string key = "enemy:rig1:" + std::to_string(int(enemy.kind)) + ":" +
                          std::to_string(enraged) + ":" + std::to_string(part);
        Point2 pivot{0, -18};
        float angle = pose.breath * .025f - pose.hurt * .15f;
        if (part >= 1 && part <= 4)
        {
            float x = hound ? -13 + (part - 1) * 9.0f : (part % 2 ? -7.0f : 7.0f);
            pivot = {x, hound ? -10.0f : -13.0f};
            angle = pose.stride * (part % 2 ? .5f : -.5f);
            if (!hound && part >= 3)
            {
                pivot = {part == 3 ? -12.0f : 12.0f, -26};
                angle = pose.stride * (part == 3 ? -.35f : .35f) -
                        (part == 4 ? pose.attack * 1.2f + charge * .8f : 0);
            }
        }
        if (part == 6)
        {
            pivot = hound ? Point2{9, -14} : Point2{0, -28};
            angle += hound ? -pose.attack * .35f : -charge * .15f;
        }
        if (part == 7)
        {
            pivot = hound ? Point2{-15, -13} : Point2{12, -26};
            angle = hound ? pose.stride * .25f + pose.breath * .12f
                          : pose.stride * .35f - pose.attack * 1.2f - charge * .8f;
        }
        if (renderer.BeginMesh(key, {0, 0}, 1))
        {
            if (part == 0)
            {
                renderer.Ellipse({0, 1}, hound ? 21.0f : 16.0f, 6, {0, 0, 0, .4f});
            }
            else if (part <= 4)
            {
                if (hound || part <= 2)
                {
                    Point2 knee{pivot.x + 2, -5}, foot{pivot.x + 4, 0};
                    renderer.Line(pivot, knee, hound ? 3.0f : 6.0f, iron);
                    renderer.Line(knee, foot, hound ? 3.0f : 5.0f, body);
                    renderer.Line(foot, {foot.x + 4, 0}, 2, bone);
                }
                else
                {
                    renderer.Line(pivot, {pivot.x * 1.4f, -16}, 5, body);
                    renderer.Ellipse({pivot.x * 1.4f, -15}, 3, 3, bone);
                }
            }
            else if (part == 5)
            {
                if (hound)
                {
                    renderer.Quad({-18, -13}, {8, -19}, {16, -6}, {-12, -3}, body);
                    for (int i = 0; i < 4; ++i)
                    {
                        float x = -13 + i * 6.0f;
                        renderer.Triangle({x, -18}, {x + 2, -27}, {x + 6, -19}, iron);
                        renderer.Line({x, -15}, {x + 3, -8}, 1, bone);
                    }
                }
                else
                {
                    renderer.Triangle({-9, -28}, {-19, -3}, {11, -8}, rag);
                    renderer.Quad({-11, -28}, {11, -28}, {14, -6}, {-13, -6}, body);
                    for (int i = 0; i < 3; ++i)
                    {
                        renderer.Line({-7, -24 + i * 4.0f}, {7, -24 + i * 4.0f}, 1, bone);
                    }
                    if (boss)
                    {
                        renderer.Quad({-10, -29}, {10, -29}, {8, -14}, {-8, -14}, iron);
                        renderer.Line({0, -28}, {0, -15}, 2, Gold);
                        for (float side : {-1.0f, 1.0f})
                        {
                            renderer.Ellipse({13 * side, -27}, 7, 5, iron);
                            renderer.Triangle(
                                {17 * side, -27}, {23 * side, -38}, {10 * side, -31}, bone);
                        }
                    }
                }
            }
            else if (part == 6)
            {
                if (hound)
                {
                    renderer.Triangle({6, -16}, {10, -30}, {17, -11}, body);
                    renderer.Quad({10, -16}, {24, -13}, {25, -7}, {13, -5}, bone);
                    renderer.Line({13, -10}, {25, -10}, 1, iron);
                    renderer.Triangle({18, -10}, {20, -5}, {21, -10}, bone);
                    renderer.Ellipse({13, -16}, 2, 2, {1, .22f, .06f, 1, 2});
                }
                else
                {
                    renderer.Ellipse({0, -35}, 8, 10, body);
                    renderer.Ellipse({1, -35}, 6, 8, bone);
                    renderer.Line({-3, -37}, {6, -37}, 2, iron);
                    renderer.Line({-2, -30}, {5, -30}, 1, iron);
                    renderer.Ellipse({3, -37}, 2, 1.5f, {1, .22f, .06f, 1, 2});
                    if (boss)
                    {
                        for (float side : {-1.0f, 1.0f})
                        {
                            renderer.Triangle(
                                {9 * side, -33}, {15 * side, -54}, {2 * side, -39}, Gold);
                        }
                    }
                }
            }
            else if (hound)
            {
                renderer.Triangle({-15, -13}, {-29, -27}, {-18, -5}, body);
            }
            else
            {
                renderer.Line({20, -5}, {20, -36}, 2, iron);
                renderer.Triangle({19, -34}, {23, -43}, {23, -15}, bone);
                renderer.Line({23, -40}, {23, -18}, 1, {1, .35f, .1f, 1, 1.7f});
            }
            renderer.EndMesh();
        }
        Point2 point = p;
        if (part > 0)
        {
            point.y +=
                (pose.breath * .7f - std::abs(pose.stride) * (hound ? 2.5f : 1.5f) + charge * 2) *
                size;
            point.x += pose.facing * (pose.attack * 3 - pose.hurt * 3) * size;
        }
        Color tint{1, 1 - pose.hurt * .4f, 1 - pose.hurt * .55f, 1, 1 + pose.hurt};
        renderer.DrawMeshPose(key,
                              point,
                              size,
                              pivot,
                              part == 0 ? 0 : angle,
                              pose.facing,
                              part == 0 ? Color{1, 1, 1, 1} : tint);
    }
    CharacterArt::Aura(renderer, p, size, pose, {1, .36f, .12f, 1});
    Bar(renderer,
        p.x - 19 * size,
        p.y - (boss ? 60 : 50) * size,
        38 * size,
        5,
        enemy.health / enemy.maxHealth,
        {.80f, .19f, .12f, 1});
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
            case DropKind::Equipment:
                color = {.77f, .48f, 1.0f, 1, 2.2f};
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
    std::string key = "soul:rpg2:" + std::to_string(soul.feeling);
    if (renderer.BeginMesh(key, p, zoom))
    {
        Color color = soul.feeling == 1   ? Color{0.58f, 0.87f, 0.62f, 0.58f, 1.6f}
                      : soul.feeling == 2 ? Color{0.89f, 0.64f, 0.38f, 0.58f, 1.6f}
                                          : Color{0.46f, 0.72f, 0.94f, 0.58f, 1.6f};
        renderer.Ellipse(p, 18 * zoom, 7 * zoom, {color.r, color.g, color.b, 0.18f, 2.0f});
        renderer.Triangle(
            {p.x, p.y - 38 * zoom}, {p.x - 14 * zoom, p.y}, {p.x + 14 * zoom, p.y}, color);
        renderer.Ellipse({p.x, p.y - 36 * zoom}, 7 * zoom, 9 * zoom, color);
        renderer.Quad({p.x - 10 * zoom, p.y - 31 * zoom},
                      {p.x + 10 * zoom, p.y - 31 * zoom},
                      {p.x + 16 * zoom, p.y - 8 * zoom},
                      {p.x - 15 * zoom, p.y - 5 * zoom},
                      {color.r * .6f, color.g * .7f, color.b * .8f, .5f, 1.5f});
        renderer.Ellipse({p.x, p.y - 39 * zoom}, 9 * zoom, 11 * zoom, color);
        renderer.Ellipse(
            {p.x + zoom, p.y - 38 * zoom}, 5 * zoom, 7 * zoom, {.08f, .16f, .20f, .75f});
        renderer.Line(
            {p.x - 9 * zoom, p.y - 29 * zoom}, {p.x - 13 * zoom, p.y - 8 * zoom}, zoom, color);
        renderer.Line(
            {p.x + 9 * zoom, p.y - 29 * zoom}, {p.x + 13 * zoom, p.y - 8 * zoom}, zoom, color);
        renderer.Ellipse({p.x + 17 * zoom, p.y - 25 * zoom},
                         3 * zoom,
                         4 * zoom,
                         {color.r, color.g, color.b, .8f, 3});
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
    renderer.Rect(20, 20, 304, 132, {0.035f, 0.045f, 0.045f, 0.92f});
    renderer.Rect(20, 20, 304, 1, Gold);
    renderer.KoreanText(
        32, 30, L"남겨진 자   ·   Lv " + std::to_wstring(player.level), Gold, 19, 280);
    Bar(renderer, 32, 59, 280, 19, player.health / player.MaxHealth(), {0.62f, 0.14f, 0.19f, 1});
    renderer.Text(40,
                  64,
                  "HP " + std::to_string(int(std::ceil(player.health))) + " / " +
                      std::to_string(int(player.MaxHealth())),
                  TextColor,
                  1.2f);
    Bar(renderer,
        32,
        84,
        280,
        8,
        player.level >= MaxCharacterLevel ? 1.0
                                          : double(player.experience) / player.ExperienceNeeded(),
        {0.24f, 0.55f, 0.86f, 1});
    std::ostringstream stats;
    stats << "XP " << player.experience << '/' << player.ExperienceNeeded() << "   WEAPON +"
          << player.weaponRank;
    renderer.Text(32, 100, stats.str(), TextColor, 1.1f);
    stats.str("");
    stats.clear();
    stats << "DMG " << int(player.Damage()) << "   CD " << std::fixed << std::setprecision(2)
          << player.ShotCooldown() << "   DEF " << int(player.gearDefense);
    renderer.Text(32, 118, stats.str(), TextColor, 1.1f);
    Bar(renderer, 32, 138, 280, 5, level.ShotReady(), Gold);
    if (level.MagnetTime() > 0.0)
    {
        renderer.Text(32,
                      154,
                      "MAGNET " + std::to_string(int(std::ceil(level.MagnetTime()))) + " SEC",
                      {0.23f, 0.88f, 0.77f, 1},
                      1.2f);
    }
    if (!level.Simulation())
    {
        renderer.KoreanText(24, 176, level.Objective(), Gold, 16, 280);
        if (!dialogue)
        {
            renderer.KoreanText(24, 226, level.Hint(), TextColor, 14, 280);
        }
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
            renderer.Rect(left, 64, barWidth, 61, {0.06f, 0.025f, 0.035f, 0.93f});
            renderer.KoreanText(
                left + 12, 69, L"잿빛 파수꾼", {1.0f, 0.45f, 0.26f, 1}, 20, int(barWidth) - 24);
            Bar(renderer,
                left + 12,
                100,
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
        int wrap = (std::max)(120, (std::min)(480, width - 100));
        float textHeight = renderer.KoreanTextHeight(level.Notice(), 18, wrap);
        float left = (float(width) - wrap - 32) * 0.5f;
        renderer.Rect(left,
                      float(height) - 190 - textHeight,
                      float(wrap) + 32,
                      textHeight + 12,
                      {0.045f, 0.065f, 0.075f, 0.94f});
        renderer.KoreanText(
            left + 16, float(height) - 184 - textHeight, level.Notice(), Gold, 18, wrap);
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
