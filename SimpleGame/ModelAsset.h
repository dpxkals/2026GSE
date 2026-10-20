#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace Models
{
    using Vec3 = std::array<float, 3>;
    using Vec4 = std::array<float, 4>;
    using Matrix = std::array<float, 16>; // glTF column-major, column vectors.

    Matrix Identity();
    Matrix Multiply(const Matrix& a, const Matrix& b);
    Matrix Compose(const Vec3& translation, const Vec4& rotation, const Vec3& scale);
    Vec3 TransformPoint(const Matrix& matrix, const Vec3& point);
    Vec3 TransformNormal(const Matrix& matrix, const Vec3& normal);
    Vec3 Normalize(Vec3 value);
    Vec4 NormalizeRotation(Vec4 value);
    Vec4 Slerp(Vec4 a, Vec4 b, float factor);

    struct Material
    {
        std::string name;
        Vec4 baseColor{1, 1, 1, 1}; // Linear RGB, not display RGB.
        Vec3 emission{};
        float metallic = 0, roughness = 1;
        bool doubleSided = false, unlit = false;
        int alphaMode = 0; // Opaque, mask, blend.
        float alphaCutoff = .5f;
    };

    struct Vertex
    {
        Vec3 position{}, normal{};
        Vec4 color{1, 1, 1, 1};
        std::array<std::uint16_t, 8> joints{};
        std::array<float, 8> weights{};
    };

    struct Primitive
    {
        int node = -1, skin = -1, material = 0;
        std::vector<Vertex> vertices;
        std::vector<std::uint32_t> indices;
    };

    struct Node
    {
        std::string name;
        int parent = -1;
        Vec3 translation{}, scale{1, 1, 1};
        Vec4 rotation{0, 0, 0, 1};
        Matrix matrix = Identity();
        bool hasMatrix = false;
    };

    struct Skin
    {
        std::string name;
        std::vector<int> joints;
        std::vector<Matrix> inverseBind;
    };

    enum class AnimationPath
    {
        Translation,
        Rotation,
        Scale
    };
    enum class Interpolation
    {
        Linear,
        Step,
        CubicSpline
    };

    struct Track
    {
        int node = -1;
        AnimationPath path = AnimationPath::Translation;
        Interpolation interpolation = Interpolation::Linear;
        std::vector<float> times;
        // Cubic samples are [in tangent, value, out tangent] for each key.
        std::vector<Vec4> values;
    };

    struct Clip
    {
        std::string name;
        float start = 0, end = 0;
        std::vector<Track> tracks;
    };

    struct Asset
    {
        std::string signature;
        std::vector<Node> nodes;
        std::vector<int> nodeOrder;
        std::vector<Primitive> primitives;
        std::vector<Material> materials;
        std::vector<Skin> skins;
        std::vector<Clip> clips;
        Vec3 boundsMin{}, boundsMax{};
    };

    // Immutable, shared data. No OpenGL calls and no gameplay state in the loader.
    std::shared_ptr<const Asset> LoadGltf(const std::filesystem::path& file,
                                          const std::filesystem::path& allowedRoot);

    struct Pose
    {
        std::vector<Vec3> translations, scales;
        std::vector<Vec4> rotations;
        std::vector<Matrix> world;
        std::vector<std::vector<Matrix>> joints;
    };

    void SamplePose(const Asset& asset, int clip, double seconds, bool loop, Pose& pose);
    // A skinned vertex is transformed directly to asset/world coordinates by G_joint * IBM.
    void DeformVertex(const Primitive& primitive,
                      const Vertex& vertex,
                      const Pose& pose,
                      Vec3& position,
                      Vec3& normal);
    bool NodeTransform(const Asset& asset,
                       const Pose& pose,
                       const std::string& name,
                       Matrix& matrix);

    class AnimationPlayer
    {
      public:
        void Play(const Asset& asset,
                  const std::string& name,
                  double now,
                  bool loop = true,
                  float speed = 1,
                  bool restart = false);
        const Pose& Evaluate(const Asset& asset, double now);
        bool Finished(const Asset& asset, double now) const;

        int ClipIndex() const
        {
            return clip_;
        }

      private:
        int clip_ = -1;
        double started_ = 0;
        float speed_ = 1;
        bool loop_ = true;
        Pose pose_;
    };
}
