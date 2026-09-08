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

namespace
{
    std::uint64_t Mix(std::uint64_t v)
    {
        v = (v ^ (v >> 30)) * 0xbf58476d1ce4e5b9ULL;
        v = (v ^ (v >> 27)) * 0x94d049bb133111ebULL;
        return v ^ (v >> 31);
    }

    double Noise(double x, double y)
    {
        auto ix = static_cast<WorldInt>(std::floor(x));
        auto iy = static_cast<WorldInt>(std::floor(y));
        double fx = x-ix, fy = y-iy;
        fx = fx*fx*(3-2*fx); fy = fy*fy*(3-2*fy);
        auto sample = [](WorldInt a, WorldInt b) {
            return (World::Hash(a,b) & 65535)/65535.0;
        };
        double a = sample(ix,iy)*(1-fx)+sample(ix+1,iy)*fx;
        double b = sample(ix,iy+1)*(1-fx)+sample(ix+1,iy+1)*fx;
        return a*(1-fy)+b*fy;
    }
}

WorldInt World::ChunkOf(WorldInt tile)
{
    // C++ division truncates toward zero; a negative partial chunk needs floor.
    WorldInt q = tile/ChunkSize;
    return tile%ChunkSize < 0 ? q-1 : q;
}

std::uint64_t World::Hash(WorldInt x, WorldInt y)
{
    return Mix(static_cast<std::uint64_t>(x) ^
        Mix(static_cast<std::uint64_t>(y)+WorldSeed));
}

World::World(const std::filesystem::path& directory) : directory_(directory)
{
    try { std::filesystem::create_directories(directory_); }
    catch (const std::exception& e) { error_ = std::string("SAVE DIRECTORY: ")+e.what(); }
}

std::filesystem::path World::Path(ChunkKey key) const
{
    return directory_ / (std::to_string(key.first)+"_"+std::to_string(key.second)+".chunk");
}

Chunk World::Generate(ChunkKey key) const
{
    Chunk chunk;
    for (int y=0; y<ChunkSize; ++y) for (int x=0; x<ChunkSize; ++x)
    {
        WorldInt wx = key.first*ChunkSize+x, wy = key.second*ChunkSize+y;
        Tile& tile = chunk.tiles[y*ChunkSize+x];
        const auto hash = Hash(wx,wy);
        tile.variation = int(hash%7);
        double moisture = Noise(wx/18.0,wy/18.0);
        bool road = std::abs(double(wy)-3.0*std::sin(wx*0.065)) < 1.3;
        bool clearing = std::abs(wx) <= 4 && std::abs(wy) <= 4;
        tile.ground = moisture < 0.26 ? Ground::Water : Ground::Grass;
        if (moisture > 0.76) tile.ground = Ground::Stone;
        if (road || clearing) tile.ground = Ground::Dirt;
        if (!road && !clearing && tile.ground != Ground::Water)
        {
            auto roll = (hash >> 16)%100;
            if (roll < 17) tile.prop = Prop::Tree;
            else if (roll < 22) tile.prop = Prop::Rock;
            else if (roll < 24) tile.prop = Prop::Ruin;
            // At most one beacon candidate in each chunk; local cross is cleared below.
            if (x == 8 && y == 8 && Hash(key.first,key.second)%3 == 0)
                tile.prop = Prop::Beacon;
        }
        if (wx == 2 && wy == 1) tile.prop = Prop::Beacon;
        if (wx == -3 && wy == -2) tile.prop = Prop::Ruin;
    }
    for (int y=1; y<ChunkSize-1; ++y) for (int x=1; x<ChunkSize-1; ++x)
        if (chunk.tiles[y*ChunkSize+x].prop == Prop::Beacon)
            for (auto offset : {std::pair<int,int>{-1,0},{1,0},{0,-1},{0,1}})
            {
                Tile& neighbor = chunk.tiles[(y+offset.second)*ChunkSize+x+offset.first];
                neighbor.ground = Ground::Dirt;
                neighbor.prop = Prop::None;
            }
    return chunk;
}

bool World::Save(ChunkKey key, const Chunk& chunk)
{
    try
    {
        const auto destination = Path(key);
        auto temporary = destination; temporary += L".tmp";
        std::ofstream out(temporary, std::ios::trunc);
        out << "GSE_CHUNK 1 " << WorldSeed << ' ' << key.first << ' ' << key.second << '\n';
        for (const Tile& tile : chunk.tiles)
            out << int(tile.ground) << ' ' << int(tile.prop) << ' '
                << tile.variation << ' ' << tile.lit << '\n';
        out.flush();
        if (!out) throw std::runtime_error("Cannot write chunk");
        out.close();
        if (!out) throw std::runtime_error("Cannot close chunk");
        // The previous file remains valid until a complete replacement is ready.
        if (!MoveFileExW(temporary.c_str(), destination.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot commit chunk file");
        return true;
    }
    catch (const std::exception& e)
    {
        error_ = "SAVE ERROR "+std::to_string(key.first)+":"+std::to_string(key.second)+" "+e.what();
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
            if (!Save(key,chunk)) return false;
            ++generated_;
            return true;
        }
        std::ifstream in(Path(key));
        std::string magic;
        int version = 0;
        std::uint64_t seed = 0;
        WorldInt x = 0, y = 0;
        if (!(in >> magic >> version >> seed >> x >> y) || magic != "GSE_CHUNK" ||
            version != 1 || seed != WorldSeed || x != key.first || y != key.second)
            throw std::runtime_error("Invalid chunk header; original file preserved");
        for (Tile& tile : chunk.tiles)
        {
            int ground, prop, variation, lit;
            if (!(in >> ground >> prop >> variation >> lit) || ground < 0 || ground > 3 ||
                prop < 0 || prop > 4 || variation < 0 || variation > 6 || lit < 0 || lit > 1)
                throw std::runtime_error("Invalid chunk data; original file preserved");
            tile = {static_cast<Ground>(ground),static_cast<Prop>(prop),variation,lit != 0};
        }
        ++loaded_;
        return true;
    }
    catch (const std::exception& e) { error_ = std::string("LOAD ERROR: ")+e.what(); return false; }
}

void World::Stream(double x, double y, int radius)
{
    if (!error_.empty()) return;
    ChunkKey center{ChunkOf(static_cast<WorldInt>(std::floor(x))),
        ChunkOf(static_cast<WorldInt>(std::floor(y)))};
    std::vector<ChunkKey> missing;
    for (int dy=-radius; dy<=radius; ++dy) for (int dx=-radius; dx<=radius; ++dx)
    {
        ChunkKey key{center.first+dx,center.second+dy};
        if (chunks_.find(key) == chunks_.end()) missing.push_back(key);
    }
    std::sort(missing.begin(),missing.end(),[center](ChunkKey a, ChunkKey b) {
        auto distance = [center](ChunkKey key) {
            auto dx = key.first-center.first, dy = key.second-center.second;
            return dx*dx+dy*dy;
        };
        auto da=distance(a), db=distance(b);
        return da == db ? a < b : da < db;
    });
    // Bounded synchronous I/O for the first prototype. Worker streaming is a later step.
    int budget = (std::min)(int(missing.size()), 3);
    for (int i=0; i<budget; ++i)
    {
        Chunk chunk;
        if (!Read(missing[i],chunk)) return;
        chunks_.emplace(missing[i],std::move(chunk));
    }
    pending_ = int(missing.size())-budget;
    for (auto it=chunks_.begin(); it!=chunks_.end(); )
    {
        if (std::abs(it->first.first-center.first) > radius+1 ||
            std::abs(it->first.second-center.second) > radius+1)
            it = chunks_.erase(it);
        else ++it;
    }
}

const Tile* World::Find(WorldInt x, WorldInt y) const
{
    ChunkKey key{ChunkOf(x),ChunkOf(y)};
    auto it = chunks_.find(key);
    if (it == chunks_.end()) return nullptr;
    int lx = int(x-key.first*ChunkSize), ly = int(y-key.second*ChunkSize);
    return &it->second.tiles[ly*ChunkSize+lx];
}

bool World::CanWalk(double x, double y) const
{
    // Circle against blocked tile AABBs, including unavailable chunks.
    constexpr double radius = 0.19;
    WorldInt bx = static_cast<WorldInt>(std::floor(x));
    WorldInt by = static_cast<WorldInt>(std::floor(y));
    for (WorldInt ty=by-1; ty<=by+1; ++ty) for (WorldInt tx=bx-1; tx<=bx+1; ++tx)
    {
        const Tile* tile = Find(tx,ty);
        if (tile && tile->ground != Ground::Water && tile->prop == Prop::None) continue;
        double nx = (std::max)(double(tx),(std::min)(x,double(tx+1)));
        double ny = (std::max)(double(ty),(std::min)(y,double(ty+1)));
        double dx=x-nx, dy=y-ny;
        if (dx*dx+dy*dy < radius*radius) return false;
    }
    return true;
}

std::string World::LightNearest(double x, double y)
{
    if (!error_.empty()) return "SAVE UNAVAILABLE - SEE CONSOLE";
    double nearest = 2.4*2.4;
    Tile* selected = nullptr;
    ChunkKey selectedChunk{};
    for (WorldInt ty=WorldInt(std::floor(y))-3; ty<=WorldInt(std::floor(y))+3; ++ty)
        for (WorldInt tx=WorldInt(std::floor(x))-3; tx<=WorldInt(std::floor(x))+3; ++tx)
        {
            ChunkKey key{ChunkOf(tx),ChunkOf(ty)};
            auto it = chunks_.find(key);
            if (it == chunks_.end()) continue;
            auto& tile = it->second.tiles[int(ty-key.second*ChunkSize)*ChunkSize+int(tx-key.first*ChunkSize)];
            double dx=tx+0.5-x, dy=ty+0.5-y, distance=dx*dx+dy*dy;
            if (tile.prop == Prop::Beacon && distance < nearest)
            {
                nearest=distance; selected=&tile; selectedChunk=key;
            }
        }
    if (!selected) return "NO BEACON NEARBY";
    if (selected->lit) return "THIS FLAME REMEMBERS YOU";
    selected->lit = true;
    if (!Save(selectedChunk,chunks_.at(selectedChunk)))
    {
        selected->lit = false;
        return "SAVE FAILED - FLAME NOT CHANGED";
    }
    return "A LITTLE HOPE REMAINS - SAVED";
}
