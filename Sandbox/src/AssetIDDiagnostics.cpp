#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "Quiet/Project/AssetRegistry.h"
#include "Quiet/Project/BuiltInAssetIDs.h"

int RunAssetIDDiagnostics(const int argc, char** argv)
{
    if (argc >= 2 && std::string_view(argv[1]) == "--asset-id-preflight")
    {
        using json = nlohmann::json;
        namespace fs = std::filesystem;

        if (argc < 4)
        {
            std::cerr << "Usage: Quiet_Sandbox --asset-id-preflight <AssetRegistry.json> <scene.json> [scene.json ...]\n";
            return 2;
        }

        std::unordered_map<std::string, std::string> assetTypes;
        std::unordered_map<std::string, Quiet::Core::UUID> uuidMapping;
        json registryPreview;
        std::vector<std::pair<fs::path, json>> scenePreviews;
        std::size_t issues = 0;
        std::size_t referencesChecked = 0;
        std::size_t referencesTranslated = 0;

        try
        {
            const fs::path registryPath = argv[2];
            std::ifstream registryInput(registryPath);

            if (!registryInput)
            {
                throw std::runtime_error("Cannot open registry: " + registryPath.string());
            }

            json registry;
            registryInput >> registry;

            if (!registry.is_object() ||
                !registry.contains("Assets") ||
                !registry.at("Assets").is_array())
            {
                throw std::runtime_error("Registry must contain an Assets array.");
            }

            if (registry.contains("Version") &&
                (!registry.at("Version").is_number_integer() ||
                 registry.at("Version") != 1))
            {
                throw std::runtime_error("Unsupported registry version.");
            }

            for (const auto& record : registry.at("Assets"))
            {
                if (!record.is_object() ||
                    !record.contains("ID") ||
                    !record.at("ID").is_string() ||
                    !record.contains("Type") ||
                    !record.at("Type").is_string())
                {
                    throw std::runtime_error("Registry contains a record without a string ID or Type.");
                }

                const std::string id = record.at("ID").get<std::string>();
                const std::string type = record.at("Type").get<std::string>();

                if (id.empty() ||
                    (type != "Mesh" &&
                     type != "Texture" &&
                     type != "Audio" &&
                     type != "Script"))
                {
                    throw std::runtime_error("Registry contains an invalid asset ID or Type.");
                }

                if (!assetTypes.emplace(id, type).second)
                {
                    throw std::runtime_error("Registry contains duplicate ID: " + id);
                }
            }

            Quiet::Project::AssetRegistry legacyRegistry{};
            if (!Quiet::Project::AssetRegistryIO::LoadFromFile(registryPath, legacyRegistry))
            {
                throw std::runtime_error("The engine rejected the legacy asset registry.");
            }
            if (!Quiet::Project::AssetRegistryIO::CreateLegacyUUIDMap(legacyRegistry, uuidMapping))
            {
                throw std::runtime_error("Could not create the legacy asset UUID mapping.");
            }

            registryPreview = std::move(registry);
            for (auto& record : registryPreview.at("Assets"))
            {
                const std::string legacyID = record.at("ID").get<std::string>();
                const auto mapped = uuidMapping.find(legacyID);

                if (mapped == uuidMapping.end() || !mapped->second.IsValid())
                {
                    throw std::runtime_error("No valid UUID mapping for registry asset: " + legacyID);
                }

                record["ID"] = Quiet::Core::UUIDToString(mapped->second);
            }

            std::cout << "Registry: " << registryPath.string() << '\n';
            std::cout << "Registry records: " << assetTypes.size() << '\n';
        }
        catch (const std::exception& exception)
        {
            std::cerr << "Registry preflight failed: " << exception.what() << '\n';
            return 1;
        }

        const auto checkReference = [&](json& object, const char* field, const char* expectedType, const std::string& location)
        {
            if (!object.is_object())
            {
                throw std::runtime_error(location + ": expected an object.");
            }

            const auto found = object.find(field);
            if (found == object.end())
            {
                return;
            }

            if (!found->is_string())
            {
                throw std::runtime_error(location + "." + field + ": expected a string.");
            }

            const std::string id = found->get<std::string>();
            if (id.empty())
            {
                return;
            }

            ++referencesChecked;

            const auto asset = assetTypes.find(id);
            if (asset == assetTypes.end())
            {
                std::cerr << location << "." << field << ": missing " << expectedType << " asset ID '" << id << "'.\n";
                ++issues;
                return;
            }

            if (asset->second != expectedType)
            {
                std::cerr << location << "." << field
                          << ": asset ID '" << id
                          << "' has type " << asset->second
                          << "; expected " << expectedType << ".\n";
                ++issues;
                return;
            }

            const auto mapped = uuidMapping.find(id);
            if (mapped == uuidMapping.end() || !mapped->second.IsValid())
            {
                std::cerr << location << "." << field << ": no valid UUID mapping for '" << id << "'.\n";
                ++issues;
                return;
            }

            *found = Quiet::Core::UUIDToString(mapped->second);
            ++referencesTranslated;
        };

        for (int argument = 3; argument < argc; ++argument)
        {
            const fs::path scenePath = argv[argument];
            std::cout << "Scene: " << scenePath.string() << '\n';

            try
            {
                std::ifstream sceneInput(scenePath);
                if (!sceneInput)
                {
                    throw std::runtime_error("Cannot open scene.");
                }

                json scene;
                sceneInput >> scene;

                if (!scene.is_object() ||
                    !scene.contains("actors") ||
                    !scene.at("actors").is_array())
                {
                    throw std::runtime_error("Scene must contain an actors array.");
                }

                std::size_t actorIndex = 0;

                for (auto& actor : scene.at("actors"))
                {
                    const std::string location = scenePath.string() + " actor[" + std::to_string(actorIndex++) + "]";

                    if (!actor.is_object())
                    {
                        throw std::runtime_error(location + ": expected an object.");
                    }

                    if (actor.contains("static_mesh"))
                    {
                        auto& mesh = actor.at("static_mesh");
                        checkReference(mesh, "resource_id", "Mesh", location + ".static_mesh");

                        if (mesh.contains("material_slots"))
                        {
                            auto& slots = mesh.at("material_slots");
                            if (!slots.is_array())
                            {
                                throw std::runtime_error(location + ".static_mesh.material_slots: expected an array.");
                            }

                            std::size_t slotIndex = 0;
                            for (auto& slot : slots)
                            {
                                checkReference(
                                    slot,
                                    "base_color_texture_asset_id",
                                    "Texture",
                                    location + ".static_mesh.material_slots[" + std::to_string(slotIndex++) + "]");
                            }
                        }
                    }

                    if (actor.contains("material"))
                    {
                        checkReference(
                            actor.at("material"),
                            "base_color_texture_asset_id",
                            "Texture",
                            location + ".material");
                    }

                    if (actor.contains("audio_source"))
                    {
                        checkReference(
                            actor.at("audio_source"),
                            "audio_asset_id",
                            "Audio",
                            location + ".audio_source");
                    }

                    if (actor.contains("script"))
                    {
                        auto& script = actor.at("script");

                        checkReference(
                            script,
                            "script_asset_id",
                            "Script",
                            location + ".script");

                        if (script.contains("properties"))
                        {
                            auto& properties = script.at("properties");
                            if (!properties.is_array())
                            {
                                throw std::runtime_error(
                                    location +
                                    ".script.properties: expected an array.");
                            }

                            std::size_t propertyIndex = 0;
                            for (auto& property : properties)
                            {
                                const std::string propertyLocation =
                                    location + ".script.properties[" +
                                    std::to_string(propertyIndex++) + "]";

                                if (!property.is_object() ||
                                    !property.contains("type") ||
                                    !property.at("type").is_string())
                                {
                                    throw std::runtime_error(
                                        propertyLocation +
                                        ": expected a string type.");
                                }

                                const std::string type =
                                    property.at("type").get<std::string>();

                                const char* expectedType = nullptr;
                                if (type == "mesh_asset") expectedType = "Mesh";
                                else if (type == "texture_asset") expectedType = "Texture";
                                else if (type == "audio_asset") expectedType = "Audio";
                                else if (type == "script_asset") expectedType = "Script";

                                if (expectedType)
                                {
                                    checkReference(
                                        property,
                                        "value",
                                        expectedType,
                                        propertyLocation);
                                }
                            }
                        }
                    }
                }
                scenePreviews.emplace_back(scenePath, std::move(scene));
            }
            catch (const std::exception& exception)
            {
                std::cerr << "Scene preflight failed for '"
                          << scenePath.string() << "': "
                          << exception.what() << '\n';
                ++issues;
            }
        }

        std::cout << "References checked: " << referencesChecked << '\n';
        std::cout << "References translated in memory: " << referencesTranslated << '\n';
        std::cout << "Issues: " << issues << '\n';

        if (issues != 0)
        {
            registryPreview = nullptr;
            scenePreviews.clear();
            std::cout << "Preview discarded because issues were found.\n";
        }
        else
        {
            std::cout << "Registry records previewed: " << registryPreview.at("Assets").size() << '\n';
            std::cout << "Scene previews retained: " << scenePreviews.size() << '\n';
        }

        std::cout << "Scope: listed Scene files only. UI documents, Lua source, asset file contents, and other project files were not checked.\n";

        return issues == 0 ? 0 : 1;
    }

    if (argc >= 2 && std::string_view(argv[1]) == "--check-built-in-asset-ids")
    {
        using namespace Quiet::Project::BuiltInAssetIDs;

        const std::array ids{ Triangle, Quad, Plane, Cube, Pyramid, Sphere, Cylinder };
        std::unordered_set<Quiet::Core::UUID> uniqueIDs;
        for (const Quiet::Core::UUID id : ids)
        {
            if (!id.IsValid() || !uniqueIDs.insert(id).second)
            {
                std::cerr << "Invalid or duplicate built-in asset UUID.\n";
                return 1;
            }
        }

        const std::array legacyMappings{
            std::pair{std::string_view{"Triangle"}, Triangle},
            std::pair{std::string_view{"Quad"}, Quad},
            std::pair{std::string_view{"Plane"}, Plane},
            std::pair{std::string_view{"Cube"}, Cube},
            std::pair{std::string_view{"Pyramid"}, Pyramid},
            std::pair{std::string_view{"Sphere"}, Sphere},
            std::pair{std::string_view{"Cylinder"}, Cylinder}
        };

        for (const auto& [legacyID, expectedID] : legacyMappings)
        {
            const auto mappedID = FromLegacyMeshID(legacyID);
            if (!mappedID || *mappedID != expectedID)
            {
                std::cerr << "Incorrect built-in UUID mapping for '" << legacyID << "'.\n";
                return 1;
            }
        }

        if (FromLegacyMeshID("cube") ||
            FromLegacyMeshID("MissingMeshTest"))
        {
            std::cerr << "Unexpected legacy built-in asset mapping.\n";
            return 1;
        }

        Quiet::Project::AssetRegistry registry = Quiet::Project::AssetRegistryIO::MakeDefault();

        registry.Records.push_back({ "Test", Quiet::Project::AssetType::Script, "Assets/Test.lua", false });
        registry.Records.push_back({ "test", Quiet::Project::AssetType::Audio, "Assets/test.wav", false });

        std::unordered_map<std::string, Quiet::Core::UUID> mapping;

        if (!Quiet::Project::AssetRegistryIO::CreateLegacyUUIDMap(registry, mapping))
        {
            std::cerr << "Could not build legacy UUID mapping.\n";
            return 1;
        }

        if (mapping.size() != registry.Records.size() ||
            mapping.at("Cube") != Cube ||
            !mapping.at("Test").IsValid() ||
            !mapping.at("test").IsValid() ||
            mapping.at("Test") == mapping.at("test") ||
            uniqueIDs.contains(mapping.at("Test")) ||
            uniqueIDs.contains(mapping.at("test")))
        {
            std::cerr << "Legacy UUID mapping check failed.\n";
            return 1;
        }

        // A duplicate must fail without changing the previous output.
        const auto previousMapping = mapping;
        registry.Records.push_back(registry.Records.back());

        if (Quiet::Project::AssetRegistryIO::CreateLegacyUUIDMap(registry, mapping) || mapping != previousMapping)
        {
            std::cerr << "Duplicate legacy ID was not rejected safely.\n";
            return 1;
        }

        std::cout << "Built-in asset UUID checks passed: " << uniqueIDs.size() << " unique IDs.\n";
        return 0;
    }

    return -1;
}