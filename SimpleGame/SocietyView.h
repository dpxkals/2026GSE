#pragma once
#include "SceneRenderer.h"
#include "WorldSimulation.h"

namespace SocietyView
{
    void CitizenBody(
        SceneRenderer& renderer, const Citizen& npc, Point2 p, float zoom, bool selected);
    void Panel(SceneRenderer& renderer, const WorldSimulation& simulation, int width, int height);
}
