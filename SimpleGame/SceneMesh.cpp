#include "stdafx.h"
#include "FrameProfiler.h"
#include "SceneRenderer.h"
#include <algorithm>
#include <cmath>
#include <iostream>

bool SceneRenderer::BeginMesh(const std::string& key, Point2 origin, float scale)
{
    auto& profile = FrameProfiler::Instance();
    profile.Add(FrameProfiler::MeshRequests);
    if (meshes_.find(key) != meshes_.end())
    {
        profile.Add(FrameProfiler::MemoryHits);
        return false;
    }
    if (!recordingKey_.empty() || key.empty() || !std::isfinite(scale) || scale <= 0.0f)
    {
        profile.Add(FrameProfiler::CacheErrors);
        return false;
    }
    Mesh cached;
    if (LoadMesh(key, cached))
    {
        meshes_.emplace(key, std::move(cached));
        return false;
    }
    profile.Add(FrameProfiler::CacheMisses);
    profile.Event("miss", key);
    Flush();
    recordingKey_ = key;
    recordingOrigin_ = origin;
    recordingScale_ = scale;
    meshBuildStart_ = std::chrono::steady_clock::now();
    return true;
}

void SceneRenderer::EndMesh()
{
    if (recordingKey_.empty())
    {
        return;
    }
    Mesh mesh;
    mesh.vertices = std::move(vertices_);
    for (auto& vertex : mesh.vertices)
    {
        vertex.x = (vertex.x - recordingOrigin_.x) / recordingScale_;
        vertex.y = (vertex.y - recordingOrigin_.y) / recordingScale_;
    }
    auto& profile = FrameProfiler::Instance();
    profile.Add(FrameProfiler::MeshBuilds);
    profile.Event(
        "build", recordingKey_, mesh.vertices.size(), mesh.vertices.size() * sizeof(Vertex));
    profile.Add(FrameProfiler::MeshBuildMs,
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() -
                                                          meshBuildStart_)
                    .count());
    SaveMesh(recordingKey_, mesh);
    meshes_.emplace(recordingKey_, std::move(mesh));
    recordingKey_.clear();
    vertices_.clear();
}

void SceneRenderer::DrawMesh(const std::string& key, Point2 origin, float scale, float opacity)
{
    QueueMesh(key, {origin.x, origin.y, scale, scale, 1, 1, 1, opacity, 1});
}

bool SceneRenderer::MeshCovers(const std::string& key,
                               Point2 origin,
                               float scale,
                               Point2 point) const
{
    auto found = meshes_.find(key);
    if (found == meshes_.end())
    {
        return false;
    }
    point = {(point.x - origin.x) / scale, (point.y - origin.y) / scale};
    const auto& vertices = found->second.vertices;
    auto side = [point](const Vertex& a, const Vertex& b)
    {
        return (b.x - a.x) * (point.y - a.y) - (b.y - a.y) * (point.x - a.x);
    };
    for (size_t i = 0; i + 2 < vertices.size(); i += 3)
    {
        if (vertices[i].a < 0.7f)
        {
            continue;
        }
        float a = side(vertices[i], vertices[i + 1]);
        float b = side(vertices[i + 1], vertices[i + 2]);
        float c = side(vertices[i + 2], vertices[i]);
        if ((a >= 0 && b >= 0 && c >= 0) || (a <= 0 && b <= 0 && c <= 0))
        {
            return true;
        }
    }
    return false;
}

void SceneRenderer::ReleaseMesh(Mesh& mesh)
{
    if (mesh.vbo)
    {
        glDeleteBuffers(1, &mesh.vbo);
    }
    if (mesh.vao)
    {
        glDeleteVertexArrays(1, &mesh.vao);
    }
    mesh = {};
}

void SceneRenderer::RetainTerrainMeshes(const std::set<std::string>& keys)
{
    for (auto it = meshes_.begin(); it != meshes_.end();)
    {
        if (it->first.compare(0, 8, "terrain:") == 0 && keys.find(it->first) == keys.end())
        {
            FrameProfiler::Instance().Add(FrameProfiler::Evictions);
            FrameProfiler::Instance().Event("evict", it->first, it->second.vertices.size());
            ReleaseMesh(it->second);
            it = meshes_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
