#include "stdafx.h"
#include "CharacterArt.h"
#include <algorithm>
#include <cmath>
#include <string>

void CharacterArt::Aura(
    SceneRenderer& r, Point2 origin, float z, const ActorPose& pose, Color color)
{
    // White cached effects are tinted at submission; no time / actor ID in cache keys.
    const std::string arc = "anim:slash:1", spark = "anim:spark:1";
    if (r.BeginMesh(arc, {0, 0}, 1))
    {
        for (int i = 0; i < 10; ++i)
        {
            float a = -.9f + i * .13f, b = a + .13f;
            r.Quad({std::cos(a) * 33, -25 + std::sin(a) * 25},
                   {std::cos(b) * 33, -25 + std::sin(b) * 25},
                   {std::cos(b) * 28, -25 + std::sin(b) * 21},
                   {std::cos(a) * 28, -25 + std::sin(a) * 21},
                   {1, 1, 1, float(i + 1) / 14, 2});
        }
        r.EndMesh();
    }
    if (r.BeginMesh(spark, {0, 0}, 1))
    {
        r.Quad({-3, 0}, {0, -6}, {3, 0}, {0, 6}, {1, 1, 1, .8f, 2});
        r.EndMesh();
    }
    if (pose.attack > .05f)
    {
        color.a *= pose.attack;
        r.DrawMeshPose(arc, origin, z, {0, -25}, -.55f + pose.attack * .9f, pose.facing, color);
        for (int i = 0; i < 3; ++i)
        {
            Point2 point{origin.x + pose.facing * (24 + i * 5) * z,
                         origin.y + (-34 + i * 9 - pose.attack * 7) * z};
            r.DrawMeshPose(
                spark, point, z * (.25f + pose.attack * .25f), {0, 0}, pose.phase + i, 1, color);
        }
    }
    if (pose.hurt > .05f)
    {
        r.DrawMeshPose(spark,
                       {origin.x - 10 * z, origin.y - 31 * z},
                       z * pose.hurt,
                       {0, 0},
                       .5f,
                       1,
                       {1, .5f, .25f, pose.hurt});
    }
}

void CharacterArt::Human(
    SceneRenderer& r, Point2 origin, float z, int style, int level, const ActorPose& pose)
{
    style = std::clamp(style, 0, 9);
    int tier = level >= 60 ? 2 : level >= 25 ? 1 : 0;
    const Color cloaks[] = {{.18f, .31f, .40f, 1},
                            {.24f, .37f, .28f, 1},
                            {.35f, .28f, .45f, 1},
                            {.24f, .39f, .22f, 1},
                            {.43f, .16f, .15f, 1},
                            {.52f, .37f, .17f, 1},
                            {.29f, .29f, .52f, 1},
                            {.37f, .36f, .30f, 1},
                            {.42f, .29f, .18f, 1},
                            {.22f, .42f, .40f, 1}};
    Color cloth = cloaks[style], dark{cloth.r * .5f, cloth.g * .5f, cloth.b * .5f, 1};
    Color steel{.47f, .52f, .51f, 1}, leather{.21f, .15f, .12f, 1}, skin{.69f, .54f, .39f, 1};
    Color trim = tier > 0 || style == 0 ? Color{.81f, .67f, .36f, 1} : Color{.52f, .53f, .45f, 1};
    const Point2 pivots[] = {
        {0, 0}, {0, -34}, {-6, -14}, {6, -14}, {0, -15}, {0, -34}, {-11, -30}, {11, -30}};
    float angles[] = {0,
                      pose.stride * .12f + pose.breath * .025f,
                      pose.stride * .42f,
                      -pose.stride * .42f,
                      -.12f * pose.hurt + .04f * pose.stride,
                      .035f * pose.breath - .12f * pose.hurt,
                      -.3f * pose.stride + .3f * pose.attack,
                      .3f * pose.stride - 1.1f * pose.attack + pose.work * .45f};
    for (int part = 0; part < 8; ++part)
    {
        std::string key = "human:rig2:" + std::to_string(style) + ":" + std::to_string(tier) + ":" +
                          std::to_string(part);
        if (r.BeginMesh(key, {0, 0}, 1))
        {
            switch (part)
            {
                case 0:
                    r.Ellipse({0, 1}, 17, 6, {0, 0, 0, .4f});
                    break;
                case 1:
                    r.Quad({-10, -36}, {9, -36}, {17, -3}, {-17, -3}, dark);
                    r.Triangle({-10, -34}, {-18, -2}, {-2, -7}, cloth);
                    r.Triangle({8, -34}, {18, -2}, {6, -7}, cloth);
                    r.Line({-14, -6}, {-8, -27}, 1, trim);
                    r.Line({13, -6}, {7, -26}, 1, trim);
                    r.Line({0, -32}, {-3, -9}, 1, {cloth.r, cloth.g, cloth.b, .6f});
                    break;
                case 2:
                case 3:
                {
                    float x = part == 2 ? -6.0f : 6.0f;
                    r.Line({x, -14}, {x, -4}, 6, leather);
                    r.Line({x, -4}, {x + 2, -1}, 6, dark);
                    r.Line({x - 3, -1}, {x + 5, -1}, 3, {.1f, .12f, .12f, 1});
                    r.Line({x - 2, -9}, {x + 2, -9}, 1, trim);
                    break;
                }
                case 4:
                    r.Quad({-9, -32}, {9, -32}, {10, -12}, {-10, -12}, cloth);
                    r.Quad({-6, -32}, {7, -32}, {6, -19}, {-6, -19}, steel);
                    r.Line({0, -30}, {0, -20}, 1, trim);
                    r.Line({-10, -15}, {10, -15}, 4, leather);
                    r.Rect(-2, -17, 4, 4, trim);
                    r.Line({-7, -32}, {6, -17}, 2, leather);
                    if (tier > 1)
                    {
                        r.Ellipse({0, -28}, 2, 3, {.9f, .72f, .3f, 1, 1.8f});
                    }
                    break;
                case 5:
                    r.Ellipse({0, -41}, 9, 11, dark);
                    r.Ellipse({1, -40}, 5, 7, skin);
                    r.Quad({-7, -44}, {0, -52}, {8, -44}, {3, -43}, cloth);
                    r.Line({0, -40}, {4, -40}, 1, {.1f, .12f, .12f, 1});
                    r.Line({-7, -44}, {0, -51}, 1, trim);
                    break;
                case 6:
                    r.Ellipse({-11, -31}, 6, 4, steel);
                    if (tier > 0)
                    {
                        r.Line({-15, -33}, {-7, -34}, 2, trim);
                    }
                    r.Line({-12, -27}, {-14, -19}, 5, cloth);
                    r.Ellipse({-14, -18}, 3, 3, skin);
                    if (style == 0 || style == 1)
                    {
                        r.Quad({-21, -28}, {-9, -26}, {-10, -12}, {-17, -7}, steel);
                        r.Quad({-19, -26}, {-11, -25}, {-12, -14}, {-17, -10}, dark);
                        r.Line({-15, -25}, {-15, -13}, 2, trim);
                    }
                    break;
                case 7:
                    r.Ellipse({11, -31}, 6, 4, steel);
                    if (tier > 0)
                    {
                        r.Line({7, -34}, {15, -33}, 2, trim);
                    }
                    r.Line({12, -27}, {14, -19}, 5, cloth);
                    r.Ellipse({14, -18}, 3, 3, skin);
                    if (style == 3)
                    {
                        r.Line({18, -42}, {24, -26}, 2, trim);
                        r.Line({24, -26}, {18, -7}, 2, trim);
                        r.Line({18, -42}, {18, -7}, .7f, {.72f, .73f, .66f, 1});
                        r.Line({12, -20}, {30, -20}, 1, steel);
                        r.Triangle({30, -22}, {34, -20}, {30, -18}, trim);
                    }
                    else if (style == 4 || style == 8)
                    {
                        r.Line({17, -4}, {19, -41}, 3, leather);
                        r.Quad({13, -42}, {28, -39}, {26, -31}, {16, -34}, steel);
                        r.Line({28, -39}, {26, -31}, 2, trim);
                    }
                    else if (style == 5 || style == 7 || style == 9)
                    {
                        r.Quad({7, -20}, {18, -19}, {17, -9}, {8, -9}, leather);
                        r.Line({9, -18}, {17, -17}, 2, trim);
                        if (style == 9)
                        {
                            r.Line({10, -14}, {16, -14}, 1, trim);
                            r.Line({13, -17}, {13, -11}, 1, trim);
                        }
                    }
                    else
                    {
                        r.Line({18, -2}, {18, -49}, 2, leather);
                        r.Quad({13, -47}, {18, -55}, {23, -47}, {18, -40}, trim);
                        r.Ellipse({18, -47}, 2, 4, {.48f, .79f, .93f, 1, 2.6f});
                        r.Line({16, -37}, {20, -37}, 2, trim);
                    }
                    break;
            }
            r.EndMesh();
        }
        Point2 point = origin;
        if (part > 0)
        {
            point.x -= pose.facing * pose.hurt * 3 * z;
            point.y += (pose.breath * .5f - std::abs(pose.stride) * 1.5f) * z;
        }
        Color tint{1, 1 - pose.hurt * .35f, 1 - pose.hurt * .5f, 1, 1 + pose.hurt * .6f};
        r.DrawMeshPose(key,
                       point,
                       z,
                       pivots[part],
                       angles[part],
                       pose.facing,
                       part == 0 ? Color{1, 1, 1, 1} : tint);
    }
    Aura(r, origin, z, pose, {.55f, .82f, 1, 1});
}
