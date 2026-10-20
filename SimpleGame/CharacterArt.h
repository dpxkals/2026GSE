#pragma once
#include "SceneRenderer.h"
#include "Actor.h"

namespace CharacterArt
{
    // 0 traveler, 1 warden, 2 scholar, 3..9 experience-derived citizen appearances.
    void Human(SceneRenderer& renderer,
               Point2 origin,
               float zoom,
               int style,
               int level,
               const ActorPose& pose);
    void Aura(
        SceneRenderer& renderer, Point2 origin, float zoom, const ActorPose& pose, Color color);
}
