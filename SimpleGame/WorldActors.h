#pragma once

#include "Actor.h"
#include "World.h"

// Immutable source coordinates identify persisted terrain. The Actor transform is
// its scene placement; rendering never holds pointers into streamed chunk storage.
class TerrainActor : public Actor
{
  public:
    explicit TerrainActor(ChunkKey chunk);
    ChunkKey Source() const;

  private:
    ChunkKey source_;
};

class PropActor : public Actor
{
  public:
    PropActor(WorldInt tileX, WorldInt tileY);
    ChunkKey Source() const;

  private:
    ChunkKey source_;
};
