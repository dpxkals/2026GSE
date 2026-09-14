#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <utility>

using WorldInt = std::int64_t;
using ChunkKey = std::pair<WorldInt, WorldInt>;
constexpr int ChunkSize = 16;

enum class Ground
{
    Grass,
    Dirt,
    Stone,
    Water
};
enum class Prop
{
    None,
    Tree,
    Rock,
    Ruin,
    Beacon
};

struct Tile
{
    Ground ground = Ground::Grass;
    Prop prop = Prop::None;
    int variation = 0;
    bool lit = false;
};

struct Chunk
{
    std::array<Tile, ChunkSize * ChunkSize> tiles;
    int generationVersion = 3;
};

class World
{
  public:
    explicit World(const std::filesystem::path& directory);
    void Stream(double x, double y, int radius);
    const Tile* Find(WorldInt x, WorldInt y) const;
    bool CanWalk(double x, double y) const;
    std::string LightNearest(double x, double y);

    const std::map<ChunkKey, Chunk>& Chunks() const
    {
        return chunks_;
    }

    const std::string& Error() const
    {
        return error_;
    }

    int Pending() const
    {
        return pending_;
    }

    std::uint64_t Generated() const
    {
        return generated_;
    }

    std::uint64_t Loaded() const
    {
        return loaded_;
    }

    static WorldInt ChunkOf(WorldInt tile);
    std::uint64_t Hash(WorldInt x, WorldInt y) const;
    std::uint64_t Seed() const;
    bool ClearLine(double fromX, double fromY, double toX, double toY) const;

  private:
    Chunk Generate(ChunkKey key) const;
    double Noise(double x, double y) const;
    bool Read(ChunkKey key, Chunk& chunk);
    bool Save(ChunkKey key, const Chunk& chunk);
    std::filesystem::path Path(ChunkKey key) const;
    std::filesystem::path directory_;
    std::uint64_t seed_ = 0;
    std::map<ChunkKey, Chunk> chunks_;
    std::string error_;
    int pending_ = 0;
    std::uint64_t generated_ = 0, loaded_ = 0;
};
