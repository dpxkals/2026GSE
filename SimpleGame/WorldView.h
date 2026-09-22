#pragma once

#include "SceneRenderer.h"
#include "World.h"

class LevelOne;
class NpcSystem;

namespace WorldView
{
    void Structure(SceneRenderer& renderer, Point2 origin, float zoom, Prop prop, int variation);
    void MiniMap(SceneRenderer& renderer,
                 const World& world,
                 const LevelOne& level,
                 const NpcSystem& npcs,
                 int width,
                 int height,
                 double dt);
}
