#include "stdafx.h"
#include "AssetRuntime.h"
#include "ModelDrawing.h"
#include "WorldSimulation.h"
#include "FrameProfiler.h"
#include "Dependencies/json.hpp"
#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>

namespace
{
    using Json = nlohmann::json;

    Json ReadJson(const std::filesystem::path& path)
    {
        if (std::filesystem::file_size(path) > 8 * 1024 * 1024)
        {
            throw std::runtime_error("Asset JSON exceeds 8 MiB");
        }
        std::ifstream input(path, std::ios::binary);
        if (!input)
        {
            throw std::runtime_error("Cannot read asset JSON: " + path.u8string());
        }
        return Json::parse(input);
    }

    void Check(bool condition, const std::string& message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    std::string Hash(const std::string& text)
    {
        std::uint64_t value = 14695981039346656037ULL;
        for (unsigned char c : text)
        {
            value = (value ^ c) * 1099511628211ULL;
        }
        std::ostringstream output;
        output << std::hex << value;
        return output.str();
    }

    std::filesystem::path Inside(const std::filesystem::path& root,
                                 const std::filesystem::path& relative)
    {
        auto path = std::filesystem::weakly_canonical(root / relative);
        auto local = path.lexically_relative(std::filesystem::weakly_canonical(root));
        Check(!local.empty() && !local.is_absolute() && *local.begin() != L"..",
              "Asset path leaves its root");
        return path;
    }

    std::string Lower(std::string value)
    {
        for (char& c : value)
        {
            if (c >= 'A' && c <= 'Z')
            {
                c += char('a' - 'A');
            }
        }
        return value;
    }

    std::uint64_t RuntimeId(const std::string& text)
    {
        Check(!text.empty() && text.size() <= 20 &&
                  std::all_of(text.begin(),
                              text.end(),
                              [](char c)
                              {
                                  return c >= '0' && c <= '9';
                              }),
              "Invalid runtime ID string");
        return std::stoull(text);
    }

    std::filesystem::path FindRoot(std::filesystem::path directory)
    {
        std::filesystem::path packaged;
        for (int i = 0; i < 6 && !directory.empty(); ++i)
        {
            auto candidate = directory / L"assets";
            if (std::filesystem::is_directory(candidate))
            {
                if (std::filesystem::exists(directory / L"SimpleGame" / L"SimpleGame.vcxproj"))
                {
                    return std::filesystem::weakly_canonical(candidate);
                }
                if (packaged.empty())
                {
                    packaged = std::filesystem::weakly_canonical(candidate);
                }
            }
            auto parent = directory.parent_path();
            if (parent == directory)
            {
                break;
            }
            directory = parent;
        }
        return packaged;
    }

    std::filesystem::path FindConfig(std::filesystem::path directory)
    {
        auto project = directory;
        for (int i = 0; i < 6 && !project.empty(); ++i)
        {
            if (std::filesystem::exists(project / L"SimpleGame" / L"SimpleGame.vcxproj") &&
                std::filesystem::is_regular_file(project / L"SimpleGame" / L"Config" /
                                                 L"AssetBindings.json"))
            {
                return project / L"SimpleGame" / L"Config" / L"AssetBindings.json";
            }
            if (project.parent_path() == project)
            {
                break;
            }
            project = project.parent_path();
        }
        for (int i = 0; i < 6 && !directory.empty(); ++i)
        {
            for (const auto& candidate :
                 {directory / L"Config" / L"AssetBindings.json",
                  directory / L"SimpleGame" / L"Config" / L"AssetBindings.json"})
            {
                if (std::filesystem::is_regular_file(candidate))
                {
                    return candidate;
                }
            }
            auto parent = directory.parent_path();
            if (parent == directory)
            {
                break;
            }
            directory = parent;
        }
        throw std::runtime_error("Config/AssetBindings.json is missing");
    }

    const char* PropName(Prop prop)
    {
        static const char* names[] = {"None",
                                      "Tree",
                                      "Rock",
                                      "Ruin",
                                      "Beacon",
                                      "Cottage",
                                      "Chapel",
                                      "Watchtower",
                                      "Well",
                                      "Graves",
                                      "WallX",
                                      "WallY",
                                      "WallTower"};
        return names[int(prop)];
    }

    struct Recipe
    {
        std::string id;
        int town = 0;
        std::vector<std::string> parts;
        Models::MaterialOverrides colors;
        std::uint64_t requestedId = 0;
    };
}

struct AssetRuntime::Implementation
{
    struct Instance
    {
        Models::AnimationPlayer player;
        double lastSeen = 0;
        std::string action = "idle";
        bool attack = false, hurt = false;
    };

    std::filesystem::path root, save;
    Json config, parts;
    std::string revision;
    std::map<std::string, std::filesystem::path> files;
    std::map<std::string, std::shared_ptr<const Models::Asset>> models;
    std::map<std::filesystem::path, std::shared_ptr<const Models::Asset>> decodedFiles;
    std::map<std::string, Recipe> recipes;
    std::map<std::uint64_t, std::string> bindings;
    std::map<std::string, Instance> animations;
    std::set<std::string> warnings;
    Models::Drawing drawing;
    double now = 0, nextBindingUpdate = 0;
    bool bindingDirty = false;

    void Warn(const std::string& message)
    {
        if (warnings.insert(message).second)
        {
            std::cerr << "[Assets] " << message << '\n';
        }
    }

    void Load(const std::filesystem::path& assetRoot,
              const std::filesystem::path& configPath,
              const std::filesystem::path& saveDirectory)
    {
        root = assetRoot;
        save = saveDirectory / L"npc_appearance_bindings_v1.json";
        config = ReadJson(configPath);
        Check(config.at("schema_version") == 1, "Unsupported asset binding schema");
        Check(config.at("props").is_object() && config.at("roles").is_object() &&
                  config.at("animation_aliases").is_object(),
              "Invalid asset binding objects");
        float scale = config.value("default_character_scale", .8f);
        Check(std::isfinite(scale) && scale > 0 && scale <= 10, "Invalid character scale");
        Check(config.value("npc_model", Json("")).is_string(), "Invalid NPC model ID");
        for (const auto& aliases : config.at("animation_aliases"))
        {
            Check(aliases.is_array(), "Animation aliases must be arrays");
            for (const auto& alias : aliases)
            {
                Check(alias.is_string() && !alias.get<std::string>().empty(),
                      "Animation aliases must contain nonempty clip names");
            }
        }
        for (const std::string group : {"props", "roles"})
        {
            for (const auto& binding : config.at(group))
            {
                Check(binding.is_object(), "Asset bindings must be objects");
                Check(binding.value("model", Json("")).is_string(), "Invalid model ID");
                Check(binding.value("recipe", Json("")).is_string(), "Invalid recipe ID");
                float bindingScale = binding.value("scale", 1.0f);
                float yaw = binding.value("yaw_degrees", 0.0f);
                Check(std::isfinite(bindingScale) && bindingScale > 0 && bindingScale <= 10 &&
                          std::isfinite(yaw),
                      "Invalid asset binding transform");
                Check(binding.value("fit_footprint", Json(false)).is_boolean() &&
                          binding.value("roof_palette", Json(false)).is_boolean(),
                      "Invalid prop binding flags");
                if (binding.contains("variants"))
                {
                    const auto& variants = binding.at("variants");
                    Check(variants.is_array() && !variants.empty(), "Invalid model variants");
                    for (const auto& variant : variants)
                    {
                        Check(variant.is_string() && !variant.get<std::string>().empty(),
                              "Model variants must contain nonempty IDs");
                    }
                }
            }
        }
        std::vector<std::filesystem::path> catalogs;
        std::vector<std::filesystem::path> meshFiles;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(
                 root, std::filesystem::directory_options::skip_permission_denied))
        {
            if (!entry.is_regular_file())
            {
                continue;
            }
            if (entry.path().filename() == L"asset_catalog.json")
            {
                catalogs.push_back(entry.path());
            }
            std::string extension = Lower(entry.path().extension().u8string());
            if (extension == ".glb" || extension == ".gltf")
            {
                meshFiles.push_back(entry.path());
            }
        }
        std::sort(catalogs.begin(), catalogs.end());
        for (const auto& path : catalogs)
        {
            Json catalog = ReadJson(path);
            Check(catalog.at("schema_version") == 1, "Unsupported asset catalog schema");
            auto pack = path.parent_path().parent_path();
            for (const auto& entry : catalog.at("assets"))
            {
                std::string id = entry.at("asset_id").get<std::string>();
                auto file = Inside(
                    root,
                    Inside(pack, std::filesystem::u8path(entry.at("file").get<std::string>()))
                        .lexically_relative(root));
                Check(!id.empty() && id.size() < 256 && std::filesystem::is_regular_file(file),
                      "Missing catalog asset: " + id);
                Check(Lower(file.extension().u8string()) == ".glb" ||
                          Lower(file.extension().u8string()) == ".gltf",
                      "Catalog runtime asset must be GLB/glTF");
                auto found = files.find(id);
                Check(found == files.end() || found->second == file, "Duplicate asset ID: " + id);
                files[id] = file;
            }
        }
        // New standalone GLB/glTF files can be addressed by filename stem before cataloging.
        std::sort(meshFiles.begin(), meshFiles.end());
        for (const auto& path : meshFiles)
        {
            auto file = Inside(root, path.lexically_relative(root));
            std::string id = path.stem().u8string();
            auto found = files.find(id);
            Check(found == files.end() || found->second == file,
                  "Duplicate model filename ID: " + id);
            files[id] = file;
        }
        Check(!files.empty(), "No GLB/glTF files found under assets");
        auto pack = Inside(root, std::filesystem::u8path(config.at("npc_pack").get<std::string>()));
        parts = ReadJson(pack / L"Data" / L"npc_parts.json");
        Check(parts.at("schema_version") == 1 && parts.at("gltf_up_axis") == "+Y",
              "Unsupported NPC part contract");
        Check(parts.at("palette").at("color_space") == "linear RGB",
              "NPC palette must use linear RGB");
        std::filesystem::path presetPath = pack / L"Data" / L"npc_presets_210.json";
        if (config.contains("npc_presets"))
        {
            presetPath =
                Inside(pack, std::filesystem::u8path(config.at("npc_presets").get<std::string>()));
        }
        Json presets = ReadJson(presetPath);
        Check(presets.at("schema_version") == 1, "Unsupported NPC preset schema");
        const auto& slots = parts.at("slots");
        for (const std::string slot : {"body", "head", "torso", "legs"})
        {
            Check(slots.contains(slot), "Missing mandatory slot definition: " + slot);
        }
        for (auto slot = slots.begin(); slot != slots.end(); ++slot)
        {
            std::set<int> indices;
            for (const auto& variant : slot.value())
            {
                int index = variant.at("index").get<int>();
                Check(index >= 0 && indices.insert(index).second,
                      "Duplicate or negative part index");
                Check(files.count(variant.at("asset_id").get<std::string>()) != 0,
                      "Slot refers to a missing model");
            }
        }
        std::set<std::uint64_t> requestedIds;
        for (const auto& source : presets.at("recipes"))
        {
            Recipe recipe;
            recipe.id = source.at("recipe_id").get<std::string>();
            recipe.town = source.value("town_index", 0);
            const auto& choices = source.at("parts");
            for (auto slot = slots.begin(); slot != slots.end(); ++slot)
            {
                int choice = choices.value(slot.key(), -1);
                bool required = slot.key() == "body" || slot.key() == "head" ||
                                slot.key() == "torso" || slot.key() == "legs";
                Check(!required || choice >= 0, "Missing mandatory slot in " + recipe.id);
                if (choice == -1)
                {
                    continue;
                }
                auto variant = std::find_if(slot.value().begin(),
                                            slot.value().end(),
                                            [choice](const Json& value)
                                            {
                                                return value.at("index") == choice;
                                            });
                Check(variant != slot.value().end(), "Unknown part index in " + recipe.id);
                std::string id = variant->at("asset_id").get<std::string>();
                Check(files.count(id) != 0, "Missing recipe GLB: " + id);
                recipe.parts.push_back(id);
            }
            for (const auto& rule : parts.at("compatibility"))
            {
                bool applies = true;
                for (auto condition = rule.at("if").begin(); condition != rule.at("if").end();
                     ++condition)
                {
                    int value = choices.value(condition.key(), -1);
                    if (std::find(condition.value().begin(),
                                  condition.value().end(),
                                  Json(value)) == condition.value().end())
                    {
                        applies = false;
                    }
                }
                if (!applies)
                {
                    continue;
                }
                for (const std::string key : {"require", "forbid"})
                {
                    if (!rule.contains(key))
                    {
                        continue;
                    }
                    for (auto check = rule.at(key).begin(); check != rule.at(key).end(); ++check)
                    {
                        int value = choices.value(check.key(), -1);
                        bool listed =
                            std::find(check.value().begin(), check.value().end(), Json(value)) !=
                            check.value().end();
                        Check(key == "require" ? listed : !listed,
                              "Incompatible NPC parts in " + recipe.id);
                    }
                }
            }
            for (auto palette = source.at("palette").begin(); palette != source.at("palette").end();
                 ++palette)
            {
                int index = palette.value().get<int>();
                const auto& names = parts.at("palette").at(palette.key());
                Check(index >= 0 && index < int(names.size()), "Palette index out of range");
                const auto& color =
                    parts.at("palette").at("colors").at(names.at(index).get<std::string>());
                Models::Vec3 rgb = color.get<Models::Vec3>();
                for (float value : rgb)
                {
                    Check(std::isfinite(value) && value >= 0 && value <= 1,
                          "Invalid linear palette color");
                }
                recipe.colors[parts.at("palette")
                                  .at("replace_material")
                                  .at(palette.key())
                                  .get<std::string>()] = rgb;
            }
            if (source.contains("runtime_npc_id") && !source.at("runtime_npc_id").is_null())
            {
                const auto& id = source.at("runtime_npc_id");
                Check(id.is_string() || id.is_number_unsigned() ||
                          (id.is_number_integer() && id.get<std::int64_t>() > 0),
                      "Runtime NPC ID must be a positive integer or digit string");
                recipe.requestedId =
                    id.is_string() ? RuntimeId(id.get<std::string>()) : id.get<std::uint64_t>();
                Check(recipe.requestedId > 0, "Invalid runtime NPC ID");
                Check(requestedIds.insert(recipe.requestedId).second,
                      "Duplicate explicitly bound runtime NPC ID");
            }
            Check(!recipe.id.empty() && recipes.emplace(recipe.id, recipe).second,
                  "Duplicate recipe ID");
        }
        Check(!recipes.empty(), "NPC preset list is empty");
        for (const auto& binding : config.at("roles"))
        {
            std::string recipe = binding.value("recipe", std::string{});
            Check(recipe.empty() || recipes.count(recipe) != 0,
                  "Role refers to an unknown recipe: " + recipe);
        }
        revision = Hash(config.dump() + parts.dump() + presets.dump());
        if (std::filesystem::exists(save))
        {
            // A damaged binding file is preserved and never silently replaced.
            Json stored = ReadJson(save);
            Check(stored.at("schema_version") == 1, "Unsupported appearance save schema");
            std::set<std::string> used;
            for (const auto& entry : stored.at("bindings"))
            {
                auto id = RuntimeId(entry.at("npc_id").get<std::string>());
                std::string recipe = entry.at("recipe_id").get<std::string>();
                Check(id > 0 && bindings.emplace(id, recipe).second && used.insert(recipe).second,
                      "Duplicate appearance binding");
                Check(recipes.count(recipe) != 0,
                      "A saved appearance recipe was removed: " + recipe);
            }
        }
        std::cout << "[Assets] root=" << root.u8string() << " models=" << files.size()
                  << " recipes=" << recipes.size() << " revision=" << revision << '\n';
    }

    std::shared_ptr<const Models::Asset> Model(const std::string& id)
    {
        auto found = models.find(id);
        if (found != models.end())
        {
            return found->second;
        }
        auto path = files.find(id);
        if (path == files.end())
        {
            Warn("Unknown model: " + id);
            return {};
        }
        auto decoded = decodedFiles.find(path->second);
        if (decoded != decodedFiles.end())
        {
            models[id] = decoded->second;
            return decoded->second;
        }
        try
        {
            auto started = std::chrono::steady_clock::now();
            auto model = Models::LoadGltf(path->second, root);
            FrameProfiler::Instance().Add(FrameProfiler::AssetLoads);
            FrameProfiler::Instance().Add(FrameProfiler::AssetDecodeMs,
                                          std::chrono::duration<double, std::milli>(
                                              std::chrono::steady_clock::now() - started)
                                              .count());
            models[id] = model;
            decodedFiles[path->second] = model;
            size_t triangles = 0;
            for (const auto& primitive : model->primitives)
            {
                triangles += primitive.indices.size() / 3;
            }
            std::cout << "[AssetLoad] id=" << id << " triangles=" << triangles
                      << " nodes=" << model->nodes.size() << " skins=" << model->skins.size()
                      << " clips=" << model->clips.size() << " ms="
                      << std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - started)
                             .count()
                      << '\n';
            return model;
        }
        catch (const std::exception& error)
        {
            Warn("Rejected " + id + ": " + error.what());
            models[id] = {}; // Retry only on explicit F10 reload, not every frame.
            decodedFiles[path->second] = {};
            return {};
        }
    }

    void SaveBindings()
    {
        Json output = {{"schema_version", 1}, {"bindings", Json::array()}};
        for (const auto& entry : bindings)
        {
            output["bindings"].push_back(
                {{"npc_id", std::to_string(entry.first)}, {"recipe_id", entry.second}});
        }
        auto temp = save;
        temp += L".tmp";
        std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
        stream << output.dump(2) << '\n';
        stream.flush();
        stream.close();
        Check(bool(stream) && MoveFileExW(temp.c_str(),
                                          save.c_str(),
                                          MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),
              "Cannot commit NPC appearance bindings");
    }

    void Bind(const std::vector<Citizen>& citizens)
    {
        auto previous = bindings;
        std::set<std::uint64_t> active;
        for (const auto& npc : citizens)
        {
            active.insert(npc.id);
        }
        for (auto it = bindings.begin(); it != bindings.end();)
        {
            if (!active.count(it->first))
            {
                it = bindings.erase(it);
            }
            else
            {
                ++it;
            }
        }
        // Explicit artist-provided IDs are respected, but unbound recipe ordinals are never IDs.
        for (const auto& entry : recipes)
        {
            if (entry.second.requestedId && active.count(entry.second.requestedId))
            {
                for (auto it = bindings.begin(); it != bindings.end();)
                {
                    if (it->second == entry.first && it->first != entry.second.requestedId)
                    {
                        it = bindings.erase(it);
                    }
                    else
                    {
                        ++it;
                    }
                }
                bindings[entry.second.requestedId] = entry.first;
            }
        }
        std::set<std::string> used;
        for (const auto& entry : bindings)
        {
            used.insert(entry.second);
        }
        for (const auto& npc : citizens)
        {
            if (bindings.count(npc.id))
            {
                continue;
            }
            auto choose = [&](bool town)
            {
                for (const auto& entry : recipes)
                {
                    if (!used.count(entry.first) && (!town || entry.second.town == npc.home))
                    {
                        return entry.first;
                    }
                }
                return std::string{};
            };
            std::string recipe = choose(true);
            if (recipe.empty())
            {
                recipe = choose(false);
            }
            if (recipe.empty())
            {
                Warn("Not enough appearance recipes for live NPCs");
                continue;
            }
            bindings[npc.id] = recipe;
            used.insert(recipe);
        }
        bindingDirty = bindingDirty || previous != bindings;
        if (bindingDirty)
        {
            try
            {
                SaveBindings();
                bindingDirty = false;
                std::cout << "[AssetBindings] bound actual NPC IDs=" << bindings.size() << '\n';
            }
            catch (const std::exception& error)
            {
                Warn(error.what());
            }
        }
    }

    std::string Clip(const Models::Asset& model, const std::string& action)
    {
        const auto& aliases = config.at("animation_aliases");
        for (const std::string state : {action, std::string("idle")})
        {
            if (!aliases.contains(state))
            {
                continue;
            }
            for (const auto& alias : aliases.at(state))
            {
                for (const auto& clip : model.clips)
                {
                    if (clip.name == alias.get<std::string>())
                    {
                        return clip.name;
                    }
                }
            }
        }
        return model.clips.empty() ? std::string{} : model.clips.front().name;
    }

    std::string PropParameters(Prop prop,
                               int variation,
                               std::shared_ptr<const Models::Asset>& model,
                               float& scale,
                               float& yaw,
                               Models::MaterialOverrides& colors)
    {
        if (!config.at("props").contains(PropName(prop)))
        {
            return {};
        }
        const auto& binding = config.at("props").at(PropName(prop));
        std::string id = binding.value("model", std::string{});
        if (binding.contains("variants"))
        {
            const auto& variants = binding.at("variants");
            Check(!variants.empty(), "Empty prop variant list");
            id = variants.at(size_t((std::max)(0, variation)) % variants.size()).get<std::string>();
        }
        if (id.empty())
        {
            return {};
        }
        model = Model(id);
        if (!model)
        {
            return {};
        }
        scale = binding.value("scale", 1.0f);
        if (binding.value("fit_footprint", false))
        {
            float extent = (std::max)(model->boundsMax[0] - model->boundsMin[0],
                                      model->boundsMax[2] - model->boundsMin[2]);
            if (extent > .001f)
            {
                scale *= (std::min)(1.0f, float(World::PropHalfExtent(prop) * 4) / extent);
            }
        }
        Check(std::isfinite(scale) && scale > 0 && scale <= 10, "Invalid prop scale");
        yaw = binding.value("yaw_degrees", 0.0f) * .01745329252f;
        Check(std::isfinite(yaw), "Invalid prop rotation");
        if (binding.value("roof_palette", false))
        {
            const auto& palette = parts.at("palette");
            const auto& roofs = palette.at("roof");
            Check(!roofs.empty(), "Empty roof palette");
            auto name =
                roofs.at(size_t((std::max)(0, variation)) % roofs.size()).get<std::string>();
            colors["MAT_Roof"] = palette.at("colors").at(name).get<Models::Vec3>();
        }
        return id;
    }

    bool Draw(SceneRenderer& renderer,
              const std::string& instance,
              const std::vector<std::string>& ids,
              const Models::MaterialOverrides& colors,
              Point2 origin,
              float zoom,
              const ActorPose& pose,
              float scale)
    {
        std::vector<Models::DrawPart> drawParts;
        std::string signature = revision;
        bool animated = false;
        for (const auto& id : ids)
        {
            auto model = Model(id);
            if (!model)
            {
                return false;
            }
            signature += ":" + id + ":" + model->signature;
            animated = animated || !model->clips.empty();
            drawParts.push_back({model, nullptr});
        }
        if (drawParts.empty())
        {
            return false;
        }
        float yaw = pose.facing >= 0 ? 1.570796327f : 3.141592654f;
        Point2 at{origin.x - pose.facing * pose.hurt * 2 * zoom,
                  origin.y + (pose.breath * .5f - std::abs(pose.stride) * .7f) * zoom};
        Color tint{1, 1 - pose.hurt * .4f, 1 - pose.hurt * .55f, 1, 1 + pose.hurt};
        renderer.Ellipse(
            {origin.x, origin.y + 2 * zoom}, 16 * zoom, 5 * zoom, {.01f, .015f, .015f, .27f});
        if (animated)
        {
            FrameProfiler::Scope poseTimer(FrameProfiler::AssetPoseMs);
            for (size_t i = 0; i < drawParts.size(); ++i)
            {
                if (drawParts[i].asset->clips.empty())
                {
                    continue;
                }
                auto& state = animations[instance + ":" + ids[i]];
                state.lastSeen = now;
                bool hurt = pose.hurt > .15f, attack = pose.attack > .1f;
                bool hurtTrigger = hurt && !state.hurt;
                bool attackTrigger = attack && !state.attack;
                state.hurt = hurt;
                state.attack = attack;
                bool holding = (state.action == "hurt" || state.action == "attack") &&
                               !state.player.Finished(*drawParts[i].asset, now);
                std::string action = state.action;
                if (hurtTrigger)
                {
                    action = "hurt";
                }
                else if (attackTrigger && (!holding || state.action != "hurt"))
                {
                    action = "attack";
                }
                else if (!holding)
                {
                    action = pose.moving > .15f ? "walk" : "idle";
                }
                bool restart = hurtTrigger || (attackTrigger && action == "attack");
                state.action = action;
                auto clip = Clip(*drawParts[i].asset, action);
                state.player.Play(*drawParts[i].asset,
                                  clip,
                                  now,
                                  action == "idle" || action == "walk",
                                  1,
                                  restart);
                drawParts[i].pose = &state.player.Evaluate(*drawParts[i].asset, now);
            }
            drawing.Build(drawParts, colors, yaw, scale);
            drawing.Submit(renderer, at, zoom, tint);
        }
        else
        {
            std::string key =
                "gltf:v1:character:" +
                Hash(signature + std::to_string(yaw) + std::to_string(scale) + Json(colors).dump());
            if (renderer.BeginMesh(key, {0, 0}, 1))
            {
                drawing.Build(drawParts, colors, yaw, scale);
                drawing.Submit(renderer);
                renderer.EndMesh();
            }
            renderer.DrawMeshPose(key, at, zoom, {0, 0}, 0, 1, tint);
        }
        return true;
    }
};

AssetRuntime::AssetRuntime() = default;
AssetRuntime::~AssetRuntime() = default;

AssetRuntime& AssetRuntime::Instance()
{
    static AssetRuntime runtime;
    return runtime;
}

bool AssetRuntime::Initialize(const std::filesystem::path& executableDirectory,
                              const std::filesystem::path& saveDirectory,
                              const std::filesystem::path& explicitAssetRoot)
{
    executable_ = executableDirectory;
    save_ = saveDirectory;
    try
    {
        root_ = explicitAssetRoot.empty() ? FindRoot(executableDirectory)
                                          : std::filesystem::weakly_canonical(explicitAssetRoot);
        Check(!root_.empty() && std::filesystem::is_directory(root_),
              "assets directory was not found");
        auto staged = std::make_unique<Implementation>();
        staged->Load(root_, FindConfig(executable_), save_);
        data_ = std::move(staged);
        return true;
    }
    catch (const std::exception& error)
    {
        std::cerr << "[Assets] Keeping current/fallback visuals: " << error.what() << '\n';
        return false;
    }
}

bool AssetRuntime::Reload(SceneRenderer& renderer)
{
    if (!Initialize(executable_, save_, root_))
    {
        return false;
    }
    renderer.RemoveMeshes("gltf:");
    std::cout << "[Assets] F10 reload committed; model/animation caches reset\n";
    return true;
}

void AssetRuntime::Update(double simulationTime, const std::vector<Citizen>& citizens)
{
    if (!data_)
    {
        return;
    }
    if (simulationTime < data_->now)
    {
        data_->animations.clear();
        data_->nextBindingUpdate = 0;
    }
    data_->now = simulationTime;
    if (simulationTime >= data_->nextBindingUpdate)
    {
        data_->nextBindingUpdate = simulationTime + 5;
        data_->Bind(citizens);
        for (auto it = data_->animations.begin(); it != data_->animations.end();)
        {
            if (simulationTime - it->second.lastSeen > 15)
            {
                it = data_->animations.erase(it);
            }
            else
            {
                ++it;
            }
        }
    }
}

void AssetRuntime::Shutdown()
{
    data_.reset();
}

bool AssetRuntime::Ready() const
{
    return bool(data_);
}

std::string AssetRuntime::PropMesh(SceneRenderer& renderer, Prop prop, int variation)
{
    if (!data_)
    {
        return {};
    }
    try
    {
        std::shared_ptr<const Models::Asset> model;
        float scale = 1, yaw = 0;
        Models::MaterialOverrides colors;
        std::string id = data_->PropParameters(prop, variation, model, scale, yaw, colors);
        if (id.empty())
        {
            return {};
        }
        std::string key = "gltf:v1:prop:" +
                          Hash(data_->revision + id + model->signature + std::to_string(variation) +
                               std::to_string(scale) + std::to_string(yaw));
        if (renderer.BeginMesh(key, {0, 0}, 1))
        {
            data_->drawing.Build({{model, nullptr}}, colors, yaw, scale);
            data_->drawing.Submit(renderer);
            renderer.EndMesh();
        }
        return key;
    }
    catch (const std::exception& error)
    {
        data_->Warn(error.what());
        return {};
    }
}

bool AssetRuntime::DrawAnimatedProp(SceneRenderer& renderer,
                                    Prop prop,
                                    int variation,
                                    const std::string& instance,
                                    Point2 origin,
                                    float zoom,
                                    float opacity)
{
    if (!data_)
    {
        return false;
    }
    try
    {
        std::shared_ptr<const Models::Asset> model;
        float scale = 1, yaw = 0;
        Models::MaterialOverrides colors;
        std::string id = data_->PropParameters(prop, variation, model, scale, yaw, colors);
        if (id.empty() || model->clips.empty())
        {
            return false;
        }
        auto& state = data_->animations["prop:" + instance];
        FrameProfiler::Scope poseTimer(FrameProfiler::AssetPoseMs);
        state.lastSeen = data_->now;
        state.player.Play(*model, data_->Clip(*model, "idle"), data_->now, true);
        auto& pose = state.player.Evaluate(*model, data_->now);
        data_->drawing.Build({{model, &pose}}, colors, yaw, scale);
        data_->drawing.Submit(renderer, origin, zoom, {1, 1, 1, opacity});
        return true;
    }
    catch (const std::exception& error)
    {
        data_->Warn(error.what());
        return false;
    }
}

bool AssetRuntime::DrawCitizen(
    SceneRenderer& renderer, std::uint64_t id, Point2 origin, float zoom, const ActorPose& pose)
{
    if (!data_)
    {
        return false;
    }
    auto found = data_->bindings.find(id);
    if (found == data_->bindings.end())
    {
        return false;
    }
    try
    {
        const auto& recipe = data_->recipes.at(found->second);
        std::string model = data_->config.value("npc_model", std::string{});
        return data_->Draw(renderer,
                           "citizen:" + std::to_string(id),
                           model.empty() ? recipe.parts : std::vector<std::string>{model},
                           recipe.colors,
                           origin,
                           zoom,
                           pose,
                           data_->config.value("default_character_scale", .8f));
    }
    catch (const std::exception& error)
    {
        data_->Warn(error.what());
        return false;
    }
}

bool AssetRuntime::DrawRole(SceneRenderer& renderer,
                            const std::string& role,
                            const std::string& instance,
                            Point2 origin,
                            float zoom,
                            const ActorPose& pose)
{
    if (!data_)
    {
        return false;
    }
    try
    {
        const auto& roles = data_->config.at("roles");
        if (!roles.contains(role))
        {
            return false;
        }
        const auto& binding = roles.at(role);
        std::string model = binding.value("model", std::string{}),
                    recipe = binding.value("recipe", std::string{});
        Models::MaterialOverrides colors;
        std::vector<std::string> ids;
        if (!model.empty())
        {
            ids.push_back(model);
        }
        else if (!recipe.empty())
        {
            ids = data_->recipes.at(recipe).parts;
            colors = data_->recipes.at(recipe).colors;
        }
        else
        {
            return false;
        }
        float scale = binding.value("scale", data_->config.value("default_character_scale", .8f));
        Check(std::isfinite(scale) && scale > 0 && scale <= 10, "Invalid character role scale");
        return data_->Draw(renderer, instance, ids, colors, origin, zoom, pose, scale);
    }
    catch (const std::exception& error)
    {
        data_->Warn(error.what());
        return false;
    }
}
