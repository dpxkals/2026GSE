#include "stdafx.h"
#include "ModelDrawing.h"
#include "FrameProfiler.h"
#include <algorithm>
#include <cmath>

namespace Models
{
    namespace
    {
        float Display(float value)
        {
            value = std::clamp(value, 0.0f, 1.0f);
            return value <= .0031308f ? value * 12.92f : 1.055f * std::pow(value, 1 / 2.4f) - .055f;
        }

        Color FaceColor(const Material& material,
                        const MaterialOverrides& overrides,
                        const Vec4& vertexColor,
                        Vec3 normal)
        {
            Vec4 base = material.baseColor;
            auto found = overrides.find(material.name);
            if (found != overrides.end())
            {
                for (int i = 0; i < 3; ++i)
                {
                    base[i] = found->second[i];
                }
            }
            auto light = Normalize({-.45f, .85f, .35f});
            float shade = material.unlit ? 1.0f
                                         : .42f + .58f * (std::max)(0.0f,
                                                                    normal[0] * light[0] +
                                                                        normal[1] * light[1] +
                                                                        normal[2] * light[2]);
            Vec3 linear;
            for (int i = 0; i < 3; ++i)
            {
                linear[i] = base[i] * vertexColor[i] * shade + material.emission[i];
            }
            float energy =
                (std::max)(1.0f, (std::max)(linear[0], (std::max)(linear[1], linear[2])));
            float alpha = material.alphaMode == 0 ? 1.0f : base[3] * vertexColor[3];
            if (material.alphaMode == 1)
            {
                alpha = alpha >= material.alphaCutoff ? 1.0f : 0.0f;
            }
            return {Display(linear[0] / energy),
                    Display(linear[1] / energy),
                    Display(linear[2] / energy),
                    alpha,
                    energy};
        }
    }

    void Drawing::Build(const std::vector<DrawPart>& parts,
                        const MaterialOverrides& colors,
                        float yaw,
                        float scale)
    {
        faces_.clear();
        Matrix rotation =
            Compose({0, 0, 0}, {0, std::sin(yaw * .5f), 0, std::cos(yaw * .5f)}, {1, 1, 1});
        for (const auto& part : parts)
        {
            const auto& asset = *part.asset;
            if (!part.pose)
            {
                SamplePose(asset, -1, 0, false, bindPose_);
            }
            const Pose& pose = part.pose ? *part.pose : bindPose_;
            for (const auto& primitive : asset.primitives)
            {
                positions_.resize(primitive.vertices.size());
                if (primitive.skin >= 0)
                {
                    FrameProfiler::Instance().Add(FrameProfiler::SkinVertices,
                                                  double(primitive.vertices.size()));
                }
                normals_.resize(primitive.vertices.size());
                for (size_t v = 0; v < primitive.vertices.size(); ++v)
                {
                    Vec3 position, normal;
                    DeformVertex(primitive, primitive.vertices[v], pose, position, normal);
                    positions_[v] = TransformPoint(rotation, position);
                    normals_[v] = TransformNormal(rotation, normal);
                }
                const auto& material = asset.materials[primitive.material];
                for (size_t i = 0; i < primitive.indices.size(); i += 3)
                {
                    DrawFace face;
                    Vec3 normal{};
                    Vec4 vertexColor{};
                    for (int c = 0; c < 3; ++c)
                    {
                        size_t index = primitive.indices[i + c];
                        const auto& p = positions_[index];
                        // glTF (x,y,z) -> world (x/2,-z/2), Y elevation; 2 meters/tile.
                        face.points[c] = {18 * (p[0] + p[2]) * scale,
                                          (9 * (p[0] - p[2]) - 36 * p[1]) * scale};
                        face.depth += (2 * p[0] + p[1] - 2 * p[2]) * scale / 3;
                        for (int k = 0; k < 3; ++k)
                        {
                            normal[k] += normals_[index][k];
                        }
                        for (int k = 0; k < 4; ++k)
                        {
                            vertexColor[k] += primitive.vertices[index].color[k] / 3;
                        }
                    }
                    if (normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2] <
                        1e-8f)
                    {
                        const auto& a = positions_[primitive.indices[i]];
                        const auto& b = positions_[primitive.indices[i + 1]];
                        const auto& c = positions_[primitive.indices[i + 2]];
                        Vec3 u{b[0] - a[0], b[1] - a[1], b[2] - a[2]},
                            v{c[0] - a[0], c[1] - a[1], c[2] - a[2]};
                        normal = {u[1] * v[2] - u[2] * v[1],
                                  u[2] * v[0] - u[0] * v[2],
                                  u[0] * v[1] - u[1] * v[0]};
                    }
                    face.color = FaceColor(material, colors, vertexColor, Normalize(normal));
                    if (face.color.a > 0)
                    {
                        face.order = faces_.size();
                        faces_.push_back(face);
                    }
                }
            }
        }
        std::sort(faces_.begin(),
                  faces_.end(),
                  [](const DrawFace& a, const DrawFace& b)
                  {
                      return a.depth == b.depth ? a.order < b.order : a.depth < b.depth;
                  });
    }

    void Drawing::Submit(SceneRenderer& renderer, Point2 origin, float zoom, Color tint)
    {
        for (const auto& face : faces_)
        {
            Point2 points[3];
            for (int i = 0; i < 3; ++i)
            {
                points[i] = {origin.x + face.points[i].x * zoom,
                             origin.y + face.points[i].y * zoom};
            }
            Color color{face.color.r * tint.r,
                        face.color.g * tint.g,
                        face.color.b * tint.b,
                        face.color.a * tint.a,
                        face.color.energy * tint.energy};
            renderer.Triangle(points[0], points[1], points[2], color);
        }
    }
}
