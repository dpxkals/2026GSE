#include "stdafx.h"
#include "SceneRenderer.h"
#include "FrameProfiler.h"
#include <cstddef>

// Preserve painter order: only adjacent identical meshes become an instanced run.
// Short/mixed runs are expanded into one shared dynamic batch, without extra draws.
void SceneRenderer::QueueMesh(const std::string& key, const MeshInstance& instance)
{
    auto found = meshes_.find(key);
    if (found == meshes_.end() || found->second.vertices.empty())
    {
        return;
    }
    if (pendingMesh_ != key)
    {
        DrainInstances();
        pendingMesh_ = key;
    }
    instances_.push_back(instance);
    if (instances_.size() >= 4096)
    {
        DrainInstances();
    }
}

void SceneRenderer::UploadMesh(Mesh& mesh)
{
    if (mesh.vao && mesh.vbo)
    {
        return;
    }
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    auto bytes = mesh.vertices.size() * sizeof(Vertex);
    glBufferData(GL_ARRAY_BUFFER, bytes, mesh.vertices.data(), GL_STATIC_DRAW);
    FrameProfiler::Instance().Add(FrameProfiler::UploadBytes, double(bytes));
    GLint allocated = 0;
    glGetBufferParameteriv(GL_ARRAY_BUFFER, GL_BUFFER_SIZE, &allocated);
    if (!mesh.vao || !mesh.vbo || allocated != bytes)
    {
        if (mesh.vao)
        {
            glDeleteVertexArrays(1, &mesh.vao);
        }
        if (mesh.vbo)
        {
            glDeleteBuffers(1, &mesh.vbo);
        }
        mesh.vao = mesh.vbo = 0;
        FrameProfiler::Instance().Add(FrameProfiler::CacheErrors);
        glBindVertexArray(0);
        return;
    }
    ConfigureAttributes();
    glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
    for (GLuint attribute = 4; attribute <= 7; ++attribute)
    {
        glEnableVertexAttribArray(attribute);
        glVertexAttribDivisor(attribute, 1);
    }
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(MeshInstance), nullptr);
    glVertexAttribPointer(5,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(MeshInstance),
                          reinterpret_cast<void*>(offsetof(MeshInstance, r)));
    glVertexAttribPointer(6,
                          1,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(MeshInstance),
                          reinterpret_cast<void*>(offsetof(MeshInstance, energy)));
    glVertexAttribPointer(7,
                          2,
                          GL_FLOAT,
                          GL_FALSE,
                          sizeof(MeshInstance),
                          reinterpret_cast<void*>(offsetof(MeshInstance, shearX)));
    glBindVertexArray(0);
}

void SceneRenderer::DrainInstances()
{
    if (instances_.empty())
    {
        return;
    }
    auto found = meshes_.find(pendingMesh_);
    if (found == meshes_.end())
    {
        instances_.clear();
        pendingMesh_.clear();
        return;
    }
    auto& mesh = found->second;
    // Tiny primitive runs (e.g. each enemy's health bar) should not split the world batch.
    const size_t minimumInstances = mesh.vertices.size() <= 6 ? 32 : 4;
    if (instances_.size() >= minimumInstances && instanceVbo_)
    {
        UploadMesh(mesh);
    }
    if (instances_.size() >= minimumInstances && mesh.vao && mesh.vbo && instanceVbo_)
    {
        FlushVertices();
        glUseProgram(program_);
        glUniform2f(viewport_, float(width_), float(height_));
        glUniform1i(instanced_, true);
        glUniform1i(textured_, false);
        glUniform1i(linearOutput_, hdrWorld_);
        glBindVertexArray(mesh.vao);
        glBindBuffer(GL_ARRAY_BUFFER, instanceVbo_);
        auto bytes = instances_.size() * sizeof(MeshInstance);
        glBufferData(GL_ARRAY_BUFFER, bytes, instances_.data(), GL_STREAM_DRAW);
        FrameProfiler::Instance().Add(FrameProfiler::UploadBytes, double(bytes));
        FrameProfiler::DrawArraysInstanced(GL_TRIANGLES,
                                           0,
                                           static_cast<GLsizei>(mesh.vertices.size()),
                                           static_cast<GLsizei>(instances_.size()));
        glBindVertexArray(0);
        glUseProgram(0);
    }
    else
    {
        for (const auto& instance : instances_)
        {
            // A bounded batch prevents unlimited transient allocations in dense scenes.
            if (vertices_.size() + mesh.vertices.size() > 262144)
            {
                FlushVertices();
            }
            for (auto vertex : mesh.vertices)
            {
                float localX = vertex.x, localY = vertex.y;
                vertex.x = instance.x + localX * instance.sx + localY * instance.shearX;
                vertex.y = instance.y + localY * instance.sy + localX * instance.shearY;
                vertex.r *= instance.r;
                vertex.g *= instance.g;
                vertex.b *= instance.b;
                vertex.a *= instance.a;
                vertex.energy *= instance.energy;
                vertices_.push_back(vertex);
            }
            FrameProfiler::Instance().Add(FrameProfiler::BatchVertices,
                                          double(mesh.vertices.size()));
        }
    }
    instances_.clear();
    pendingMesh_.clear();
}

void SceneRenderer::ReportMetrics() const
{
    size_t cpu = 0, gpu = 0;
    for (const auto& entry : meshes_)
    {
        cpu += entry.second.vertices.capacity() * sizeof(Vertex);
        if (entry.second.vbo)
        {
            gpu += entry.second.vertices.size() * sizeof(Vertex);
        }
    }
    auto& profile = FrameProfiler::Instance();
    profile.Set(FrameProfiler::MeshCount, double(meshes_.size()));
    profile.Set(FrameProfiler::MeshCpuBytes, double(cpu));
    profile.Set(FrameProfiler::MeshGpuBytes, double(gpu));
}
