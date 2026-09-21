#include "stdafx.h"
#include "WorldActors.h"

TerrainActor::TerrainActor(ChunkKey chunk)
    : Actor("terrain/" + std::to_string(chunk.first) + "/" + std::to_string(chunk.second),
            "terrain"),
      source_(chunk)
{
    layer = RenderLayer::Ground;
}

ChunkKey TerrainActor::Source() const
{
    return source_;
}

PropActor::PropActor(WorldInt tileX, WorldInt tileY)
    : Actor("prop/" + std::to_string(tileX) + "/" + std::to_string(tileY), "prop"),
      source_(tileX, tileY)
{
}

ChunkKey PropActor::Source() const
{
    return source_;
}
