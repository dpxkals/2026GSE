#pragma once
#include "ModelAsset.h"
#include "SceneRenderer.h"
#include <map>

namespace Models
{
    using MaterialOverrides = std::map<std::string, Vec3>;

    struct DrawPart
    {
        std::shared_ptr<const Asset> asset;
        const Pose* pose = nullptr;
    };

    struct DrawFace
    {
        Point2 points[3];
        Color color;
        float depth = 0;
        size_t order = 0;
    };

    // CPU pose deformation/projection feeds the existing ordered renderer batch.
    // Topology stays immutable; scratch storage is reused, never added to the mesh cache per pose.
    class Drawing
    {
      public:
        void Build(const std::vector<DrawPart>& parts,
                   const MaterialOverrides& colors,
                   float yaw,
                   float scale);
        void Submit(SceneRenderer& renderer,
                    Point2 origin = {0, 0},
                    float zoom = 1,
                    Color tint = {1, 1, 1, 1});

      private:
        std::vector<DrawFace> faces_;
        std::vector<Vec3> positions_, normals_;
        Pose bindPose_;
    };
}
