#include "stdafx.h"
#include "ModelAsset.h"
#define CGLTF_IMPLEMENTATION
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
#include "Dependencies/cgltf.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>

namespace Models
{
    namespace
    {
        constexpr size_t MaxFileBytes = 256 * 1024 * 1024, MaxVertices = 1000000;

        struct FileContext
        {
            std::filesystem::path root;
            std::uint64_t signature = 14695981039346656037ULL;
            size_t totalBytes = 0;
        };

        cgltf_result ReadFile(const cgltf_memory_options*,
                              const cgltf_file_options* options,
                              const char* filename,
                              cgltf_size* size,
                              void** output)
        {
            try
            {
                auto& context = *static_cast<FileContext*>(options->user_data);
                auto path = std::filesystem::weakly_canonical(std::filesystem::u8path(filename));
                auto relative = path.lexically_relative(context.root);
                if (relative.empty() || relative.is_absolute() || *relative.begin() == L"..")
                {
                    return cgltf_result_io_error;
                }
                std::ifstream input(path, std::ios::binary | std::ios::ate);
                if (!input)
                {
                    return cgltf_result_file_not_found;
                }
                auto length = input.tellg();
                if (length <= 0 || std::uint64_t(length) > MaxFileBytes ||
                    context.totalBytes + size_t(length) > MaxFileBytes)
                {
                    return cgltf_result_io_error;
                }
                if (*size && *size > size_t(length))
                {
                    return cgltf_result_data_too_short;
                }
                size_t bytes = size_t(length);
                void* data = std::malloc(bytes);
                if (!data)
                {
                    return cgltf_result_out_of_memory;
                }
                input.seekg(0);
                if (!input.read(static_cast<char*>(data), std::streamsize(bytes)))
                {
                    std::free(data);
                    return cgltf_result_io_error;
                }
                for (size_t i = 0; i < bytes; ++i)
                {
                    context.signature = (context.signature ^ static_cast<unsigned char*>(data)[i]) *
                                        1099511628211ULL;
                }
                context.totalBytes += bytes;
                *size = bytes;
                *output = data;
                return cgltf_result_success;
            }
            catch (...)
            {
                return cgltf_result_io_error;
            }
        }

        void ReleaseFile(const cgltf_memory_options*, const cgltf_file_options*, void* data)
        {
            std::free(data);
        }

        void Require(bool condition, const char* message)
        {
            if (!condition)
            {
                throw std::runtime_error(message);
            }
        }

        std::string Name(const char* name, const std::string& fallback)
        {
            return name && *name ? name : fallback;
        }

        std::vector<float> ReadValues(const cgltf_accessor* accessor,
                                      int components,
                                      size_t limit = MaxVertices)
        {
            Require(accessor && accessor->count > 0 && accessor->count <= limit &&
                        cgltf_num_components(accessor->type) == size_t(components),
                    "Invalid accessor shape or size");
            std::vector<float> values(accessor->count * components);
            Require(cgltf_accessor_unpack_floats(accessor, values.data(), values.size()) ==
                        values.size(),
                    "Cannot decode accessor");
            for (float value : values)
            {
                Require(std::isfinite(value), "Non-finite accessor value");
            }
            return values;
        }

        const cgltf_accessor* Attribute(const cgltf_primitive& primitive,
                                        cgltf_attribute_type type,
                                        int index = 0)
        {
            for (size_t i = 0; i < primitive.attributes_count; ++i)
            {
                if (primitive.attributes[i].type == type && primitive.attributes[i].index == index)
                {
                    return primitive.attributes[i].data;
                }
            }
            return nullptr;
        }
    }

    std::shared_ptr<const Asset> LoadGltf(const std::filesystem::path& file,
                                          const std::filesystem::path& allowedRoot)
    {
        FileContext context{std::filesystem::weakly_canonical(allowedRoot)};
        cgltf_options options{};
        options.file.read = ReadFile;
        options.file.release = ReleaseFile;
        options.file.user_data = &context;
        cgltf_data* raw = nullptr;
        std::string path = std::filesystem::weakly_canonical(file).u8string();
        Require(cgltf_parse_file(&options, path.c_str(), &raw) == cgltf_result_success,
                "Cannot parse GLB/glTF");
        std::unique_ptr<cgltf_data, decltype(&cgltf_free)> data(raw, cgltf_free);
        Require(raw->nodes_count > 0 && raw->nodes_count <= 4096 && raw->meshes_count <= 4096 &&
                    raw->skins_count <= 256 && raw->animations_count <= 256,
                "Model scene exceeds loader limits");
        const std::set<std::string> supportedExtensions = {
            "KHR_materials_unlit", "KHR_materials_emissive_strength", "KHR_mesh_quantization"};
        for (size_t i = 0; i < raw->extensions_required_count; ++i)
        {
            Require(supportedExtensions.count(raw->extensions_required[i]) != 0,
                    "Unsupported required glTF extension");
        }
        Require(cgltf_load_buffers(&options, raw, path.c_str()) == cgltf_result_success,
                "Cannot load glTF buffers");
        Require(cgltf_validate(raw) == cgltf_result_success,
                "Invalid glTF structure or buffer range");
        for (size_t i = 0; i < raw->buffer_views_count; ++i)
        {
            Require(!raw->buffer_views[i].has_meshopt_compression,
                    "Meshopt compression requires an offline decode");
        }

        auto asset = std::make_shared<Asset>();
        asset->materials.push_back(Material{});
        for (size_t i = 0; i < raw->materials_count; ++i)
        {
            const auto& source = raw->materials[i];
            Require(!source.normal_texture.texture && !source.emissive_texture.texture &&
                        !source.occlusion_texture.texture &&
                        !source.pbr_metallic_roughness.base_color_texture.texture &&
                        !source.pbr_metallic_roughness.metallic_roughness_texture.texture,
                    "Textured materials are not supported by the current flat-color renderer");
            Require(!source.has_pbr_specular_glossiness && !source.has_transmission &&
                        !source.has_volume,
                    "Unsupported material shading model");
            Material material;
            material.name = Name(source.name, "material_" + std::to_string(i));
            if (source.has_pbr_metallic_roughness)
            {
                std::copy_n(
                    source.pbr_metallic_roughness.base_color_factor, 4, material.baseColor.begin());
                material.metallic = source.pbr_metallic_roughness.metallic_factor;
                material.roughness = source.pbr_metallic_roughness.roughness_factor;
            }
            float energy =
                source.has_emissive_strength ? source.emissive_strength.emissive_strength : 1;
            for (int c = 0; c < 3; ++c)
            {
                material.emission[c] = source.emissive_factor[c] * energy;
            }
            for (float value : material.baseColor)
            {
                Require(std::isfinite(value) && value >= 0 && value <= 1, "Invalid material color");
            }
            for (float value : material.emission)
            {
                Require(std::isfinite(value) && value >= 0 && value <= 10000, "Invalid emission");
            }
            material.doubleSided = source.double_sided != 0;
            material.unlit = source.unlit != 0;
            material.alphaMode = int(source.alpha_mode);
            material.alphaCutoff = source.alpha_cutoff;
            asset->materials.push_back(material);
        }

        asset->nodes.resize(raw->nodes_count);
        for (size_t i = 0; i < raw->nodes_count; ++i)
        {
            const auto& source = raw->nodes[i];
            auto& node = asset->nodes[i];
            node.name = Name(source.name, "node_" + std::to_string(i));
            node.parent = source.parent ? int(cgltf_node_index(raw, source.parent)) : -1;
            node.hasMatrix = source.has_matrix != 0;
            std::copy_n(source.translation, 3, node.translation.begin());
            std::copy_n(source.rotation, 4, node.rotation.begin());
            std::copy_n(source.scale, 3, node.scale.begin());
            cgltf_node_transform_local(&source, node.matrix.data());
            for (float value : node.matrix)
            {
                Require(std::isfinite(value), "Invalid node transform");
            }
        }
        std::vector<int> marks(raw->nodes_count);
        std::function<void(int, int)> visit = [&](int node, int depth)
        {
            Require(depth < 256 && marks[node] != 1, "Cyclic or excessively deep node hierarchy");
            if (marks[node] == 2)
            {
                return;
            }
            marks[node] = 1;
            if (asset->nodes[node].parent >= 0)
            {
                visit(asset->nodes[node].parent, depth + 1);
            }
            marks[node] = 2;
            asset->nodeOrder.push_back(node);
        };
        for (size_t i = 0; i < raw->nodes_count; ++i)
        {
            visit(int(i), 0);
        }
        for (size_t i = 0; i < raw->skins_count; ++i)
        {
            const auto& source = raw->skins[i];
            Require(source.joints_count > 0 && source.joints_count <= 256,
                    "Unsupported joint count (1..256)");
            Skin skin;
            skin.name = Name(source.name, "skin_" + std::to_string(i));
            std::vector<float> inverse;
            if (source.inverse_bind_matrices)
            {
                Require(source.inverse_bind_matrices->count == source.joints_count,
                        "Inverse bind count mismatch");
                inverse = ReadValues(source.inverse_bind_matrices, 16);
            }
            for (size_t j = 0; j < source.joints_count; ++j)
            {
                skin.joints.push_back(int(cgltf_node_index(raw, source.joints[j])));
                Matrix matrix = Identity();
                if (!inverse.empty())
                {
                    std::copy_n(inverse.data() + j * 16, 16, matrix.begin());
                }
                skin.inverseBind.push_back(matrix);
            }
            asset->skins.push_back(std::move(skin));
        }

        std::vector<bool> visible(raw->nodes_count, false);
        auto markTree = [&](const cgltf_node* root)
        {
            std::vector<const cgltf_node*> pending{root};
            while (!pending.empty())
            {
                auto node = pending.back();
                pending.pop_back();
                size_t index = cgltf_node_index(raw, node);
                if (visible[index])
                {
                    continue;
                }
                visible[index] = true;
                for (size_t j = 0; j < node->children_count; ++j)
                {
                    pending.push_back(node->children[j]);
                }
            }
        };
        const cgltf_scene* scene = raw->scene          ? raw->scene
                                   : raw->scenes_count ? &raw->scenes[0]
                                                       : nullptr;
        if (scene)
        {
            for (size_t i = 0; i < scene->nodes_count; ++i)
            {
                markTree(scene->nodes[i]);
            }
        }
        else
        {
            for (size_t i = 0; i < raw->nodes_count; ++i)
            {
                if (!raw->nodes[i].parent)
                {
                    markTree(&raw->nodes[i]);
                }
            }
        }
        size_t vertexCount = 0, indexCount = 0;
        for (size_t n = 0; n < raw->nodes_count; ++n)
        {
            const auto& node = raw->nodes[n];
            if (!visible[n] || !node.mesh)
            {
                continue;
            }
            Require(!node.has_mesh_gpu_instancing, "EXT_mesh_gpu_instancing is not supported");
            for (size_t p = 0; p < node.mesh->primitives_count; ++p)
            {
                const auto& source = node.mesh->primitives[p];
                Require(source.type == cgltf_primitive_type_triangles &&
                            !source.has_draco_mesh_compression && !source.targets_count,
                        "Export triangles without Draco compression or morph targets");
                Require(!Attribute(source, cgltf_attribute_type_joints, 2) &&
                            !Attribute(source, cgltf_attribute_type_weights, 2),
                        "At most eight skin influences are supported");
                auto positions = ReadValues(Attribute(source, cgltf_attribute_type_position), 3);
                Primitive primitive;
                primitive.node = int(n);
                primitive.skin = node.skin ? int(cgltf_skin_index(raw, node.skin)) : -1;
                primitive.material =
                    source.material ? int(cgltf_material_index(raw, source.material)) + 1 : 0;
                primitive.vertices.resize(positions.size() / 3);
                vertexCount += primitive.vertices.size();
                Require(vertexCount <= MaxVertices, "Decoded mesh vertex budget exceeded");
                for (size_t i = 0; i < primitive.vertices.size(); ++i)
                {
                    std::copy_n(
                        positions.data() + i * 3, 3, primitive.vertices[i].position.begin());
                }
                auto normals = Attribute(source, cgltf_attribute_type_normal);
                if (normals)
                {
                    Require(normals->count == primitive.vertices.size(), "Normal count mismatch");
                    auto values = ReadValues(normals, 3);
                    for (size_t i = 0; i < primitive.vertices.size(); ++i)
                    {
                        std::copy_n(values.data() + i * 3, 3, primitive.vertices[i].normal.begin());
                    }
                }
                auto colors = Attribute(source, cgltf_attribute_type_color);
                if (colors)
                {
                    int components = int(cgltf_num_components(colors->type));
                    Require(colors->count == primitive.vertices.size() &&
                                (components == 3 || components == 4),
                            "Invalid vertex colors");
                    auto values = ReadValues(colors, components);
                    for (size_t i = 0; i < primitive.vertices.size(); ++i)
                    {
                        std::copy_n(values.data() + i * components,
                                    components,
                                    primitive.vertices[i].color.begin());
                    }
                }
                for (int set = 0; set < 2; ++set)
                {
                    auto joints = Attribute(source, cgltf_attribute_type_joints, set),
                         weights = Attribute(source, cgltf_attribute_type_weights, set);
                    Require(bool(joints) == bool(weights),
                            "Joint/weight attributes must be paired");
                    if (!joints)
                    {
                        continue;
                    }
                    Require(primitive.skin >= 0 && joints->count == primitive.vertices.size() &&
                                weights->count == primitive.vertices.size(),
                            "Skin attribute count mismatch");
                    auto jvalues = ReadValues(joints, 4), wvalues = ReadValues(weights, 4);
                    for (size_t i = 0; i < primitive.vertices.size(); ++i)
                    {
                        for (int k = 0; k < 4; ++k)
                        {
                            float joint = jvalues[i * 4 + k], weight = wvalues[i * 4 + k];
                            Require(joint >= 0 &&
                                        joint < asset->skins[primitive.skin].joints.size() &&
                                        joint == std::floor(joint) && weight >= 0,
                                    "Invalid skin influence");
                            primitive.vertices[i].joints[set * 4 + k] = std::uint16_t(joint);
                            primitive.vertices[i].weights[set * 4 + k] = weight;
                        }
                    }
                }
                if (primitive.skin >= 0)
                {
                    for (auto& vertex : primitive.vertices)
                    {
                        float total = 0;
                        for (float weight : vertex.weights)
                        {
                            total += weight;
                        }
                        Require(std::isfinite(total) && total > 1e-8f,
                                "Skinned vertex has no valid weights");
                        for (auto& weight : vertex.weights)
                        {
                            weight /= total;
                        }
                    }
                }
                if (source.indices)
                {
                    auto indices = ReadValues(source.indices, 1, MaxVertices * 3);
                    for (float index : indices)
                    {
                        Require(index >= 0 && index < primitive.vertices.size() &&
                                    index == std::floor(index),
                                "Triangle index out of range");
                        primitive.indices.push_back(std::uint32_t(index));
                    }
                }
                else
                {
                    for (size_t i = 0; i < primitive.vertices.size(); ++i)
                    {
                        primitive.indices.push_back(std::uint32_t(i));
                    }
                }
                indexCount += primitive.indices.size();
                Require(primitive.indices.size() % 3 == 0 && indexCount <= MaxVertices * 3,
                        "Invalid triangle list or triangle budget exceeded");
                asset->primitives.push_back(std::move(primitive));
            }
        }
        Require(!asset->primitives.empty(), "No renderable triangle meshes in the default scene");
        std::set<std::string> clipNames;
        size_t animationSamples = 0;
        for (size_t a = 0; a < raw->animations_count; ++a)
        {
            const auto& source = raw->animations[a];
            Clip clip;
            clip.name = Name(source.name, "animation_" + std::to_string(a));
            Require(clipNames.insert(clip.name).second, "Animation clip names must be unique");
            clip.start = (std::numeric_limits<float>::max)();
            std::set<std::pair<int, AnimationPath>> channels;
            for (size_t c = 0; c < source.channels_count; ++c)
            {
                const auto& channel = source.channels[c];
                Require(channel.target_node && channel.sampler &&
                            (channel.target_path == cgltf_animation_path_type_translation ||
                             channel.target_path == cgltf_animation_path_type_rotation ||
                             channel.target_path == cgltf_animation_path_type_scale),
                        "Unsupported animation target");
                Track track;
                track.node = int(cgltf_node_index(raw, channel.target_node));
                Require(!asset->nodes[track.node].hasMatrix,
                        "Animated nodes must use TRS, not matrix");
                track.path = channel.target_path == cgltf_animation_path_type_rotation
                                 ? AnimationPath::Rotation
                             : channel.target_path == cgltf_animation_path_type_scale
                                 ? AnimationPath::Scale
                                 : AnimationPath::Translation;
                Require(channels.emplace(track.node, track.path).second,
                        "Duplicate animation channel");
                const auto& sampler = *channel.sampler;
                Require(sampler.output != nullptr, "Missing animation output");
                animationSamples += sampler.output->count;
                Require(animationSamples <= 1000000, "Animation sample budget exceeded");
                track.interpolation =
                    sampler.interpolation == cgltf_interpolation_type_step ? Interpolation::Step
                    : sampler.interpolation == cgltf_interpolation_type_cubic_spline
                        ? Interpolation::CubicSpline
                        : Interpolation::Linear;
                track.times = ReadValues(sampler.input, 1, 100000);
                for (size_t t = 0; t < track.times.size(); ++t)
                {
                    Require(track.times[t] >= 0 && (t == 0 || track.times[t] > track.times[t - 1]),
                            "Animation times must increase strictly");
                }
                int components = track.path == AnimationPath::Rotation ? 4 : 3;
                auto values = ReadValues(sampler.output, components, 300000);
                size_t samples = track.times.size() *
                                 (track.interpolation == Interpolation::CubicSpline ? 3 : 1);
                Require(sampler.output->count == samples, "Animation input/output count mismatch");
                track.values.resize(samples);
                for (size_t t = 0; t < samples; ++t)
                {
                    std::copy_n(
                        values.data() + t * components, components, track.values[t].begin());
                }
                clip.start = (std::min)(clip.start, track.times.front());
                clip.end = (std::max)(clip.end, track.times.back());
                clip.tracks.push_back(std::move(track));
            }
            Require(!clip.tracks.empty(), "Animation clip has no channels");
            asset->clips.push_back(std::move(clip));
        }
        Pose pose;
        SamplePose(*asset, -1, 0, false, pose);
        asset->boundsMin.fill((std::numeric_limits<float>::max)());
        asset->boundsMax.fill((std::numeric_limits<float>::lowest)());
        for (const auto& primitive : asset->primitives)
        {
            for (const auto& vertex : primitive.vertices)
            {
                Vec3 position, normal;
                DeformVertex(primitive, vertex, pose, position, normal);
                for (int c = 0; c < 3; ++c)
                {
                    Require(std::isfinite(position[c]), "Invalid transformed vertex");
                    asset->boundsMin[c] = (std::min)(asset->boundsMin[c], position[c]);
                    asset->boundsMax[c] = (std::max)(asset->boundsMax[c], position[c]);
                }
            }
        }
        std::ostringstream signature;
        signature << std::hex << context.signature;
        asset->signature = signature.str();
        return asset;
    }
}
