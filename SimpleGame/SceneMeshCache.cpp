#include "stdafx.h"
#include "SceneRenderer.h"
#include "FrameProfiler.h"
#include <Windows.h>
#include <fstream>
#include <cmath>
#include <sstream>
#include <cstring>

namespace
{
    // Bump this when authored geometry, vertex layout or generation rules change.
    constexpr std::uint32_t CacheVersion = 2;
    constexpr std::uint64_t MaxVertices = 1000000;

    std::uint64_t HashBytes(const void* data, size_t length)
    {
        auto bytes = static_cast<const unsigned char*>(data);
        std::uint64_t hash = 14695981039346656037ull;
        for (size_t i = 0; i < length; ++i)
        {
            hash = (hash ^ bytes[i]) * 1099511628211ull;
        }
        return hash;
    }

    template <class T> bool Read(std::istream& stream, T& value)
    {
        return bool(stream.read(reinterpret_cast<char*>(&value), sizeof(value)));
    }

    template <class T> void Write(std::ostream& stream, const T& value)
    {
        stream.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }
}

void SceneRenderer::SetMeshCacheDirectory(const std::filesystem::path& directory)
{
    cacheDirectory_ = directory / ("geometry-v" + std::to_string(CacheVersion));
    std::error_code error;
    std::filesystem::create_directories(cacheDirectory_, error);
    if (error)
    {
        std::fprintf(stderr, "[MeshCache] Directory unavailable; using memory cache only.\n");
        cacheDirectory_.clear();
    }
}

std::filesystem::path SceneRenderer::CachePath(const std::string& key) const
{
    std::ostringstream filename;
    filename << std::hex << HashBytes(key.data(), key.size()) << ".mesh";
    return cacheDirectory_ / filename.str();
}

bool SceneRenderer::LoadMesh(const std::string& key, Mesh& mesh)
{
    if (cacheDirectory_.empty())
    {
        return false;
    }
    FrameProfiler::Scope timer(FrameProfiler::CacheIoMs);
    auto& profile = FrameProfiler::Instance();
    std::ifstream input(CachePath(key), std::ios::binary);
    if (!input)
    {
        return false;
    }
    auto reject = [&]()
    {
        profile.Add(FrameProfiler::CacheErrors);
        profile.Event("invalid", key);
        std::fprintf(stderr, "[MeshCache] Invalid cache, rebuilding: %s\n", key.c_str());
        return false;
    };
    std::uint32_t version = 0, stride = 0, keyLength = 0;
    std::uint64_t count = 0, checksum = 0;
    char magic[8]{};
    input.read(magic, sizeof(magic));
    if (std::memcmp(magic, "GSEMESH1", 8) != 0 || !Read(input, version) || !Read(input, stride) ||
        !Read(input, keyLength) || !Read(input, count) || !Read(input, checksum) ||
        version != CacheVersion || stride != sizeof(Vertex) || keyLength != key.size() ||
        keyLength > 4096 || count == 0 || count > MaxVertices || count % 3 != 0)
    {
        return reject();
    }
    std::string storedKey(keyLength, '\0');
    if (!input.read(storedKey.data(), keyLength) || storedKey != key)
    {
        return reject();
    }
    std::vector<Vertex> vertices(static_cast<size_t>(count));
    auto bytes = vertices.size() * sizeof(Vertex);
    if (!input.read(reinterpret_cast<char*>(vertices.data()), bytes) ||
        input.peek() != std::char_traits<char>::eof() ||
        HashBytes(vertices.data(), bytes) != checksum)
    {
        return reject();
    }
    for (const auto& vertex : vertices)
    {
        for (float value : {vertex.x,
                            vertex.y,
                            vertex.r,
                            vertex.g,
                            vertex.b,
                            vertex.a,
                            vertex.u,
                            vertex.v,
                            vertex.energy})
        {
            if (!std::isfinite(value))
            {
                return reject();
            }
        }
    }
    mesh.vertices = std::move(vertices);
    profile.Add(FrameProfiler::DiskHits);
    profile.Event("disk_hit", key, mesh.vertices.size(), bytes);
    profile.Add(FrameProfiler::DiskReadBytes, double(bytes + keyLength + 36));
    return true;
}

void SceneRenderer::SaveMesh(const std::string& key, const Mesh& mesh)
{
    if (cacheDirectory_.empty() || mesh.vertices.empty() || mesh.vertices.size() > MaxVertices ||
        key.size() > 4096)
    {
        return;
    }
    static_assert(sizeof(Vertex) == 9 * sizeof(float), "Disk vertex layout changed");
    FrameProfiler::Scope timer(FrameProfiler::CacheIoMs);
    auto& profile = FrameProfiler::Instance();
    auto destination = CachePath(key);
    auto temporary = destination;
    temporary += "." + std::to_string(GetCurrentProcessId()) + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    auto bytes = mesh.vertices.size() * sizeof(Vertex);
    output.write("GSEMESH1", 8);
    Write(output, CacheVersion);
    Write(output, static_cast<std::uint32_t>(sizeof(Vertex)));
    Write(output, static_cast<std::uint32_t>(key.size()));
    Write(output, static_cast<std::uint64_t>(mesh.vertices.size()));
    Write(output, HashBytes(mesh.vertices.data(), bytes));
    output.write(key.data(), key.size());
    output.write(reinterpret_cast<const char*>(mesh.vertices.data()), bytes);
    output.close();
    if (!output || !MoveFileExW(temporary.c_str(), destination.c_str(), MOVEFILE_REPLACE_EXISTING))
    {
        profile.Add(FrameProfiler::CacheErrors);
        profile.Event("write_failed", key, mesh.vertices.size(), bytes);
        std::fprintf(
            stderr, "[MeshCache] Save failed; memory cache remains valid: %s\n", key.c_str());
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return;
    }
    profile.Add(FrameProfiler::DiskWriteBytes, double(bytes + key.size() + 36));
    profile.Event("disk_write", key, mesh.vertices.size(), bytes);
}
