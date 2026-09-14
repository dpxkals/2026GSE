#include "stdafx.h"
#include "World.h"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <random>

namespace
{
    std::uint64_t Mix(std::uint64_t v)
    {
        v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ULL;
        v = (v ^ (v >> 27)) * 0x94d049bb133111ebULL;
        return v ^ (v >> 31);
    }

}

double World::Noise(double x, double y) const
{
    auto ix = static_cast<WorldInt>(std::floor(x));
    auto iy = static_cast<WorldInt>(std::floor(y));
    double fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    auto sample = [this](WorldInt a, WorldInt b)
    {
        return (Hash(a, b) & 65535) / 65535.0;
    };
    double a = sample(ix, iy) * (1 - fx) + sample(ix + 1, iy) * fx;
    double b = sample(ix, iy + 1) * (1 - fx) + sample(ix + 1, iy + 1) * fx;
    return a * (1 - fy) + b * fy;
}

WorldInt World::ChunkOf(WorldInt tile)
{
    // C++ division truncates toward zero; a negative partial chunk needs floor.
    WorldInt q = tile / ChunkSize;
    return tile % ChunkSize < 0 ? q - 1 : q;
}

std::uint64_t World::Hash(WorldInt x, WorldInt y) const
{
    return Mix(static_cast<std::uint64_t>(x) ^ Mix(static_cast<std::uint64_t>(y) + seed_));
}

std::uint64_t World::Seed() const
{
    return seed_;
}

World::World(const std::filesystem::path& directory)
    : directory_(directory)
{
    try
    {
        std::filesystem::create_directories(directory_);
        auto manifest = directory_ / L"world.meta";
        if (std::filesystem::exists(manifest))
        {
            std::ifstream in(manifest);
            std::string magic;
            int version = 0;
            if (!(in >> magic >> version >> seed_) || magic != "GSE_FARM_WORLD" || version != 1)
            {
                throw std::runtime_error("Invalid world manifest; original preserved");
            }
        }
        else
        {
            // Never invent another seed for a world whose manifest has been lost.
            for (const auto& entry : std::filesystem::directory_iterator(directory_))
            {
                if (entry.path().extension() == L".chunk" ||
                    entry.path().filename() == L"level_one.dat")
                {
                    throw std::runtime_error("Existing world is missing its seed manifest");
                }
            }

            std::random_device entropy;
            seed_ = (static_cast<std::uint64_t>(entropy()) << 32) ^ entropy();
            auto temporary = manifest;
            temporary += L".tmp";
            std::ofstream out(temporary, std::ios::trunc);
            out << "GSE_FARM_WORLD 1 " << seed_ << '\n';
            out.close();
            if (!out || !MoveFileExW(temporary.c_str(), manifest.c_str(), MOVEFILE_WRITE_THROUGH))
            {
                throw std::runtime_error("Cannot save world seed");
            }
        }
    }
    catch (const std::exception& e)
    {
        error_ = std::string("SAVE DIRECTORY: ") + e.what();
    }
}

std::filesystem::path World::Path(ChunkKey key) const
{
    return directory_ / (std::to_string(key.first) + "_" + std::to_string(key.second) + ".chunk");
}

Chunk World::Generate(ChunkKey key) const
{
    Chunk chunk;
    // Ecology patches are world-space, independent of the 16x16 streaming grid.
    // Solid, convex obstacle islands have a walkable grass margin. No visible
    // boundary road or per-chunk cross is needed to keep the outdoors connected.
    constexpr WorldInt patchSize = 11;
    auto patchOf = [patchSize](WorldInt value)
    {
        WorldInt q = value / patchSize;
        return value % patchSize < 0 ? q - 1 : q;
    };
    for (int y = 0; y < ChunkSize; ++y)
    {
        for (int x = 0; x < ChunkSize; ++x)
        {
            WorldInt wx = key.first * ChunkSize + x;
            WorldInt wy = key.second * ChunkSize + y;
            Tile& tile = chunk.tiles[y * ChunkSize + x];
            auto hash = Hash(wx, wy);
            tile.variation = int(hash % 7);
            double moisture = Noise(wx / 23.0, wy / 23.0);
            tile.ground = moisture > 0.77 ? Ground::Stone : Ground::Grass;

            // Winding trails are sampled in global coordinates, so their width,
            // bends and forks continue through every chunk boundary unchanged.
            double mainTrail = 6.0 * std::sin(wx * 0.032) + 5.0 * (Noise(wx / 38.0, 17.0) - 0.5);
            double sideTrail = 7.0 * std::sin(wy * 0.024) + 5.0 * (Noise(31.0, wy / 44.0) - 0.5);
            double trailWidth = 1.05 + Noise(wx / 15.0, wy / 15.0) * 0.7;
            bool trail = std::abs(wy - mainTrail) < trailWidth ||
                         std::abs(wx - sideTrail) < trailWidth * 0.8;
            bool camp = std::hypot(double(wx), double(wy)) < 5.3;

            WorldInt px = patchOf(wx), py = patchOf(wy);
            auto patchHash = Hash(px + 7123, py - 8191);
            int centerX = 3 + int(patchHash % 5);
            int centerY = 3 + int((patchHash >> 8) % 5);
            double patchMoisture =
                Noise((px * patchSize + centerX) / 23.0, (py * patchSize + centerY) / 23.0);
            int radiusX = 1 + int((patchHash >> 16) % 3);
            int radiusY = 1 + int((patchHash >> 24) % 3);
            radiusX = (std::min)(radiusX, (std::min)(centerX - 1, 9 - centerX));
            radiusY = (std::min)(radiusY, (std::min)(centerY - 1, 9 - centerY));
            int lx = int(wx - px * patchSize), ly = int(wy - py * patchSize);
            double dx = double(lx - centerX) / (radiusX + 0.25);
            double dy = double(ly - centerY) / (radiusY + 0.25);
            bool island = dx * dx + dy * dy <= 1.0;
            int kind = int((patchHash >> 32) % 12);
            if (island && !trail && !camp)
            {
                if (kind < 3)
                {
                    tile.ground = Ground::Water;
                }
                else if (kind < 9 && patchMoisture > 0.35)
                {
                    // Filled clusters cannot enclose a walkable pocket.
                    tile.prop = hash % 11 == 0 ? Prop::Rock : Prop::Tree;
                }
                else if (kind == 9 && lx == centerX && ly == centerY)
                {
                    tile.prop = Prop::Ruin;
                }
                else if (kind == 10 && lx == centerX && ly == centerY)
                {
                    tile.prop = Prop::Beacon;
                }
            }
            if (trail || camp)
            {
                tile.ground = Ground::Dirt;
                tile.prop = Prop::None;
            }
            if (wx == 2 && wy == 1)
            {
                tile.prop = Prop::Beacon;
            }
            if (wx == -3 && wy == -2)
            {
                tile.prop = Prop::Ruin;
            }
        }
    }
    return chunk;
}

bool World::Save(ChunkKey key, const Chunk& chunk)
{
    try
    {
        const auto destination = Path(key);
        auto temporary = destination;
        temporary += L".tmp";
        std::ofstream out(temporary, std::ios::trunc);
        out << "GSE_CHUNK " << chunk.generationVersion << ' ' << seed_ << ' ' << key.first << ' '
            << key.second << '\n';
        for (const Tile& tile : chunk.tiles)
        {
            out << int(tile.ground) << ' ' << int(tile.prop) << ' ' << tile.variation << ' '
                << tile.lit << '\n';
        }
        out.flush();
        if (!out)
        {
            throw std::runtime_error("Cannot write chunk");
        }
        out.close();
        if (!out)
        {
            throw std::runtime_error("Cannot close chunk");
        }
        // The previous file remains valid until a complete replacement is ready.
        if (!MoveFileExW(temporary.c_str(),
                         destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        {
            throw std::runtime_error("Cannot commit chunk file");
        }
        return true;
    }
    catch (const std::exception& e)
    {
        error_ = "SAVE ERROR " + std::to_string(key.first) + ":" + std::to_string(key.second) +
                 " " + e.what();
        return false;
    }
}

bool World::Read(ChunkKey key, Chunk& chunk)
{
    try
    {
        if (!std::filesystem::exists(Path(key)))
        {
            chunk = Generate(key);
            if (!Save(key, chunk))
            {
                return false;
            }
            ++generated_;
            return true;
        }
        std::ifstream in(Path(key));
        std::string magic;
        int version = 0;
        std::uint64_t seed = 0;
        WorldInt x = 0, y = 0;
        if (!(in >> magic >> version >> seed >> x >> y) || magic != "GSE_CHUNK" ||
            (version != 2 && version != 3) || seed != seed_ || x != key.first || y != key.second)
        {
            throw std::runtime_error("Invalid chunk header; original file preserved");
        }
        chunk.generationVersion = version;
        for (Tile& tile : chunk.tiles)
        {
            int ground, prop, variation, lit;
            if (!(in >> ground >> prop >> variation >> lit) || ground < 0 || ground > 3 ||
                prop < 0 || prop > 4 || variation < 0 || variation > 6 || lit < 0 || lit > 1)
            {
                throw std::runtime_error("Invalid chunk data; original file preserved");
            }
            tile = {static_cast<Ground>(ground), static_cast<Prop>(prop), variation, lit != 0};
        }
        ++loaded_;
        return true;
    }
    catch (const std::exception& e)
    {
        error_ = std::string("LOAD ERROR: ") + e.what();
        return false;
    }
}

void World::Stream(double x, double y, int radius)
{
    if (!error_.empty())
    {
        return;
    }
    ChunkKey center{ChunkOf(static_cast<WorldInt>(std::floor(x))),
                    ChunkOf(static_cast<WorldInt>(std::floor(y)))};
    std::vector<ChunkKey> missing;
    for (int dy = -radius; dy <= radius; ++dy)
    {
        for (int dx = -radius; dx <= radius; ++dx)
        {
            ChunkKey key{center.first + dx, center.second + dy};
            if (chunks_.find(key) == chunks_.end())
            {
                missing.push_back(key);
            }
        }
    }
    std::sort(missing.begin(),
              missing.end(),
              [center](ChunkKey a, ChunkKey b)
              {
                  auto distance = [center](ChunkKey key)
                  {
                      auto dx = key.first - center.first, dy = key.second - center.second;
                      return dx * dx + dy * dy;
                  };
                  auto da = distance(a), db = distance(b);
                  return da == db ? a < b : da < db;
              });
    // Bounded synchronous I/O for the first prototype. Worker streaming is a later step.
    int budget = (std::min)(int(missing.size()), 3);
    for (int i = 0; i < budget; ++i)
    {
        Chunk chunk;
        if (!Read(missing[i], chunk))
        {
            return;
        }
        chunks_.emplace(missing[i], std::move(chunk));
    }
    pending_ = int(missing.size()) - budget;
    for (auto it = chunks_.begin(); it != chunks_.end();)
    {
        if (std::abs(it->first.first - center.first) > radius + 1 ||
            std::abs(it->first.second - center.second) > radius + 1)
        {
            it = chunks_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

const Tile* World::Find(WorldInt x, WorldInt y) const
{
    ChunkKey key{ChunkOf(x), ChunkOf(y)};
    auto it = chunks_.find(key);
    if (it == chunks_.end())
    {
        return nullptr;
    }
    int lx = int(x - key.first * ChunkSize), ly = int(y - key.second * ChunkSize);
    return &it->second.tiles[ly * ChunkSize + lx];
}

bool World::CanWalk(double x, double y) const
{
    // Circle against blocked tile AABBs, including unavailable chunks.
    constexpr double radius = 0.19;
    WorldInt bx = static_cast<WorldInt>(std::floor(x));
    WorldInt by = static_cast<WorldInt>(std::floor(y));
    for (WorldInt ty = by - 1; ty <= by + 1; ++ty)
    {
        for (WorldInt tx = bx - 1; tx <= bx + 1; ++tx)
        {
            const Tile* tile = Find(tx, ty);
            if (tile && tile->ground != Ground::Water && tile->prop == Prop::None)
            {
                continue;
            }
            double nx = (std::max)(double(tx), (std::min)(x, double(tx + 1)));
            double ny = (std::max)(double(ty), (std::min)(y, double(ty + 1)));
            double dx = x - nx, dy = y - ny;
            if (dx * dx + dy * dy < radius * radius)
            {
                return false;
            }
        }
    }
    return true;
}

bool World::ClearLine(double fromX, double fromY, double toX, double toY) const
{
    double dx = toX - fromX;
    double dy = toY - fromY;
    int steps = (std::max)(1, int(std::ceil(std::hypot(dx, dy) / 0.12)));
    for (int step = 0; step <= steps; ++step)
    {
        double t = double(step) / steps;
        if (!CanWalk(fromX + dx * t, fromY + dy * t))
        {
            return false;
        }
    }
    return true;
}

std::string World::LightNearest(double x, double y)
{
    if (!error_.empty())
    {
        return "SAVE UNAVAILABLE - SEE CONSOLE";
    }
    double nearest = 2.4 * 2.4;
    Tile* selected = nullptr;
    ChunkKey selectedChunk{};
    for (WorldInt ty = WorldInt(std::floor(y)) - 3; ty <= WorldInt(std::floor(y)) + 3; ++ty)
    {
        for (WorldInt tx = WorldInt(std::floor(x)) - 3; tx <= WorldInt(std::floor(x)) + 3; ++tx)
        {
            ChunkKey key{ChunkOf(tx), ChunkOf(ty)};
            auto it = chunks_.find(key);
            if (it == chunks_.end())
            {
                continue;
            }
            auto& tile = it->second.tiles[int(ty - key.second * ChunkSize) * ChunkSize +
                                          int(tx - key.first * ChunkSize)];
            double dx = tx + 0.5 - x, dy = ty + 0.5 - y, distance = dx * dx + dy * dy;
            if (tile.prop == Prop::Beacon && distance < nearest)
            {
                nearest = distance;
                selected = &tile;
                selectedChunk = key;
            }
        }
    }
    if (!selected)
    {
        return "NO BEACON NEARBY";
    }
    if (selected->lit)
    {
        return "THIS FLAME REMEMBERS YOU";
    }
    selected->lit = true;
    if (!Save(selectedChunk, chunks_.at(selectedChunk)))
    {
        selected->lit = false;
        return "SAVE FAILED - FLAME NOT CHANGED";
    }
    return "A LITTLE HOPE REMAINS - SAVED";
}
