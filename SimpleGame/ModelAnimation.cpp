#include "stdafx.h"
#include "ModelAsset.h"
#include <algorithm>
#include <cmath>

namespace Models
{
    namespace
    {
        Vec4 Sample(const Track& track, float time)
        {
            bool cubic = track.interpolation == Interpolation::CubicSpline;
            auto value = [&](size_t key)
            {
                return track.values[key * (cubic ? 3 : 1) + (cubic ? 1 : 0)];
            };
            if (time <= track.times.front())
            {
                return value(0);
            }
            if (time >= track.times.back())
            {
                return value(track.times.size() - 1);
            }
            size_t right = size_t(std::upper_bound(track.times.begin(), track.times.end(), time) -
                                  track.times.begin());
            size_t left = right - 1;
            if (track.interpolation == Interpolation::Step)
            {
                return value(left);
            }
            float duration = track.times[right] - track.times[left];
            float t = (time - track.times[left]) / duration;
            Vec4 a = value(left), b = value(right), result{};
            if (cubic)
            {
                float t2 = t * t, t3 = t2 * t;
                const auto& out = track.values[left * 3 + 2];
                const auto& in = track.values[right * 3];
                for (int i = 0; i < 4; ++i)
                {
                    result[i] = (2 * t3 - 3 * t2 + 1) * a[i] +
                                (t3 - 2 * t2 + t) * duration * out[i] + (-2 * t3 + 3 * t2) * b[i] +
                                (t3 - t2) * duration * in[i];
                }
            }
            else if (track.path == AnimationPath::Rotation)
            {
                result = Slerp(a, b, t);
            }
            else
            {
                for (int i = 0; i < 4; ++i)
                {
                    result[i] = a[i] + (b[i] - a[i]) * t;
                }
            }
            return track.path == AnimationPath::Rotation ? NormalizeRotation(result) : result;
        }
    }

    void SamplePose(const Asset& asset, int clip, double seconds, bool loop, Pose& pose)
    {
        size_t count = asset.nodes.size();
        pose.translations.resize(count);
        pose.rotations.resize(count);
        pose.scales.resize(count);
        pose.world.resize(count);
        for (size_t i = 0; i < count; ++i)
        {
            pose.translations[i] = asset.nodes[i].translation;
            pose.rotations[i] = asset.nodes[i].rotation;
            pose.scales[i] = asset.nodes[i].scale;
        }
        if (clip >= 0 && size_t(clip) < asset.clips.size())
        {
            const auto& animation = asset.clips[clip];
            double duration = animation.end - animation.start;
            double elapsed = (std::max)(0.0, seconds);
            float time =
                float(animation.start + (loop && duration > 0 ? std::fmod(elapsed, duration)
                                                              : (std::min)(elapsed, duration)));
            for (const auto& track : animation.tracks)
            {
                Vec4 value = Sample(track, time);
                if (track.path == AnimationPath::Rotation)
                {
                    pose.rotations[track.node] = NormalizeRotation(value);
                }
                else
                {
                    Vec3 xyz{value[0], value[1], value[2]};
                    if (track.path == AnimationPath::Translation)
                    {
                        pose.translations[track.node] = xyz;
                    }
                    else
                    {
                        pose.scales[track.node] = xyz;
                    }
                }
            }
        }
        for (int i : asset.nodeOrder)
        {
            const auto& node = asset.nodes[i];
            Matrix local = node.hasMatrix
                               ? node.matrix
                               : Compose(pose.translations[i], pose.rotations[i], pose.scales[i]);
            pose.world[i] = node.parent < 0 ? local : Multiply(pose.world[node.parent], local);
        }
        pose.joints.resize(asset.skins.size());
        for (size_t i = 0; i < asset.skins.size(); ++i)
        {
            const auto& skin = asset.skins[i];
            pose.joints[i].resize(skin.joints.size());
            for (size_t j = 0; j < skin.joints.size(); ++j)
            {
                pose.joints[i][j] = Multiply(pose.world[skin.joints[j]], skin.inverseBind[j]);
            }
        }
    }

    void DeformVertex(const Primitive& primitive,
                      const Vertex& vertex,
                      const Pose& pose,
                      Vec3& position,
                      Vec3& normal)
    {
        if (primitive.skin < 0)
        {
            position = TransformPoint(pose.world[primitive.node], vertex.position);
            normal = TransformNormal(pose.world[primitive.node], vertex.normal);
            return;
        }
        Matrix blended{};
        for (size_t i = 0; i < vertex.weights.size(); ++i)
        {
            if (vertex.weights[i] <= 0)
            {
                continue;
            }
            const auto& joint = pose.joints[primitive.skin][vertex.joints[i]];
            for (size_t j = 0; j < 16; ++j)
            {
                blended[j] += joint[j] * vertex.weights[i];
            }
        }
        position = TransformPoint(blended, vertex.position);
        normal = TransformNormal(blended, vertex.normal);
    }

    bool NodeTransform(const Asset& asset,
                       const Pose& pose,
                       const std::string& name,
                       Matrix& matrix)
    {
        for (size_t i = 0; i < asset.nodes.size() && i < pose.world.size(); ++i)
        {
            if (asset.nodes[i].name == name)
            {
                matrix = pose.world[i];
                return true;
            }
        }
        return false;
    }

    void AnimationPlayer::Play(const Asset& asset,
                               const std::string& name,
                               double now,
                               bool loop,
                               float speed,
                               bool restart)
    {
        int clip = -1;
        for (size_t i = 0; i < asset.clips.size(); ++i)
        {
            if (asset.clips[i].name == name)
            {
                clip = int(i);
            }
        }
        if (!std::isfinite(speed) || speed <= 0)
        {
            speed = 1;
        }
        if (restart || clip != clip_ || loop != loop_ || speed != speed_)
        {
            clip_ = clip;
            started_ = now;
            loop_ = loop;
            speed_ = speed;
        }
    }

    const Pose& AnimationPlayer::Evaluate(const Asset& asset, double now)
    {
        SamplePose(asset, clip_, (now - started_) * speed_, loop_, pose_);
        return pose_;
    }

    bool AnimationPlayer::Finished(const Asset& asset, double now) const
    {
        if (clip_ < 0 || size_t(clip_) >= asset.clips.size())
        {
            return true;
        }
        return !loop_ &&
               (now - started_) * speed_ >= asset.clips[clip_].end - asset.clips[clip_].start;
    }
}
