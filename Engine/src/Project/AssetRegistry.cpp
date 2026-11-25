#include "Quiet/Project/AssetRegistry.h"

#include <array>
#include <algorithm>
#include <fstream>
#include <utility>
#include <limits>
#include <unordered_set>

#include <nlohmann/json.hpp>

#include "Renderer/Vulkan/Mesh/MeshPrimitives.h"
#include "Quiet/Core/Log.h"
#include "Quiet/Core/UUID.h"
#include "Quiet/Project/BuiltInAssetIDs.h"

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif

    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif

    #include <windows.h>
#endif

namespace Quiet::Project
{
    namespace
    {
        using json = nlohmann::json;

        bool IsValidRegistryRecord(const AssetRecord& record, const std::vector<Core::UUID>& builtInIDs)
        {
            if (!record.IsValid() || record.ID == BuiltInAssetIDs::EditorTopbarLogo) return false;

            const bool isBuiltInID = std::find(builtInIDs.begin(), builtInIDs.end(), record.ID) != builtInIDs.end();
            if (record.BuiltIn != isBuiltInID) return false;

            if (!record.MeshTextureBindings.empty() && (record.BuiltIn || record.Type != AssetType::Mesh))
                return false;

            for (const auto& [source, textureID] : record.MeshTextureBindings)
            {
                if (source.empty() || !textureID.IsValid() || textureID == BuiltInAssetIDs::EditorTopbarLogo ||
                    std::find(builtInIDs.begin(), builtInIDs.end(), textureID) != builtInIDs.end())
                {
                    return false;
                }
            }

            return !isBuiltInID || (record.Type == AssetType::Mesh && record.Path.empty());
        }

        void UpsertRecord(AssetRegistry& registry, AssetRecord record)
        {

            auto it = std::find_if(
                registry.Records.begin(),
                registry.Records.end(),
                [&](const AssetRecord& existing)
                {
                    return existing.ID == record.ID;
                }
            );

            if (it != registry.Records.end())
            {
                *it = std::move(record);
                return;
            }

            registry.Records.push_back(std::move(record));
        }

        void SortRecords(AssetRegistry& registry)
        {
            std::sort(
                registry.Records.begin(),
                registry.Records.end(),
                [](const AssetRecord& lhs, const AssetRecord& rhs)
                {
                    return lhs.ID < rhs.ID;
                }
            );
        }
    }

    AssetRegistry AssetRegistryIO::MakeDefault()
    {
        AssetRegistry registry{};
        EnsureBuiltInAssets(registry);
        return registry;
    }

    bool AssetRegistryIO::LoadFromFile(const std::filesystem::path& path, AssetRegistry& outRegistry)
    {
        std::ifstream input(path);
        if (!input.is_open()) return false;

        try
        {
            json root;
            input >> root;

            if (!root.is_object()) return false;

            // Saving a registry currently writes only these root fields.
            // Reject unknown fields so opening cannot silently discard them.
            for (auto it = root.begin(); it != root.end(); ++it)
            {
                if (it.key() != "Version" && it.key() != "Assets")
                {
                    return false;
                }
            }

            AssetRegistry registry{};

            if (!root.contains("Version") ||
                !root.at("Version").is_number_integer() ||
                root.at("Version") != CurrentAssetRegistryVersion)
            {
                return false;
            }

            registry.Version = CurrentAssetRegistryVersion;

            if (!root.contains("Assets") || !root.at("Assets").is_array())
            {
                return false;
            }

            std::unordered_set<Core::UUID> seenIDs;
            const auto builtInIDs = Renderer::Vulkan::MeshPrimitives::GetBuiltInAssetIDs();

            for (const auto& value : root.at("Assets"))
            {
                if (!value.is_object()) return false;

                // These are the fields SaveToFile knows how to preserve.
                for (auto it = value.begin(); it != value.end(); ++it)
                {
                    if (it.key() != "ID" &&
                        it.key() != "Name" &&
                        it.key() != "Type" &&
                        it.key() != "Path" &&
                        it.key() != "BuiltIn" &&
                        it.key() != "MeshTextureBindings")
                    {
                        return false;
                    }
                }

                AssetRecord record{};
                const std::string idText = value.at("ID").get<std::string>();
                if (!Core::UUIDFromString(idText, record.ID) || Core::UUIDToString(record.ID) != idText)
                {
                    return false;
                }

                record.Name = value.at("Name").get<std::string>();
                record.Path = value.value("Path", "");
                record.BuiltIn = value.value("BuiltIn", false);

                AssetType type = AssetType::Unknown;
                if (!AssetTypeFromString(value.value("Type", "Unknown"), type))
                {
                    return false;
                }

                record.Type = type;

                if (value.contains("MeshTextureBindings"))
                {
                    const auto& bindings = value.at("MeshTextureBindings");
                    if (!bindings.is_object() || record.BuiltIn || record.Type != AssetType::Mesh) return false;

                    for (auto it = bindings.begin(); it != bindings.end(); ++it)
                    {
                        const std::string textureIDText = it.value().get<std::string>();
                        Core::UUID textureID{};
                        if (!Core::UUIDFromString(textureIDText, textureID) || Core::UUIDToString(textureID) != textureIDText)
                        {
                            return false;
                        }
                        record.MeshTextureBindings.emplace(it.key(), textureID);
                    }
                }

                if (!IsValidRegistryRecord(record, builtInIDs) || !seenIDs.insert(record.ID).second)
                {
                    return false;
                }

                registry.Records.push_back(std::move(record));
            }

            EnsureBuiltInAssets(registry);
            SortRecords(registry);

            outRegistry = std::move(registry);
            return true;
        }
        catch (const json::exception&)
        {
            return false;
        }
    }

    bool AssetRegistryIO::SaveToFile(const std::filesystem::path& path, const AssetRegistry& registry)
    {
        const auto builtInIDs = Renderer::Vulkan::MeshPrimitives::GetBuiltInAssetIDs();
        if (registry.Version != CurrentAssetRegistryVersion)
        {
            QUIET_CORE_ERROR("Cannot save asset registry: unsupported version {}.", registry.Version);
            return false;
        }

        std::unordered_set<Core::UUID> seenIDs;

        for (const AssetRecord& record : registry.Records)
        {
            if (!IsValidRegistryRecord(record, builtInIDs) || !seenIDs.insert(record.ID).second)
            {
                QUIET_CORE_ERROR("Cannot save asset registry: invalid record or duplicate UUID '{}'.", Core::UUIDToString(record.ID));
                return false;
            }
        }

        std::error_code directoryError;
        const std::filesystem::path parentPath = path.parent_path();

        if (!parentPath.empty())
        {
            std::filesystem::create_directories(parentPath, directoryError);
            if (directoryError)
            {
                QUIET_CORE_ERROR(
                    "Failed to create asset registry directory '{}': {}",
                    parentPath.string(), directoryError.message());
                return false;
            }
        }

        json root;
        root["Version"] = registry.Version;
        root["Assets"] = json::array();

        for (const auto& record : registry.Records)
        {
            json value = {
                { "ID", Core::UUIDToString(record.ID) },
                { "Name", record.Name },
                { "Type", std::string(ToString(record.Type)) },
                { "Path", record.Path },
                { "BuiltIn", record.BuiltIn }
            };

            if (!record.MeshTextureBindings.empty())
            {
                auto& bindings = value["MeshTextureBindings"] = json::object();
                for (const auto& [source, textureID] : record.MeshTextureBindings)
                    bindings[source] = Core::UUIDToString(textureID);
            }

            root["Assets"].push_back(std::move(value));
        }

        const std::string serialized = root.dump(4);

        #if defined(_WIN32)

        std::filesystem::path temporaryPath = path;
        temporaryPath += ".quiet-tmp-" + Core::UUIDToString(Core::UUIDGenerator::New());

        std::filesystem::path backupPath = path;
        backupPath += ".quiet-backup-" + Core::UUIDToString(Core::UUIDGenerator::New());

        HANDLE temporaryFile = CreateFileW(
            temporaryPath.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (temporaryFile == INVALID_HANDLE_VALUE)
        {
            const DWORD error = GetLastError();
            QUIET_CORE_ERROR(
                "Failed to create temporary asset registry '{}'. Windows error: {}",
                temporaryPath.string(), error);
            return false;
        }

        bool writeSucceeded = true;
        DWORD writeError = ERROR_SUCCESS;
        std::size_t offset = 0;

        while (offset < serialized.size())
        {
            const std::size_t remaining = serialized.size() - offset;
            const DWORD bytesRequested = std::min(remaining, static_cast<std::size_t>((std::numeric_limits<DWORD>::max)()));
            DWORD bytesWritten = 0;

            if (!WriteFile(temporaryFile, serialized.data() + offset, bytesRequested, &bytesWritten, nullptr))
            {
                writeError = GetLastError();
                writeSucceeded = false;
                break;
            }

            if (bytesWritten == 0)
            {
                writeSucceeded = false;
                break;
            }

            offset += bytesWritten;
        }

        if (!CloseHandle(temporaryFile))
        {
            if (writeSucceeded)
            {
                writeError = GetLastError();
            }
            writeSucceeded = false;
        }

        if (!writeSucceeded)
        {
            QUIET_CORE_ERROR("Failed to write temporary asset registry '{}'. Windows error: {}", temporaryPath.string(), writeError);
            return false;
        }

        const DWORD destinationAttributes = GetFileAttributesW(path.c_str());

        if (destinationAttributes == INVALID_FILE_ATTRIBUTES)
        {
            const DWORD destinationError = GetLastError();

            if (destinationError != ERROR_FILE_NOT_FOUND)
            {
                QUIET_CORE_ERROR(
                    "Failed to inspect asset registry destination '{}'. Windows error: {}. Temporary file retained at '{}'.",
                    path.string(), destinationError, temporaryPath.string());
                return false;
            }

            if (!MoveFileW(temporaryPath.c_str(), path.c_str()))
            {
                const DWORD error = GetLastError();
                QUIET_CORE_ERROR(
                    "Failed to create asset registry '{}'. Windows error: {}. Temporary file retained at '{}'.",
                    path.string(), error, temporaryPath.string());
                return false;
            }
        }
        else
        {
            if ((destinationAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                QUIET_CORE_ERROR(
                    "Asset registry destination is a directory: '{}'. Temporary file retained at '{}'.",
                    path.string(), temporaryPath.string());
                return false;
            }

            if (!ReplaceFileW(
                    path.c_str(),
                    temporaryPath.c_str(),
                    backupPath.c_str(),
                    0,
                    nullptr,
                    nullptr))
            {
                const DWORD error = GetLastError();
                QUIET_CORE_ERROR(
                    "Failed to replace asset registry '{}'. Windows error: {}. Check temporary file '{}' and possible backup '{}'.",
                    path.string(), error, temporaryPath.string(), backupPath.string());
                return false;
            }

            if (!DeleteFileW(backupPath.c_str()))
            {
                const DWORD error = GetLastError();
                QUIET_CORE_WARN(
                    "Asset registry saved, but backup '{}' could not be removed. Windows error: {}",
                    backupPath.string(), error);
            }
        }

        #else

        std::ofstream output(path);
        if (!output.is_open())
        {
            return false;
        }

        output << serialized;
        output.close();

        if (!output)
        {
            return false;
        }
        #endif

        return true;
    }

    void AssetRegistryIO::EnsureBuiltInAssets(AssetRegistry& registry)
    {
        using namespace BuiltInAssetIDs;

        const std::array builtIns{
            std::pair{Triangle, "Triangle"},
            std::pair{Quad, "Quad"},
            std::pair{Plane, "Plane"},
            std::pair{Cube, "Cube"},
            std::pair{Pyramid, "Pyramid"},
            std::pair{Sphere, "Sphere"},
            std::pair{Cylinder, "Cylinder"}
        };

        for (const auto& [id, name] : builtIns)
        {
            UpsertRecord(registry, AssetRecord{ id, name, AssetType::Mesh, "", true });
        }

        SortRecords(registry);
    }

    bool AssetRegistryIO::AddOrUpdate(AssetRegistry& registry, AssetRecord record)
    {
        const auto builtInIDs = Renderer::Vulkan::MeshPrimitives::GetBuiltInAssetIDs();
        if (record.BuiltIn || !IsValidRegistryRecord(record, builtInIDs)) return false;

        UpsertRecord(registry, std::move(record));
        SortRecords(registry);
        return true;
    }

    bool AssetRegistryIO::RemoveByID(AssetRegistry &registry, Core::UUID id)
    {
        const auto it = std::remove_if(
            registry.Records.begin(),
            registry.Records.end(),
            [&](const AssetRecord& record)
            {
                return record.ID == id;
            });

        if (it == registry.Records.end()) return false;
        registry.Records.erase(it, registry.Records.end());
        SortRecords(registry);
        return true;
    }

    const AssetRecord * AssetRegistryIO::FindByID(const AssetRegistry &registry, Core::UUID id)
    {
        const auto it = std::find_if(
                    registry.Records.begin(),
                    registry.Records.end(),
                    [&](const AssetRecord& record)
                    {
                        return record.ID == id;
                    }
                );

        if (it == registry.Records.end()) return nullptr;
        return &(*it);
    }

    std::vector<const AssetRecord *> AssetRegistryIO::GetAssetsByType(const AssetRegistry &registry, AssetType type)
    {
        std::vector<const AssetRecord*> result;
        result.reserve(registry.Records.size());

        for (const auto& record : registry.Records)
        {
            if (record.Type == type && record.IsValid())
                result.push_back(&record);
        }
        return result;
    }

    std::vector<Core::UUID> AssetRegistryIO::GetAssetIDsByType(const AssetRegistry &registry, AssetType type)
    {
        std::vector<Core::UUID> result;
        result.reserve(registry.Records.size());

        for (const auto& record : registry.Records)
        {
            if (record.Type == type && record.IsValid())
                result.push_back(record.ID);
        }
        std::sort(result.begin(), result.end());
        return result;
    }
}
