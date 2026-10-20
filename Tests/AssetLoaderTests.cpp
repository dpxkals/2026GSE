// Optional CPU-only test executable. It is not part of the game build.
#include "ModelAsset.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace
{
    void Near(float actual, float expected)
    {
        if (!std::isfinite(actual) || std::abs(actual - expected) > 1e-4f)
        {
            throw std::runtime_error("Reference transform/animation result differs");
        }
    }

    void MathAndAnimation()
    {
        using namespace Models;
        auto transform = Compose({1, 2, 3}, {0, 0, std::sqrt(.5f), std::sqrt(.5f)}, {2, 3, 4});
        auto point = TransformPoint(transform, {1, 0, 0});
        Near(point[0], 1);
        Near(point[1], 4);
        Near(point[2], 3);
        auto normal = TransformNormal(transform, {1, 1, 0});
        Near(normal[0], -.5547002f);
        Near(normal[1], .8320503f);

        Asset asset;
        asset.nodes.resize(3);
        asset.nodes[0].name = "hand_R";
        asset.nodes[1].translation = {7, 0, 0}; // Must NOT be applied twice to skinned vertices.
        asset.nodes[2].translation = {0, 2, 0};
        asset.nodeOrder = {0, 1, 2};
        Skin skin;
        skin.joints = {0, 2};
        skin.inverseBind = {Identity(), Compose({0, -2, 0}, {0, 0, 0, 1}, {1, 1, 1})};
        asset.skins.push_back(skin);
        Primitive mesh;
        mesh.node = 1;
        mesh.skin = 0;
        Vertex vertex;
        vertex.joints[0] = 0;
        vertex.weights[0] = .25f;
        vertex.joints[4] = 1;
        vertex.weights[4] = .75f;
        Track track;
        track.node = 0;
        track.times = {0, 1};
        track.values = {{0, 0, 0, 0}, {2, 0, 0, 0}};
        Clip clip;
        clip.name = "Walk";
        clip.start = 0;
        clip.end = 1;
        clip.tracks = {track};
        asset.clips.push_back(clip);
        Pose pose;
        Vec3 position, deformedNormal;
        SamplePose(asset, -1, 0, false, pose);
        DeformVertex(mesh, vertex, pose, position, deformedNormal);
        Near(position[0], 0);
        Near(position[1], 0);
        SamplePose(asset, 0, .5, false, pose);
        DeformVertex(mesh, vertex, pose, position, deformedNormal);
        Near(position[0], .25f);
        SamplePose(asset, 0, 1.5, true, pose);
        DeformVertex(mesh, vertex, pose, position, deformedNormal);
        Near(position[0], .25f);
        SamplePose(asset, 0, 1.5, false, pose);
        DeformVertex(mesh, vertex, pose, position, deformedNormal);
        Near(position[0], .5f);
        Matrix socket;
        if (!NodeTransform(asset, pose, "hand_R", socket))
        {
            throw std::runtime_error("Missing animated socket");
        }
        Near(socket[12], 2);

        asset.clips[0].tracks[0].interpolation = Interpolation::Step;
        SamplePose(asset, 0, .75, false, pose);
        DeformVertex(mesh, vertex, pose, position, deformedNormal);
        Near(position[0], 0);
        auto& cubic = asset.clips[0].tracks[0];
        cubic.interpolation = Interpolation::CubicSpline;
        cubic.values = {
            {0, 0, 0, 0}, {0, 0, 0, 0}, {2, 0, 0, 0}, {0, 0, 0, 0}, {2, 0, 0, 0}, {0, 0, 0, 0}};
        SamplePose(asset, 0, .5, false, pose);
        DeformVertex(mesh, vertex, pose, position, deformedNormal);
        Near(position[0], .3125f); // Hermite midpoint 1.25, first joint weight 0.25.

        Track rotation;
        rotation.node = 0;
        rotation.path = AnimationPath::Rotation;
        rotation.times = {0, 1};
        rotation.values = {{0, 0, 0, 1}, {0, 1, 0, 0}};
        asset.clips[0].tracks = {rotation};
        vertex.position = {0, 0, 1};
        vertex.weights.fill(0);
        vertex.weights[0] = 1;
        SamplePose(asset, 0, .5, false, pose);
        DeformVertex(mesh, vertex, pose, position, deformedNormal);
        Near(position[0], 1);
        Near(position[2], 0);

        AnimationPlayer player;
        player.Play(asset, "Walk", 10, false);
        if (player.ClipIndex() != 0 || player.Finished(asset, 10.5) || !player.Finished(asset, 11))
        {
            throw std::runtime_error("Incorrect one-shot animation lifetime");
        }
        player.Play(asset, "Walk", 12, false, 1, true);
        const auto& restarted = player.Evaluate(asset, 12);
        Near(restarted.world[0][0], 1);
        player.Play(asset, "Walk", 14, false, 2);
        if (player.Finished(asset, 14.25) || !player.Finished(asset, 14.5))
        {
            throw std::runtime_error("Animation speed was not applied");
        }
        player.Play(asset, "Walk", 15, true);
        if (player.Finished(asset, 100))
        {
            throw std::runtime_error("Looping animation must not finish");
        }
        player.Play(asset, "MissingClip", 100);
        if (player.ClipIndex() != -1 || !player.Finished(asset, 100))
        {
            throw std::runtime_error("Missing clip must use the bind pose");
        }
    }
}

int wmain(int argc, wchar_t** argv)
{
    try
    {
        MathAndAnimation();
        if (argc > 1)
        {
            auto root = std::filesystem::weakly_canonical(std::filesystem::path(argv[1]));
            size_t count = 0;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(root))
            {
                if (entry.path().extension() != L".glb" && entry.path().extension() != L".gltf")
                {
                    continue;
                }
                auto asset = Models::LoadGltf(entry.path(), root);
                Models::Pose pose;
                Models::SamplePose(*asset, asset->clips.empty() ? -1 : 0, .5, true, pose);
                for (const auto& primitive : asset->primitives)
                {
                    for (const auto& vertex : primitive.vertices)
                    {
                        Models::Vec3 position, normal;
                        Models::DeformVertex(primitive, vertex, pose, position, normal);
                        for (float component : position)
                        {
                            if (!std::isfinite(component))
                            {
                                throw std::runtime_error("Non-finite imported pose");
                            }
                        }
                    }
                }
                ++count;
            }
            if (!count)
            {
                throw std::runtime_error("No model files found");
            }
            std::cout << "Imported model files: " << count << '\n';
        }
        std::cout << "Asset loader reference tests passed.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
