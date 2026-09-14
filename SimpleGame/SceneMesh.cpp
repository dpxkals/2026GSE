#include "stdafx.h"
#include "SceneRenderer.h"
#include <algorithm>
#include <cmath>
#include <iostream>

bool SceneRenderer::BeginMesh(const std::string& key, Point2 origin, float scale)
{
    if (meshes_.find(key) != meshes_.end())
    {
        return false;
    }
    Flush();
    recordingKey_ = key;
    recordingOrigin_ = origin;
    recordingScale_ = scale;
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
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    if (mesh.vao && mesh.vbo)
    {
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
        glBufferData(GL_ARRAY_BUFFER,
                     mesh.vertices.size() * sizeof(Vertex),
                     mesh.vertices.data(),
                     GL_STATIC_DRAW);
        GLint allocated = 0;
        glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &allocated);
        if (allocated == mesh.vertices.size() * sizeof(Vertex))
        {
            ConfigureAttributes();
        }
        else
        {
            glDeleteBuffers(1, &mesh.vbo);
            mesh.vbo = 0;
        }
    }
    if (!mesh.vao || !mesh.vbo)
    {
        std::cerr << "Mesh GPU allocation failed; using cached CPU geometry: " << recordingKey_
                  << '\n';
    }
    glBindVertexArray(0);
    meshes_.emplace(recordingKey_, std::move(mesh));
    recordingKey_.clear();
    vertices_.clear();
}

void SceneRenderer::DrawMesh(const std::string& key, Point2 origin, float scale, float opacity)
{
    auto found = meshes_.find(key);
    if (found == meshes_.end())
    {
        return;
    }
    Flush();
    if (!found->second.vao || !found->second.vbo)
    {
        for (auto vertex : found->second.vertices)
        {
            vertex.x = origin.x + vertex.x * scale;
            vertex.y = origin.y + vertex.y * scale;
            vertex.a *= opacity;
            vertices_.push_back(vertex);
        }
        Flush();
        return;
    }
    glUseProgram(program_);
    glUniform2f(viewport_, float(width_), float(height_));
    glUniform2f(meshOffset_, origin.x, origin.y);
    glUniform1f(meshScale_, scale);
    glUniform1f(meshOpacity_, opacity);
    glUniform1i(textured_, false);
    glUniform1i(linearOutput_, hdrWorld_);
    glBindVertexArray(found->second.vao);
    glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(found->second.vertices.size()));
    glBindVertexArray(0);
    glUseProgram(0);
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
            ReleaseMesh(it->second);
            it = meshes_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}
