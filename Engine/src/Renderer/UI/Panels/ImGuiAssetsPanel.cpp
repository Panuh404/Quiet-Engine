#include "Renderer/UI/ImGuiLayer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

#include <glm/vec3.hpp>
#include <imgui.h>
#include <imgui_internal.h>

#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/ProjectContext.h"
#include "Quiet/Platform/Shell.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/UI/Assets/ImGuiAssetFileDialog.h"
#include "Renderer/UI/ImGuiTexturePreview.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/Vulkan/Texture/TextureLibrary.h"
#include "Scene/Scene.h"

namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;

    enum class AssetBrowserTypeFilter
    {
        All, Meshes, Textures
    };

    enum class AssetBrowserViewMode
    {
        List,
        Browser
    };

    enum class AssetBrowserThumbnailSize
    {
        Small,
        Medium,
        Large
    };

    std::string ToLowerCopy(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](const unsigned char c)
            {
                return static_cast<char>(std::tolower(c));
            }
        );
        return value;
    }

    bool ContainsCaseInsensitive(const std::string& text, const std::string& query)
    {
        if (query.empty()) return true;

        const std::string lowerText = ToLowerCopy(text);
        const std::string lowerQuery = ToLowerCopy(query);
        return lowerText.find(lowerQuery) != std::string::npos;
    }

    bool MatchesTypeFilter(const Quiet::Project::AssetRecord& record, const AssetBrowserTypeFilter filter)
    {
        switch (filter)
        {
            case AssetBrowserTypeFilter::All:
                return true;
            case AssetBrowserTypeFilter::Meshes:
                return record.Type == Quiet::Project::AssetType::Mesh;
            case AssetBrowserTypeFilter::Textures:
                return record.Type == Quiet::Project::AssetType::Texture;
            default:
                return true;
        }
    }

    bool MatchesAssetSearch(const Quiet::Project::AssetRecord& record, const std::string& query)
    {
        if (query.empty()) return true;
        if (ContainsCaseInsensitive(record.ID, query)) return true;
        if (ContainsCaseInsensitive(record.Path, query)) return true;

        const std::string typeLabel = std::string(Quiet::Project::ToString(record.Type));
        return ContainsCaseInsensitive(typeLabel, query);
    }

    std::vector<const Quiet::Project::AssetRecord*> BuildVisibleAssetList(const Quiet::Project::AssetRegistry& registry,
                                         const AssetBrowserTypeFilter typeFilter,
                                         const std::string& searchQuery)
    {
        std::vector<const Quiet::Project::AssetRecord*> visibleAssets;
        visibleAssets.reserve(registry.Records.size());

        for (const Quiet::Project::AssetRecord& record : registry.Records)
        {
            if (!record.IsValid()) continue;
            if (!MatchesTypeFilter(record, typeFilter)) continue;
            if (!MatchesAssetSearch(record, searchQuery)) continue;
            visibleAssets.push_back(&record);
        }
        return visibleAssets;
    }

    std::size_t CountAssetByType(const Quiet::Project::AssetRegistry& registry, const Quiet::Project::AssetType type)
    {
        std::size_t count = 0;
        for (const Quiet::Project::AssetRecord& record : registry.Records)
        {
            if (record.IsValid() && record.Type == type) ++count;
        }
        return count;
    }

    template <std::size_t Size>
    void CopyToBuffer(std::array<char, Size>& buffer, const std::string& value)
    {
        std::snprintf(buffer.data(), buffer.size(), "%s", value.c_str());
    }

    std::string ExtractFileNameNoExt(const std::string& path)
    {
        return std::filesystem::path(path).stem().string();
    }

    std::filesystem::path ResolveDisplayPath(const Quiet::Project::ProjectContext& projectContext, const std::string& assetPath)
    {
        std::filesystem::path path = assetPath;
        if (path.empty())
        {
            return {};
        }

        if (path.is_absolute())
        {
            return path.lexically_normal();
        }

        return (projectContext.GetRootPath() / path).lexically_normal();
    }

    bool DoesAssetPathExist(const Quiet::Project::ProjectContext& projectContext, const std::string& assetPath)
    {
        const std::filesystem::path resolvedPath = ResolveDisplayPath(projectContext, assetPath);
        return !resolvedPath.empty() && std::filesystem::exists(resolvedPath);
    }

    std::string MakeStatusLabel(const Quiet::Project::AssetRecord& record,
                                const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary,
                                const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary)
    {
        if (record.Type == Quiet::Project::AssetType::Mesh)
        {
            if (meshLibrary.TryGet(record.ID))
                return "Loaded";

            if (record.BuiltIn)
                return "Missing Built-In";

            return "Registered";
        }

        if (record.Type == Quiet::Project::AssetType::Texture)
        {
            const auto textureStats = textureLibrary.GetStats(record.ID);
            if (textureStats.has_value())
                return textureStats->GpuReady ? "GPU Ready" : "Loaded Metadata";

            if (record.Path.empty() && !record.BuiltIn)
                return "Missing Path";

            return "Registered";
        }

        return "Unknown";
    }

    ImVec4 MakeStatusColor(const std::string& status)
    {
        if (status == "Loaded") return QUI::EditorColor::Success;
        if (status == "GPU Ready") return QUI::EditorColor::Success;
        if (status == "Loaded Metadata") return QUI::EditorColor::Success;
        if (status == "Registered") return QUI::EditorColor::Warning;
        if (status == "Missing Built-In") return QUI::EditorColor::Error;
        if (status == "Missing Path") return QUI::EditorColor::Error;
        return QUI::EditorColor::TextMuted;
    }

    void DrawPillLabel(const char* text, const ImVec4& textColor, const ImVec4& backgroundColor)
    {
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        const ImVec2 padding = SZ::Px(ImVec2(8.0f, 3.0f));
        const float rounding = SZ::Px(6.0f);

        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max = ImVec2(
            min.x + textSize.x + padding.x * 2.0f,
            min.y + textSize.y + padding.y * 2.0f
        );

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->AddRectFilled(
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(backgroundColor),
            rounding
        );

        drawList->AddRect(
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::BorderSoft),
            rounding
        );

        drawList->AddText(
            ImVec2(min.x + padding.x, min.y + padding.y),
            ImGui::ColorConvertFloat4ToU32(textColor),
            text
        );

        ImGui::Dummy(ImVec2(max.x - min.x, max.y - min.y));
    }

    void DrawAssetTypePill(const Quiet::Project::AssetType type)
    {
        const std::string typeLabel = std::string(Quiet::Project::ToString(type));

        ImVec4 background = QUI::EditorColor::Background3;
        ImVec4 text = QUI::EditorColor::TextMuted;

        if (type == Quiet::Project::AssetType::Mesh)
        {
            background = ImVec4(0.045f, 0.100f, 0.075f, 1.0f);
            text = QUI::EditorColor::Success;
        }
        else if (type == Quiet::Project::AssetType::Texture)
        {
            background = ImVec4(0.075f, 0.055f, 0.115f, 1.0f);
            text = ImVec4(0.760f, 0.620f, 1.000f, 1.0f);
        }
        DrawPillLabel(typeLabel.c_str(), text, background);
    }

    std::string MakeAssetDisplayName(const Quiet::Project::AssetRecord& record)
    {
        if (!record.Path.empty())
        {
            const std::string filename = std::filesystem::path(record.Path).filename().string();
            if (!filename.empty()) return filename;
        }
        return record.ID.empty() ? "<unnamed>" : record.ID;
    }

    bool IsAssetReadyStatus(const std::string& status)
    {
        return status == "Loaded" ||
               status == "GPU Ready" ||
               status == "Loaded Metadata";
    }

    bool IsAssetMissingStatus(const std::string& status)
    {
        return status == "Missing Built-In" ||
               status == "Missing Path";
    }

    std::string MakeAssetBrowserStateLabel(const Quiet::Project::AssetRecord& record,
                                           const std::string& status,
                                           const bool runtimeModeActive,
                                           const std::uint32_t referenceCount)
    {
        if (IsAssetMissingStatus(status)) return "Missing";
        const std::string readiness = IsAssetReadyStatus(status) ? "Ready" : status;

        if (record.BuiltIn) return "Built-in · " + readiness;
        if (referenceCount > 0) return readiness + " · In Use";
        if (runtimeModeActive) return readiness + " · Runtime";

        return readiness + " · Removable";
    }

    void DrawAssetStatePill(const std::string& state)
    {
        ImVec4 text = QUI::EditorColor::TextMuted;
        ImVec4 background = QUI::EditorColor::Background3;

        if (state.find("Missing") != std::string::npos)
        {
            text = QUI::EditorColor::Error;
            background = ImVec4(0.140f, 0.045f, 0.050f, 1.0f);
        }
        else if (state.find("Built-in") != std::string::npos)
        {
            text = QUI::EditorColor::Accent;
            background = ImVec4(0.040f, 0.075f, 0.135f, 1.0f);
        }
        else if (state.find("In Use") != std::string::npos)
        {
            text = QUI::EditorColor::Warning;
            background = ImVec4(0.130f, 0.095f, 0.030f, 1.0f);
        }
        else if (state.find("Removable") != std::string::npos)
        {
            text = QUI::EditorColor::Success;
            background = ImVec4(0.035f, 0.105f, 0.065f, 1.0f);
        }
        else if (state.find("Ready") != std::string::npos)
        {
            text = QUI::EditorColor::Success;
            background = ImVec4(0.035f, 0.105f, 0.065f, 1.0f);
        }

        DrawPillLabel(state.c_str(), text, background);
    }

    bool DrawAssetTableCellHitTarget(const char* id, const float rowHeight)
    {
        const float cellWidth = ImGui::GetContentRegionAvail().x;
        if (cellWidth <= SZ::Px(4.0f)) return false;

        ImGui::InvisibleButton(id, ImVec2(cellWidth, rowHeight));
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        return ImGui::IsItemClicked(ImGuiMouseButton_Left);
    }

    struct AssetBrowserCardMetrics
    {
        float DesiredColumnWidth = 178.0f;
        float MinimumCardWidth = 150.0f;
        float CardHeight = 192.0f;
        float Padding = 10.0f;
        float PreviewHeight = 86.0f;
    };

    AssetBrowserCardMetrics MakeAssetBrowserCardMetrics(const AssetBrowserThumbnailSize thumbnailSize)
    {
        switch (thumbnailSize)
        {
            case AssetBrowserThumbnailSize::Small:
                return {
                    SZ::Px(142.0f),
                    SZ::Px(120.0f),
                    SZ::Px(132.0f),
                    SZ::Px(8.0f),
                    SZ::Px(62.0f)
                };

            case AssetBrowserThumbnailSize::Large:
                return {
                    SZ::Px(230.0f),
                    SZ::Px(190.0f),
                    SZ::Px(218.0f),
                    SZ::Px(12.0f),
                    SZ::Px(126.0f)
                };

            case AssetBrowserThumbnailSize::Medium:
            default:
                return {
                    SZ::Px(178.0f),
                    SZ::Px(150.0f),
                    SZ::Px(166.0f),
                    SZ::Px(10.0f),
                    SZ::Px(86.0f)
                };
        }
    }

    void DrawAssetPaddedTooltip(const std::string& text)
    {
        if (text.empty()) return;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(12.0f, 9.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, SZ::Px(8.0f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        ImGui::BeginTooltip();
        {
            ImGui::PushTextWrapPos(SZ::Px(520.0f));

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextWrapped("%s", text.c_str());
            ImGui::PopStyleColor();

            ImGui::PopTextWrapPos();
        }
        ImGui::EndTooltip();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    std::string MakeEllipsizedText(const std::string& text, const float maxWidth)
    {
        if (text.empty()) return {};
        if (ImGui::CalcTextSize(text.c_str()).x <= maxWidth) return text;

        constexpr const char* ellipsis = "...";
        const float ellipsisWidth = ImGui::CalcTextSize(ellipsis).x;

        if (ellipsisWidth >= maxWidth) return ellipsis;

        std::size_t low = 0;
        std::size_t high = text.size();

        while (low < high)
        {
            const std::size_t mid = (low + high + 1) / 2;
            const std::string candidate = text.substr(0, mid) + ellipsis;

            if (ImGui::CalcTextSize(candidate.c_str()).x <= maxWidth) low = mid;
            else high = mid - 1;
        }

        return text.substr(0, low) + ellipsis;
    }

    void DrawAssetCardTextLine(const std::string& text, const ImVec2& min, const ImVec2& max, const ImVec4& color)
    {
        const float maxWidth = std::max(0.0f, max.x - min.x);
        const std::string visibleText = MakeEllipsizedText(text, maxWidth);

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        drawList->PushClipRect(min, max, true);
        drawList->AddText(min, ImGui::ColorConvertFloat4ToU32(color), visibleText.c_str());
        drawList->PopClipRect();
    }

    bool DrawRegisterAssetForm(Quiet::Project::ProjectContext& projectContext,
                               Quiet::Editor::EditorState& editorState,
                               std::array<char, 128>& assetIDBuffer,
                               std::array<char, 260>& assetPathBuffer,
                               std::string& errorMessage)
    {
        bool registered = false;

        const std::string currentPath = assetPathBuffer.data();
        const bool hasPath = !currentPath.empty();
        const bool fileExists = hasPath && DoesAssetPathExist(projectContext, currentPath);

        const Quiet::Project::AssetType detectedType =
            hasPath
                ? Quiet::Renderer::UI::DetectImportAssetType(currentPath)
                : Quiet::Project::AssetType::Unknown;

        const bool supportedType = detectedType != Quiet::Project::AssetType::Unknown;

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(10.0f, 7.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(10.0f, 8.0f)));

        ImGui::TextUnformatted("Asset ID");
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint(
            "##AssetID",
            "Leave empty to auto-generate from filename",
            assetIDBuffer.data(),
            assetIDBuffer.size()
        );

        ImGui::TextUnformatted("Source Path");

        const float browseWidth = SZ::Px(96.0f);
        ImGui::SetNextItemWidth(-(browseWidth + ImGui::GetStyle().ItemSpacing.x));
        ImGui::InputTextWithHint(
            "##AssetPath",
            "Assets/Meshes/model.obj or Assets/Textures/albedo.png",
            assetPathBuffer.data(),
            assetPathBuffer.size()
        );

        ImGui::SameLine();
        if (QUI::EditorButton("Browse", ImVec2(browseWidth, 0.0f)))
            Quiet::Renderer::UI::OpenAssetFileDialog(projectContext);

        if (!hasPath)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextUnformatted("Select or paste a supported asset file.");
            ImGui::PopStyleColor();
        }
        else if (!supportedType)
        {
            ImGui::TextColored(QUI::EditorColor::Error, "Unsupported asset type");
        }
        else if (fileExists)
        {
            const std::string typeLabel = std::string(Quiet::Project::ToString(detectedType));
            ImGui::TextColored(QUI::EditorColor::Success, "%s file found", typeLabel.c_str());
        }
        else
        {
            ImGui::TextColored(QUI::EditorColor::Error, "File not found");
        }

        ImGui::Spacing();
        const bool canRegister = hasPath && supportedType && fileExists;

        if (QUI::EditorButton("Import", SZ::Px(ImVec2(125.0f, 0.0f)), canRegister))
        {
            std::string assetID = assetIDBuffer.data();
            std::string assetPath = assetPathBuffer.data();

            if (assetID.empty()) assetID = ExtractFileNameNoExt(assetPath);
            bool success = false;

            switch (detectedType)
            {
                case Quiet::Project::AssetType::Mesh:
                    success = projectContext.RegisterMeshAsset(assetID, assetPath, false);
                    if (success) editorState.QueueRequest(Quiet::Editor::EditorRequestType::ReloadAssets);
                    break;

                case Quiet::Project::AssetType::Texture:
                    success = projectContext.RegisterTextureAsset(assetID, assetPath, false);
                    if (success) editorState.QueueRequest(Quiet::Editor::EditorRequestType::ReloadAssets);
                    break;

                case Quiet::Project::AssetType::Unknown:
                default:
                    success = false;
                    break;
            }

            if (success)
            {
                assetIDBuffer[0] = '\0';
                assetPathBuffer[0] = '\0';
                registered = true;
            }
            else
            {
                errorMessage = "Failed to import asset. Check that the Asset ID is valid and the file type is supported.";
                ImGui::OpenPopup("Import Asset Failed");
            }
        }

        ImGui::PopStyleVar(2);
        return registered;
    }

    void DrawImportAssetPopup(Quiet::Project::ProjectContext& projectContext,
                              Quiet::Editor::EditorState& editorState,
                              std::array<char, 128>& assetIDBuffer,
                              std::array<char, 260>& assetPathBuffer,
                              std::string& errorMessage)
    {
        ImGui::SetNextWindowSize(SZ::Px(ImVec2(640.0f, 0.0f)), ImGuiCond_Appearing);
        bool importPopupOpen = true;

        if (!QUI::EditorBeginPopupModal("Import Asset", importPopupOpen, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        // Title
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextUnformatted("Import Asset");
        ImGui::PopStyleColor();

        // Subtitle
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::TextWrapped("%s", "Register a supported mesh or texture file as a project asset.");
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Separator, QUI::EditorColor::BorderSoft);
        ImGui::Separator();
        ImGui::PopStyleColor();

        if (DrawRegisterAssetForm(projectContext, editorState, assetIDBuffer, assetPathBuffer, errorMessage))
        {
            editorState.AssetOperationFeedback.Set(
                Quiet::Editor::EditorFeedbackSeverity::Success,
                "Asset imported",
                "Asset import succeeded. Project assets will be reloaded."
            );
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (QUI::EditorButton("Cancel", SZ::Px(ImVec2(125.0f, 0.0f))))
        {
            errorMessage.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    void DrawFailurePopup(const char* title, const std::string& message)
    {
        bool popupOpen = true;
        if (QUI::EditorBeginPopupModal(title, popupOpen, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("%s", message.c_str());
            ImGui::Spacing();

            if (QUI::EditorButton("OK", SZ::Px(ImVec2(120.0f, 0.0f))))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
    }

    ImVec4 MakeAssetFeedbackTextColor(const Quiet::Editor::EditorFeedbackSeverity severity)
    {
        switch (severity)
        {
            case Quiet::Editor::EditorFeedbackSeverity::Success: return QUI::EditorColor::Success;
            case Quiet::Editor::EditorFeedbackSeverity::Warning: return QUI::EditorColor::Warning;
            case Quiet::Editor::EditorFeedbackSeverity::Error: return QUI::EditorColor::Error;
            case Quiet::Editor::EditorFeedbackSeverity::Info:
            default: return QUI::EditorColor::Accent;
        }
    }

    ImVec4 MakeAssetFeedbackBackgroundColor(const Quiet::Editor::EditorFeedbackSeverity severity)
    {
        switch (severity)
        {
            case Quiet::Editor::EditorFeedbackSeverity::Success: return {0.035f, 0.105f, 0.065f, 1.0f};
            case Quiet::Editor::EditorFeedbackSeverity::Warning: return {0.130f, 0.095f, 0.030f, 1.0f};
            case Quiet::Editor::EditorFeedbackSeverity::Error: return {0.140f, 0.045f, 0.050f, 1.0f};
            case Quiet::Editor::EditorFeedbackSeverity::Info:
            default: return {0.040f, 0.075f, 0.135f, 1.0f};
        }
    }

    void DrawAssetOperationFeedback(Quiet::Editor::EditorState& editorState)
    {
        auto& feedback = editorState.AssetOperationFeedback;
        if (!feedback.Visible) return;

        const ImVec4 accentColor = MakeAssetFeedbackTextColor(feedback.Severity);
        const ImVec4 backgroundColor = MakeAssetFeedbackBackgroundColor(feedback.Severity);

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(12.0f, 10.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, backgroundColor);
        ImGui::PushStyleColor(ImGuiCol_Border, accentColor);

        ImGui::BeginChild("##AssetOperationFeedback", ImVec2(0.0f, SZ::Px(74.0f)),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        {
            const std::string title = feedback.Title.empty() ? "Asset operation" : feedback.Title;
            const std::string message = feedback.Message.empty() ? "Operation completed." : feedback.Message;

            ImGui::PushStyleColor(ImGuiCol_Text, accentColor);
            ImGui::TextUnformatted(title.c_str());
            ImGui::PopStyleColor();

            const float dismissWidth = SZ::Px(82.0f);
            const float dismissX = ImGui::GetWindowContentRegionMax().x - dismissWidth;

            ImGui::SameLine();

            if (dismissX > ImGui::GetCursorPosX())
                ImGui::SetCursorPosX(dismissX);

            if (QUI::EditorButton("Dismiss", ImVec2(dismissWidth, 0.0f)))
                feedback.Clear();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextWrapped("%s", message.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);

        ImGui::Spacing();
    }

    std::uint32_t CountMeshReferences(const Quiet::Scene::Scene& scene, const std::string& meshID)
    {
        std::uint32_t count = 0;
        for (const auto& actor : scene.GetActors())
        {
            if (!actor.StaticMesh.has_value()) continue;
            if (actor.StaticMesh->MeshResourceID == meshID) ++count;
        }
        return count;
    }

    void DrawDetailRow(const char* label, const char* value, const float labelWidth)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        ImGui::SameLine(labelWidth);

        if (value && value[0] != '\0')
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted(value);
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::TextUnformatted("-");
            ImGui::PopStyleColor();
        }
    }

    void DrawDetailRowWrapped(const char* label, const std::string& value, const float labelWidth)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        ImGui::SameLine(labelWidth);

        if (value.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::TextUnformatted("-");
            ImGui::PopStyleColor();
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextWrapped("%s", value.c_str());
        ImGui::PopStyleColor();
    }

    void DrawDetailRowUInt(const char* label, const std::uint32_t value, const float labelWidth)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        ImGui::SameLine(labelWidth);

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::Text("%u", value);
        ImGui::PopStyleColor();
    }

    void DrawDetailRowVec3(const char* label, const glm::vec3& value, const float labelWidth)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::Text("%s", label);
        ImGui::PopStyleColor();

        ImGui::SameLine(labelWidth);

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::Text("%.3f, %.3f, %.3f", value.x, value.y, value.z);
        ImGui::PopStyleColor();
    }

    void DrawDetailsSectionTitle(const char* title)
    {
        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Separator, QUI::EditorColor::BorderSoft);
        ImGui::Separator();
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextUnformatted(title);
        ImGui::PopStyleColor();

        ImGui::PushStyleColor(ImGuiCol_Separator, QUI::EditorColor::BorderSoft);
        ImGui::Separator();
        ImGui::PopStyleColor();
    }

    void DrawAssetDetailsPopup(const Quiet::Project::ProjectContext& projectContext,
                               const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary,
                               const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary,
                               const std::string& assetID,
                               const float uiScale)
    {
        if (assetID.empty()) return;

        const auto* record = Quiet::Project::AssetRegistryIO::FindByID(
            projectContext.GetAssetRegistry(),
            assetID
        );
        if (!record) return;

        const auto stats = meshLibrary.GetStats(record->ID);
        const std::string status = MakeStatusLabel(*record, meshLibrary, textureLibrary);
        const ImVec4 statusColor = MakeStatusColor(status);

        const float labelWidth = 128.0f * uiScale;
        const float modalWidth = 640.0f * uiScale;

        ImGui::SetNextWindowSize(ImVec2(modalWidth, 0.0f), ImGuiCond_Appearing);

        bool assetDetailsOpen = true;
        if (QUI::EditorBeginPopupModal("Asset Details", assetDetailsOpen, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f * uiScale, 7.0f * uiScale));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f * uiScale, 8.0f * uiScale));

            ImGui::TextUnformatted(record->ID.c_str());
            ImGui::SameLine();
            ImGui::TextColored(statusColor, "  %s", status.c_str());

            DrawDetailsSectionTitle("Asset Record");

            const std::string typeLabel = std::string(Quiet::Project::ToString(record->Type));

            DrawDetailRow("ID", record->ID.c_str(), labelWidth);
            DrawDetailRow("Type", typeLabel.c_str(), labelWidth);
            DrawDetailRow("Status", status.c_str(), labelWidth);
            DrawDetailRow("Built In", record->BuiltIn ? "true" : "false", labelWidth);

            if (record->Path.empty())
                DrawDetailRow("Path", "<built-in>", labelWidth);
            else
                DrawDetailRowWrapped("Path", record->Path, labelWidth);

            if (record->Type == Quiet::Project::AssetType::Mesh)
            {
                DrawDetailsSectionTitle("Runtime Mesh");
                if (stats.has_value())
                {
                    DrawDetailRowWrapped("Importer", stats->ImporterName, labelWidth);
                    DrawDetailRowUInt("Meshes", stats->ImportedMeshCount, labelWidth);
                    DrawDetailRowUInt("Submeshes", stats->SubmeshCount, labelWidth);
                    DrawDetailRowUInt("Materials", stats->MaterialCount, labelWidth);
                    DrawDetailRowUInt("Vertices", stats->VertexCount, labelWidth);
                    DrawDetailRowUInt("Indices", stats->IndexCount, labelWidth);
                    DrawDetailRowUInt("Warnings", stats->WarningCount, labelWidth);

                    DrawDetailsSectionTitle("Bounds");

                    if (stats->BoundsValid)
                    {
                        DrawDetailRowVec3("Min", stats->BoundsMin, labelWidth);
                        DrawDetailRowVec3("Max", stats->BoundsMax, labelWidth);

                        const glm::vec3 size = stats->BoundsMax - stats->BoundsMin;
                        DrawDetailRowVec3("Size", size, labelWidth);
                    }
                    else
                    {
                        ImGui::TextDisabled("No bounds available.");
                    }

                    DrawDetailsSectionTitle("Import Warnings");
                    if (stats->Warnings.empty())
                        ImGui::TextDisabled("No import warnings.");
                    else
                    {
                        ImGui::BeginChild(
                            "AssetImportWarnings",
                            ImVec2(0.0f, 120.0f * uiScale),
                            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding,
                            ImGuiWindowFlags_None
                        );

                        for (const std::string& warning : stats->Warnings)
                        {
                            ImGui::BulletText("%s", warning.c_str());
                        }
                        ImGui::EndChild();
                    }
                }
                else
                    ImGui::TextDisabled("This mesh asset is not currently loaded.");
            }
            else if (record->Type == Quiet::Project::AssetType::Texture)
            {
                DrawDetailsSectionTitle("Source Texture");
                const auto textureStats = textureLibrary.GetStats(record->ID);

                if (!textureStats.has_value())
                {
                    ImGui::TextDisabled("Texture metadata is not loaded.");
                    ImGui::TextDisabled("Use Reload Meshes/Assets after registering the texture.");
                }
                else
                {
                    DrawDetailRow("Format", textureStats->Format.c_str(), labelWidth);

                    if (textureStats->DimensionsValid)
                    {
                        ImGui::TextDisabled("Dimensions");
                        ImGui::SameLine(labelWidth);
                        ImGui::Text("%u x %u", textureStats->Width, textureStats->Height);
                    }
                    else
                    {
                        DrawDetailRow("Dimensions", "Unknown", labelWidth);
                    }

                    DrawDetailRowWrapped(
                        "File Size",
                        std::to_string(textureStats->FileSizeBytes) + " bytes",
                        labelWidth
                    );
                    DrawDetailRowWrapped(
                        "Pixel Bytes",
                        std::to_string(textureStats->PixelBytes) + " bytes",
                        labelWidth
                    );
                    DrawDetailRow(
                        "GPU Ready",
                        textureStats->GpuReady ? "true" : "false",
                        labelWidth
                    );

                    if (textureStats->GpuReady)
                    {
                        DrawDetailsSectionTitle("Preview");
                        Quiet::Renderer::UI::DrawTexturePreview(
                            record->ID,
                            textureLibrary,
                            textureStats.value(),
                            uiScale
                        );
                    }

                    DrawDetailsSectionTitle("Texture Warnings");

                    DrawDetailRowUInt(
                        "Warnings",
                        static_cast<std::uint32_t>(textureStats->Warnings.size()),
                        labelWidth
                    );
                    if (textureStats->Warnings.empty())
                    {
                        ImGui::TextDisabled("No texture warnings.");
                    }
                    else
                    {
                        for (const std::string& warning : textureStats->Warnings)
                        {
                            ImGui::BulletText("%s", warning.c_str());
                        }
                    }

                    DrawDetailsSectionTitle("Runtime Texture");
                    ImGui::TextDisabled("GPU texture resource is loaded and available for material sampling.");
                }
            }

            ImGui::PopStyleVar(2);
            ImGui::EndPopup();
        }
    }

    std::string MakeAssetTypeFilterDropdownLabel(const AssetBrowserTypeFilter filter, const std::size_t meshCount, const std::size_t textureCount)
    {
        switch (filter)
        {
            case AssetBrowserTypeFilter::Meshes:
                return "Filter: Meshes " + std::to_string(meshCount);
            case AssetBrowserTypeFilter::Textures:
                return "Filter: Textures " + std::to_string(textureCount);
            case AssetBrowserTypeFilter::All:
            default:
                return "Filter: All";
        }
    }

    const char* ToAssetBrowserViewModeLabel(const AssetBrowserViewMode viewMode)
    {
        switch (viewMode)
        {
            case AssetBrowserViewMode::Browser:
                return "Browser";
            case AssetBrowserViewMode::List:
            default:
                return "List";
        }
    }

    const char* ToAssetBrowserThumbnailSizeLabel(const AssetBrowserThumbnailSize thumbnailSize)
    {
        switch (thumbnailSize)
        {
            case AssetBrowserThumbnailSize::Small:
                return "Small";
            case AssetBrowserThumbnailSize::Large:
                return "Large";
            case AssetBrowserThumbnailSize::Medium:
            default:
                return "Medium";
        }
    }

    bool DrawAssetToolbarDropdownButton(const std::string& label, const char* popupID, const ImVec2& size)
    {
        if (QUI::EditorButton(label.c_str(), size))
        {
            ImGui::OpenPopup(popupID);
        }
        return ImGui::BeginPopup(popupID);
    }

    void DrawAssetBrowserToolbar(Quiet::Editor::EditorState& editorState,
                                 const Quiet::Project::AssetRegistry& registry,
                                 std::array<char, 128>& searchBuffer,
                                 AssetBrowserTypeFilter& typeFilter,
                                 AssetBrowserViewMode& viewMode,
                                 AssetBrowserThumbnailSize& thumbnailSize,
                                 bool& openImportPopup)
    {
        const std::size_t meshCount = CountAssetByType(registry, Quiet::Project::AssetType::Mesh);
        const std::size_t textureCount = CountAssetByType(registry, Quiet::Project::AssetType::Texture);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(10.f, 7.f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(8.0f, 8.0f)));

        if (QUI::EditorButton("Import", SZ::Px(ImVec2(86.0f, 0.0f))))
            openImportPopup = true;

        ImGui::SameLine();

        if (QUI::EditorButton("Reload", SZ::Px(ImVec2(86.0f, 0.0f))))
            editorState.QueueRequest(Quiet::Editor::EditorRequestType::ReloadAssets);

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        ImGui::SetNextItemWidth(std::max(SZ::Px(220.0f), ImGui::GetContentRegionAvail().x * 0.38f));
        ImGui::InputTextWithHint("##AssetBrowserSearch", "Search assets by id, path, or type...", searchBuffer.data(), searchBuffer.size());
        ImGui::SameLine();

        if (searchBuffer[0] != '\0')
        {
            ImGui::SameLine();
            if (QUI::EditorButton("Clear", SZ::Px(ImVec2(72.0f, 0.0f))))
                searchBuffer[0] = '\0';
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        const std::size_t totalAssetCount = registry.Records.size();
        const std::string filterDropdownLabel = MakeAssetTypeFilterDropdownLabel(typeFilter, meshCount, textureCount);
        if (DrawAssetToolbarDropdownButton(filterDropdownLabel, "##AssetTypeFilterDropdown", SZ::Px(ImVec2(140.0f, 0.0f))))
        {
            const std::string allLabel = "All " + std::to_string(totalAssetCount);
            const std::string meshesLabel = "Meshes " + std::to_string(meshCount);
            const std::string texturesLabel = "Textures " + std::to_string(textureCount);

            if (QUI::EditorMenuItem(allLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::All))
                typeFilter = AssetBrowserTypeFilter::All;

            if (QUI::EditorMenuItem(meshesLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::Meshes))
                typeFilter = AssetBrowserTypeFilter::Meshes;

            if (QUI::EditorMenuItem(texturesLabel.c_str(), nullptr, typeFilter == AssetBrowserTypeFilter::Textures))
                typeFilter = AssetBrowserTypeFilter::Textures;

            ImGui::EndPopup();
        }

        ImGui::SameLine();

        const std::string viewDropdownLabel = std::string("View: ") + ToAssetBrowserViewModeLabel(viewMode);
        if (DrawAssetToolbarDropdownButton(viewDropdownLabel, "##AssetViewModeDropdown", SZ::Px(ImVec2(140.0f, 0.0f))))
        {
            if (QUI::EditorMenuItem("List", nullptr, viewMode == AssetBrowserViewMode::List))
                viewMode = AssetBrowserViewMode::List;

            if (QUI::EditorMenuItem("Browser", nullptr, viewMode == AssetBrowserViewMode::Browser))
                viewMode = AssetBrowserViewMode::Browser;

            ImGui::EndPopup();
        }

        if (viewMode == AssetBrowserViewMode::Browser)
        {
            ImGui::SameLine();

            const std::string sizeDropdownLabel = std::string("Size: ") + ToAssetBrowserThumbnailSizeLabel(thumbnailSize);
            if (DrawAssetToolbarDropdownButton(sizeDropdownLabel, "##AssetThumbnailSizeDropdown", SZ::Px(ImVec2(140.0f, 0.0f))))
            {
                if (QUI::EditorMenuItem("Small", nullptr, thumbnailSize == AssetBrowserThumbnailSize::Small))
                    thumbnailSize = AssetBrowserThumbnailSize::Small;

                if (QUI::EditorMenuItem("Medium", nullptr, thumbnailSize == AssetBrowserThumbnailSize::Medium))
                    thumbnailSize = AssetBrowserThumbnailSize::Medium;

                if (QUI::EditorMenuItem("Large", nullptr, thumbnailSize == AssetBrowserThumbnailSize::Large))
                    thumbnailSize = AssetBrowserThumbnailSize::Large;

                ImGui::EndPopup();
            }
        }

        ImGui::PopStyleVar(2);
    }

    void DrawAssetBrowserEmptyState()
    {
        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(14.0f, 12.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        const float emptyStateHeight = std::max(SZ::Px(120.0f), ImGui::GetContentRegionAvail().y);

        ImGui::BeginChild("##AssetBrowserEmptyState", ImVec2(0.0f, emptyStateHeight), ImGuiChildFlags_Borders);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted("No assets match the current filters");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped("Clear the search field or switch the asset type filter.");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    void DrawAssetSelectionEmptyState()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(14.0f, 12.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const float panelHeight = std::max(availableHeight, SZ::Px(160.0f));

        ImGui::BeginChild("##AssetDetailsEmptyState", ImVec2(0.0f, panelHeight),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding
        );
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted("No asset selected");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped(
                "Select an asset from the registry to inspect its path, runtime status, "
                "statistics, references, and preview."
            );
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    void DrawAssetDetailsInline(const Quiet::Project::ProjectContext& projectContext,
                                const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary,
                                const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary,
                                const Quiet::Scene::Scene& scene,
                                const std::string& assetID,
                                const float uiScale)
    {
        if (assetID.empty())
        {
            DrawAssetSelectionEmptyState();
            return;
        }

        const Quiet::Project::AssetRecord* record = Quiet::Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);

        if (!record)
        {
            DrawAssetSelectionEmptyState();
            return;
        }

        const std::string status = MakeStatusLabel(*record, meshLibrary, textureLibrary);
        const ImVec4 statusColor = MakeStatusColor(status);
        const std::string typeLabel = std::string(Quiet::Project::ToString(record->Type));

        const float labelWidth = SZ::Px(112.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(14.0f, 12.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        const float availableHeight = ImGui::GetContentRegionAvail().y;
        const float panelHeight = std::max(availableHeight, SZ::Px(260.0f));

        ImGui::BeginChild("##AssetPersistentDetails", ImVec2(0.0f, panelHeight),
            ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding
        );
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted(record->ID.c_str());
            ImGui::PopStyleColor();

            ImGui::SameLine();

            ImGui::TextColored(statusColor, "  %s", status.c_str());

            ImGui::Spacing();

            DrawDetailRow("Type", typeLabel.c_str(), labelWidth);
            DrawDetailRow("Built In", record->BuiltIn ? "true" : "false", labelWidth);

            if (record->Path.empty())
                DrawDetailRow("Path", "<built-in>", labelWidth);
            else
                DrawDetailRowWrapped("Path", record->Path, labelWidth);

            const std::uint32_t referenceCount = CountMeshReferences(scene, record->ID);
            DrawDetailRowUInt("References", referenceCount, labelWidth);

            if (record->Type == Quiet::Project::AssetType::Mesh)
            {
                const auto stats = meshLibrary.GetStats(record->ID);
                DrawDetailsSectionTitle("Mesh");

                if (!stats.has_value())
                {
                    ImGui::TextDisabled("This mesh asset is not currently loaded.");
                }
                else
                {
                    DrawDetailRowWrapped("Importer", stats->ImporterName, labelWidth);
                    DrawDetailRowUInt("Meshes", stats->ImportedMeshCount, labelWidth);
                    DrawDetailRowUInt("Submeshes", stats->SubmeshCount, labelWidth);
                    DrawDetailRowUInt("Materials", stats->MaterialCount, labelWidth);
                    DrawDetailRowUInt("Vertices", stats->VertexCount, labelWidth);
                    DrawDetailRowUInt("Indices", stats->IndexCount, labelWidth);

                    if (stats->BoundsValid)
                    {
                        const glm::vec3 size = stats->BoundsMax - stats->BoundsMin;
                        DrawDetailRowVec3("Bounds Size", size, labelWidth);
                    }

                    if (!stats->Warnings.empty())
                    {
                        DrawDetailRowUInt("Warnings", static_cast<std::uint32_t>(stats->Warnings.size()), labelWidth);
                    }
                }
            }
            else if (record->Type == Quiet::Project::AssetType::Texture)
            {
                const auto textureStats = textureLibrary.GetStats(record->ID);
                DrawDetailsSectionTitle("Texture");

                if (!textureStats.has_value())
                {
                    ImGui::TextDisabled("Texture metadata is not currently loaded.");
                }
                else
                {
                    DrawDetailRow("Format", textureStats->Format.c_str(), labelWidth);

                    if (textureStats->DimensionsValid)
                    {
                        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
                        ImGui::TextUnformatted("Dimensions");
                        ImGui::PopStyleColor();

                        ImGui::SameLine(labelWidth);

                        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
                        ImGui::Text("%u x %u", textureStats->Width, textureStats->Height);
                        ImGui::PopStyleColor();
                    }
                    else
                    {
                        DrawDetailRow("Dimensions", "Unknown", labelWidth);
                    }
                    DrawDetailRow("GPU Ready", textureStats->GpuReady ? "true" : "false", labelWidth);

                    if (textureStats->GpuReady)
                    {
                        ImGui::Spacing();
                        Quiet::Renderer::UI::DrawTexturePreview(
                            record->ID,
                            textureLibrary,
                            textureStats.value(),
                            uiScale,
                            150.0f
                        );
                    }
                }
            }
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    bool ShouldUseAssetDetailsSplitLayout()
    {
        return ImGui::GetContentRegionAvail().x >= SZ::Px(920.0f);
    }

    float CalculateAssetDetailsPanelWidth(const float availableWidth)
    {
        return std::clamp(
            availableWidth * 0.34f,
            SZ::Px(330.0f),
            SZ::Px(460.0f)
        );
    }

    std::string MakeAssetRemoveDisabledReason(const Quiet::Project::AssetRecord& record, const bool runtimeModeActive, const std::uint32_t referenceCount)
    {
        if (record.BuiltIn) return "Built-in assets cannot be removed.";
        if (runtimeModeActive) return "Assets cannot be removed while runtime mode is active.";
        if (referenceCount > 0) return "This asset is referenced by actors in the current scene.";
        return {};
    }

    std::string MakeAssetReimportDisabledReason(const Quiet::Project::AssetRecord& record, const bool runtimeModeActive, const bool resolvedPathExists)
    {
        if (record.BuiltIn) return "Built-in assets cannot be reimported.";
        if (runtimeModeActive) return "Assets cannot be reimported while runtime mode is active.";
        if (record.Path.empty()) return "This asset has no source path.";
        if (!resolvedPathExists) return "The source file does not exist on disk.";
        return {};
    }

    void DrawAssetContextMenu(const char* popupID,
                              const Quiet::Project::ProjectContext& projectContext,
                              const Quiet::Project::AssetRecord& record,
                              const bool runtimeModeActive,
                              const std::uint32_t referenceCount,
                              std::string& selectedDetailsAssetID,
                              std::string& pendingDetailsAssetID,
                              std::string& pendingReimportAssetID,
                              std::string& pendingRemoveAssetID)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(12.0f, 10.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, SZ::Px(ImVec2(8.0f, 6.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(8.0f, 5.0f)));

        if (!ImGui::BeginPopupContextItem(popupID))
        {
            ImGui::PopStyleVar(3);
            return;
        }

        selectedDetailsAssetID = record.ID;
        if (QUI::EditorMenuItem("Inspect"))
            pendingDetailsAssetID = record.ID;

        ImGui::Separator();
        if (QUI::EditorMenuItem("Copy ID"))
            ImGui::SetClipboardText(record.ID.c_str());

        const bool hasPath = !record.Path.empty();
        if (QUI::EditorMenuItem("Copy Path", nullptr, false, hasPath))
            ImGui::SetClipboardText(record.Path.c_str());

        const std::filesystem::path resolvedPath = hasPath ? ResolveDisplayPath(projectContext, record.Path) : std::filesystem::path{};
        const bool resolvedPathExists = hasPath && !resolvedPath.empty() && std::filesystem::exists(resolvedPath);

        if (QUI::EditorMenuItem("Copy Absolute Path", nullptr, false, hasPath))
        {
            const std::string resolvedPathText = resolvedPath.string();
            ImGui::SetClipboardText(resolvedPathText.c_str());
        }

        if (QUI::EditorMenuItem("Reveal in File Manager", nullptr, false, resolvedPathExists))
            Quiet::Platform::RevealPathInFileManager(resolvedPath);

        if (!resolvedPathExists && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("The source file does not exist on disk.");

        ImGui::Separator();

        const std::string reimportDisabledReason = MakeAssetReimportDisabledReason(record, runtimeModeActive, resolvedPathExists);
        const bool canReimport = reimportDisabledReason.empty();

        if (QUI::EditorMenuItem("Reimport", nullptr, false, canReimport))
            pendingReimportAssetID = record.ID;

        if (!canReimport && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", reimportDisabledReason.c_str());

        const std::string removeDisabledReason = MakeAssetRemoveDisabledReason(record, runtimeModeActive, referenceCount);
        const bool canRemove = removeDisabledReason.empty();

        if (QUI::EditorMenuItem("Remove", nullptr, false, canRemove))
            pendingRemoveAssetID = record.ID;

        if (!canRemove && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", removeDisabledReason.c_str());

        ImGui::EndPopup();
        ImGui::PopStyleVar(3);
    }

    void DrawRemoveAssetConfirmationPopup(const Quiet::Project::ProjectContext& projectContext,
                                          const std::string& assetID,
                                          bool& confirmed,
                                          bool& cancelled,
                                          const float uiScale)
    {
        confirmed = false;
        cancelled = false;

        if (assetID.empty()) return;

        const float modalWidth = 560.0f * uiScale;
        const float labelWidth = 92.0f * uiScale;

        ImGui::SetNextWindowSize(ImVec2(modalWidth, 0.0f), ImGuiCond_Appearing);

        bool removeAssetOpen = true;
        if (!QUI::EditorBeginPopupModal("Remove Asset", removeAssetOpen, ImGuiWindowFlags_AlwaysAutoResize))
            return;

        const Quiet::Project::AssetRecord* record = Quiet::Project::AssetRegistryIO::FindByID(projectContext.GetAssetRegistry(), assetID);
        if (!record)
        {
            ImGui::TextWrapped("The selected asset is no longer registered.");
            ImGui::Spacing();

            if (QUI::EditorButton("OK", SZ::Px(ImVec2(120.0f, 0.0f))))
            {
                cancelled = true;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
            return;
        }

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Warning);
        ImGui::TextUnformatted("Remove asset from registry?");
        ImGui::PopStyleColor();

        ImGui::Spacing();

        ImGui::TextWrapped(
            "This will unregister the asset from the current project. "
            "The source file on disk will not be deleted."
        );

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const std::string typeLabel = std::string(Quiet::Project::ToString(record->Type));

        DrawDetailRow("ID", record->ID.c_str(), labelWidth);
        DrawDetailRow("Type", typeLabel.c_str(), labelWidth);
        DrawDetailRow("Source", record->BuiltIn ? "Built-in" : "Project", labelWidth);

        if (record->Path.empty())
            DrawDetailRow("Path", "<none>", labelWidth);
        else
            DrawDetailRowWrapped("Path", record->Path, labelWidth);

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        if (QUI::EditorButton("Cancel", SZ::Px(ImVec2(130.0f, 0.0f))))
        {
            cancelled = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.360f, 0.070f, 0.080f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.520f, 0.100f, 0.120f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.620f, 0.120f, 0.140f, 1.0f));

        if (QUI::EditorButton("Remove Asset", SZ::Px(ImVec2(160.0f, 0.0f))))
        {
            confirmed = true;
            ImGui::CloseCurrentPopup();
        }

        ImGui::PopStyleColor(3);
        ImGui::EndPopup();
    }

    void DrawAssetPreviewCheckerboard(ImDrawList* drawList, const ImVec2& min, const ImVec2& max)
    {
        const float cellSize = SZ::Px(8.0f);

        const ImU32 colorA = ImGui::ColorConvertFloat4ToU32(ImVec4(0.105f, 0.115f, 0.135f, 1.0f));
        const ImU32 colorB = ImGui::ColorConvertFloat4ToU32(ImVec4(0.075f, 0.085f, 0.105f, 1.0f));

        int row = 0;

        for (float y = min.y; y < max.y; y += cellSize, ++row)
        {
            int column = 0;

            for (float x = min.x; x < max.x; x += cellSize, ++column)
            {
                const ImVec2 cellMin(x, y);
                const ImVec2 cellMax(
                    std::min(x + cellSize, max.x),
                    std::min(y + cellSize, max.y)
                );

                drawList->AddRectFilled(
                    cellMin,
                    cellMax,
                    ((row + column) % 2 == 0) ? colorA : colorB
                );
            }
        }
    }

    void DrawAssetMeshGlyph(ImDrawList* drawList, const ImVec2& min, const ImVec2& max)
    {
        const ImVec2 center(
            min.x + (max.x - min.x) * 0.5f,
            min.y + (max.y - min.y) * 0.48f
        );

        const float radius = std::min(max.x - min.x, max.y - min.y) * 0.26f;

        const ImU32 accent = ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::AccentSoft);
        const ImU32 muted = ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::TextMuted);

        const ImVec2 top(center.x, center.y - radius);
        const ImVec2 left(center.x - radius * 0.92f, center.y + radius * 0.54f);
        const ImVec2 right(center.x + radius * 0.92f, center.y + radius * 0.54f);
        const ImVec2 inner(center.x, center.y + radius * 0.06f);

        drawList->AddTriangle(top, right, left, accent, SZ::Px(2.0f));
        drawList->AddLine(top, inner, muted, SZ::Px(1.5f));
        drawList->AddLine(left, inner, muted, SZ::Px(1.5f));
        drawList->AddLine(right, inner, muted, SZ::Px(1.5f));

        const char* label = "MESH";
        const ImVec2 textSize = ImGui::CalcTextSize(label);

        drawList->AddText(
            ImVec2(
                min.x + (max.x - min.x - textSize.x) * 0.5f,
                max.y - textSize.y - SZ::Px(10.0f)
            ),
            muted,
            label
        );
    }

    void DrawAssetBrowserCardPreview(const Quiet::Project::AssetRecord& record,
                                     const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary,
                                     const ImVec2& min,
                                     const ImVec2& max,
                                     const float uiScale)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        drawList->AddRectFilled(
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Background3),
            SZ::Px(8.0f)
        );
        drawList->AddRect(
            min,
            max,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::BorderSoft),
            SZ::Px(8.0f)
        );

        if (record.Type == Quiet::Project::AssetType::Texture)
        {
            const auto textureStats = textureLibrary.GetStats(record.ID);
            if (textureStats.has_value() && textureStats->GpuReady)
            {
                DrawAssetPreviewCheckerboard(drawList, min, max);
                const ImTextureID previewID = Quiet::Renderer::UI::GetOrCreateTexturePreviewID(record.ID, textureLibrary);

                if (previewID)
                {
                    const float availableWidth = max.x - min.x - SZ::Px(16.0f);
                    const float availableHeight = max.y - min.y - SZ::Px(16.0f);
                    const float maxSide =
                        std::max(SZ::Px(32.0f), std::min(availableWidth, availableHeight));

                    const ImVec2 previewSize =
                        Quiet::Renderer::UI::CalculateTexturePreviewSize(
                            textureStats->Width,
                            textureStats->Height,
                            maxSide
                        );

                    const ImVec2 imageMin(
                        min.x + (max.x - min.x - previewSize.x) * 0.5f,
                        min.y + (max.y - min.y - previewSize.y) * 0.5f
                    );
                    const ImVec2 imageMax(
                        imageMin.x + previewSize.x,
                        imageMin.y + previewSize.y
                    );
                    drawList->AddImage(
                        previewID,
                        imageMin,
                        imageMax,
                        ImVec2(0.0f, 1.0f),
                        ImVec2(1.0f, 0.0f),
                        IM_COL32_WHITE
                    );

                    drawList->AddRect(
                        imageMin,
                        imageMax,
                        ImGui::ColorConvertFloat4ToU32(ImVec4(1.0f, 1.0f, 1.0f, 0.10f)),
                        SZ::Px(3.0f)
                    );

                    return;
                }
            }
        }

        if (record.Type == Quiet::Project::AssetType::Mesh)
        {
            DrawAssetMeshGlyph(drawList, min, max);
        }
        else
        {
            const char* fallbackLabel = record.Type == Quiet::Project::AssetType::Texture ? "TEXTURE" : "ASSET";
            const ImVec2 textSize = ImGui::CalcTextSize(fallbackLabel);

            drawList->AddText(
                ImVec2(
                    min.x + (max.x - min.x - textSize.x) * 0.5f,
                    min.y + (max.y - min.y - textSize.y) * 0.5f
                ),
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::TextMuted),
                fallbackLabel
            );
        }

        (void)uiScale;
    }
    void DrawAssetBrowserGrid(const std::vector<const Quiet::Project::AssetRecord*>& visibleAssets,
                              const Quiet::Project::ProjectContext& projectContext,
                              Quiet::Editor::EditorState& editorState,
                              const Quiet::Scene::Scene& scene,
                              const Quiet::Renderer::Vulkan::TextureLibrary& textureLibrary,
                              std::string& selectedDetailsAssetID,
                              std::string& pendingDetailsAssetID,
                              std::string& pendingReimportAssetID,
                              std::string& pendingRemoveAssetID,
                              const AssetBrowserThumbnailSize thumbnailSize,
                              const float uiScale)
    {
        const bool runtimeModeActive = editorState.Scene.Mode != Quiet::Editor::SceneMode::Edit;

        const AssetBrowserCardMetrics metrics = MakeAssetBrowserCardMetrics(thumbnailSize);
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const int columnCount = std::max(1, static_cast<int>(availableWidth / std::max(SZ::Px(1.0f), metrics.DesiredColumnWidth)));

        if (!ImGui::BeginTable("AssetBrowserGrid", columnCount, ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX)) return;

        for (int columnIndex = 0; columnIndex < columnCount; ++columnIndex)
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch);
        }

        for (const Quiet::Project::AssetRecord* recordPtr : visibleAssets)
        {
            if (!recordPtr) continue;

            const Quiet::Project::AssetRecord& record = *recordPtr;

            ImGui::TableNextColumn();

            const std::uint32_t referenceCount = CountMeshReferences(scene, record.ID);
            const std::string displayName = MakeAssetDisplayName(record);
            const bool isSelectedAsset = selectedDetailsAssetID == record.ID;

            const float cardWidth = std::max(metrics.MinimumCardWidth, ImGui::GetContentRegionAvail().x);

            const float cardHeight = metrics.CardHeight;
            const float padding = metrics.Padding;
            const float previewHeight = metrics.PreviewHeight;

            const ImVec2 cardMin = ImGui::GetCursorScreenPos();
            const ImVec2 cardSize(cardWidth, cardHeight);
            const ImVec2 cardMax(cardMin.x + cardSize.x, cardMin.y + cardSize.y);

            const std::string hitID = "##AssetBrowserCard_" + record.ID;

            ImGui::InvisibleButton(hitID.c_str(), cardSize);
            const bool hovered = ImGui::IsItemHovered();

            if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

            if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            {
                selectedDetailsAssetID = record.ID;
            }

            if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                pendingDetailsAssetID = record.ID;
            }

            DrawAssetContextMenu(
                hitID.c_str(),
                projectContext,
                record,
                runtimeModeActive,
                referenceCount,
                selectedDetailsAssetID,
                pendingDetailsAssetID,
                pendingReimportAssetID,
                pendingRemoveAssetID
            );

            ImDrawList* drawList = ImGui::GetWindowDrawList();

            const ImVec4 cardColor =
                isSelectedAsset
                    ? QUI::EditorColor::AccentDim
                    : hovered
                        ? QUI::EditorColor::Background3
                        : QUI::EditorColor::Background2;

            drawList->AddRectFilled(
                cardMin,
                cardMax,
                ImGui::ColorConvertFloat4ToU32(cardColor),
                SZ::Px(12.0f)
            );

            drawList->AddRect(
                cardMin,
                cardMax,
                ImGui::ColorConvertFloat4ToU32(
                    isSelectedAsset ? QUI::EditorColor::AccentSoft : QUI::EditorColor::BorderSoft
                ),
                SZ::Px(12.0f)
            );

            const ImVec2 previewMin(cardMin.x + padding, cardMin.y + padding);
            const ImVec2 previewMax(cardMax.x - padding, previewMin.y + previewHeight);

            DrawAssetBrowserCardPreview(
                record,
                textureLibrary,
                previewMin,
                previewMax,
                uiScale
            );

            const ImVec2 afterCardCursor = ImGui::GetCursorScreenPos();
            const ImVec2 nameMin(cardMin.x + padding, previewMax.y + SZ::Px(8.0f));
            const ImVec2 nameMax(cardMax.x - padding, nameMin.y + ImGui::GetTextLineHeight());

            DrawAssetCardTextLine(
                displayName,
                nameMin,
                nameMax,
                isSelectedAsset ? QUI::EditorColor::Accent : QUI::EditorColor::Text
            );

            const ImVec2 typePillPos(cardMin.x + padding, nameMax.y + SZ::Px(8.0f));

            ImGui::SetCursorScreenPos(typePillPos);
            DrawAssetTypePill(record.Type);

            if (hovered)
            {
                DrawAssetPaddedTooltip(!record.Path.empty() ? record.Path : record.ID);
            }

            ImGui::SetCursorScreenPos(afterCardCursor);
        }

        ImGui::EndTable();
    }
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildAssetsPanel(Project::ProjectContext& projectContext,
                                      Editor::EditorState& editorState,
                                      Scene::Scene& scene,
                                      const Vulkan::MeshLibrary& meshLibrary,
                                      const Vulkan::TextureLibrary& textureLibrary)
    {
        ImGui::Begin("Assets");
        {
            static std::array<char, 128> assetIDBuffer{};
            static std::array<char, 260> assetPathBuffer{};
            static std::string registerErrorMessage;
            static std::string removeErrorMessage;
            static std::string selectedDetailsAssetID;
            static std::string confirmRemoveAssetID;
            static std::array<char, 128> assetSearchBuffer{};
            static AssetBrowserTypeFilter assetTypeFilter = AssetBrowserTypeFilter::All;
            static AssetBrowserViewMode assetViewMode = AssetBrowserViewMode::Browser;
            static AssetBrowserThumbnailSize assetThumbnailSize = AssetBrowserThumbnailSize::Small;

            const auto& registry = projectContext.GetAssetRegistry();
            bool openImportPopup = false;

            DrawAssetBrowserToolbar(
                editorState,
                registry,
                assetSearchBuffer,
                assetTypeFilter,
                assetViewMode,
                assetThumbnailSize,
                openImportPopup
            );

            if (openImportPopup)
            {
                ImGui::OpenPopup("Import Asset");
            }

            DrawImportAssetPopup(
                projectContext,
                editorState,
                assetIDBuffer,
                assetPathBuffer,
                registerErrorMessage
            );

            if (const auto selectedAsset = DrawAssetFileDialog(projectContext))
            {
                CopyToBuffer(assetPathBuffer, selectedAsset->AssetPath);

                if (assetIDBuffer[0] == '\0')
                {
                    CopyToBuffer(assetIDBuffer, selectedAsset->AssetID);
                }
            }

            DrawFailurePopup("Import Asset Failed", registerErrorMessage);

            ImGui::Spacing();
            const std::vector<const Project::AssetRecord*> visibleAssets =
                BuildVisibleAssetList(registry, assetTypeFilter, assetSearchBuffer.data());

            bool openAssetDetailsPopup = false;

            const bool useSplitDetailsLayout = ShouldUseAssetDetailsSplitLayout();
            const ImGuiWindowFlags browserBodyFlags = useSplitDetailsLayout
                    ? ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
                    : ImGuiWindowFlags_None;

            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(0.0f, 0.0f)));

            ImGui::BeginChild("##AssetBrowserBodyRegion", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, browserBodyFlags);
            {
                std::string pendingRemoveAssetID;
                std::string pendingDetailsAssetID;
                std::string pendingReimportAssetID;

                const float assetBrowserWidth = ImGui::GetContentRegionAvail().x;
                const float detailsPanelWidth = useSplitDetailsLayout
                    ? CalculateAssetDetailsPanelWidth(assetBrowserWidth)
                    : 0.0f;
                const float tablePanelWidth = useSplitDetailsLayout
                    ? std::max(
                        SZ::Px(520.0f),
                        assetBrowserWidth - detailsPanelWidth - ImGui::GetStyle().ItemSpacing.x
                    )
                    : 0.0f;

                if (useSplitDetailsLayout)
                {
                    ImGui::BeginChild("##AssetRegistryTableRegion", ImVec2(tablePanelWidth, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_None);
                }

            if (visibleAssets.empty())
            {
                DrawAssetBrowserEmptyState();
            }
            else if (assetViewMode == AssetBrowserViewMode::Browser)
            {
                DrawAssetBrowserGrid(
                    visibleAssets,
                    projectContext,
                    editorState,
                    scene,
                    textureLibrary,
                    selectedDetailsAssetID,
                    pendingDetailsAssetID,
                    pendingReimportAssetID,
                    pendingRemoveAssetID,
                    assetThumbnailSize,
                    GetUIScale()
                );
            }
            else
            {
                ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, Scale(ImVec2(10.0f, 8.0f)));

                ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, QUI::EditorColor::Background2);
                ImGui::PushStyleColor(ImGuiCol_TableBorderStrong, QUI::EditorColor::Border);
                ImGui::PushStyleColor(ImGuiCol_TableBorderLight, QUI::EditorColor::BorderSoft);
                ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, ImVec4(0.030f, 0.036f, 0.048f, 0.45f));

                if (ImGui::BeginTable("AssetsTable", 4,
                    ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_BordersInnerV |
                    ImGuiTableFlags_BordersOuter |
                    ImGuiTableFlags_Resizable |
                    ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Asset", ImGuiTableColumnFlags_WidthStretch, 0.55f);
                    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, Scale(92.0f));
                    ImGui::TableSetupColumn("State", ImGuiTableColumnFlags_WidthFixed, Scale(190.0f));
                    ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, Scale(96.0f));
                    ImGui::TableHeadersRow();

                    const bool runtimeModeActive = editorState.Scene.Mode != Editor::SceneMode::Edit;

                    for (const Project::AssetRecord* recordPtr : visibleAssets)
                    {
                        if (!recordPtr) continue;

                        const Project::AssetRecord& record = *recordPtr;
                        const std::uint32_t referenceCount = CountMeshReferences(scene, record.ID);
                        const std::string status = MakeStatusLabel(record, meshLibrary, textureLibrary);

                        const std::string browserState =
                            MakeAssetBrowserStateLabel(
                                record,
                                status,
                                runtimeModeActive,
                                referenceCount
                            );

                        const std::string displayName = MakeAssetDisplayName(record);
                        const bool isSelectedAsset = selectedDetailsAssetID == record.ID;

                        const float rowHeight = SZ::Px(42.0f);
                        const float contentOffsetY = SZ::Px(9.0f);

                        ImGui::TableNextRow(ImGuiTableRowFlags_None, rowHeight);

                        if (isSelectedAsset)
                        {
                            ImGui::TableSetBgColor(
                                ImGuiTableBgTarget_RowBg0,
                                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::AccentDim)
                            );

                            ImGui::TableSetBgColor(
                                ImGuiTableBgTarget_RowBg1,
                                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::AccentDim)
                            );
                        }

                        bool rowHovered = false;

                        // Asset
                        ImGui::TableSetColumnIndex(0);
                        {
                            const ImVec2 cellStart = ImGui::GetCursorScreenPos();
                            const std::string hitID = "##AssetRowHit_Asset_" + record.ID;

                            if (DrawAssetTableCellHitTarget(hitID.c_str(), rowHeight))
                                selectedDetailsAssetID = record.ID;

                            const bool cellHovered = ImGui::IsItemHovered();
                            rowHovered |= cellHovered;

                            DrawAssetContextMenu(
                                hitID.c_str(),
                                projectContext,
                                record,
                                runtimeModeActive,
                                referenceCount,
                                selectedDetailsAssetID,
                                pendingDetailsAssetID,
                                pendingReimportAssetID,
                                pendingRemoveAssetID
                            );

                            ImGui::SetCursorScreenPos(ImVec2(cellStart.x, cellStart.y + contentOffsetY));
                            ImGui::PushStyleColor(ImGuiCol_Text, isSelectedAsset ? QUI::EditorColor::Accent : QUI::EditorColor::Text);

                            ImGui::TextUnformatted(displayName.c_str());
                            ImGui::PopStyleColor();

                            if (cellHovered && !record.Path.empty())
                                DrawAssetPaddedTooltip(record.Path);
                            else if (cellHovered)
                                DrawAssetPaddedTooltip(record.ID);
                        }

                        // Type
                        ImGui::TableSetColumnIndex(1);
                        {
                            const ImVec2 cellStart = ImGui::GetCursorScreenPos();
                            const std::string hitID = "##AssetRowHit_Type_" + record.ID;

                            if (DrawAssetTableCellHitTarget(hitID.c_str(), rowHeight))
                                selectedDetailsAssetID = record.ID;

                            rowHovered |= ImGui::IsItemHovered();

                            DrawAssetContextMenu(
                                hitID.c_str(),
                                projectContext,
                                record,
                                runtimeModeActive,
                                referenceCount,
                                selectedDetailsAssetID,
                                pendingDetailsAssetID,
                                pendingReimportAssetID,
                                pendingRemoveAssetID
                            );

                            ImGui::SetCursorScreenPos(ImVec2(cellStart.x, cellStart.y + SZ::Px(7.0f)));
                            DrawAssetTypePill(record.Type);
                        }

                        // State
                        ImGui::TableSetColumnIndex(2);
                        {
                            const ImVec2 cellStart = ImGui::GetCursorScreenPos();
                            const std::string hitID = "##AssetRowHit_State_" + record.ID;

                            if (DrawAssetTableCellHitTarget(hitID.c_str(), rowHeight))
                                selectedDetailsAssetID = record.ID;

                            rowHovered |= ImGui::IsItemHovered();

                            DrawAssetContextMenu(
                                hitID.c_str(),
                                projectContext,
                                record,
                                runtimeModeActive,
                                referenceCount,
                                selectedDetailsAssetID,
                                pendingDetailsAssetID,
                                pendingReimportAssetID,
                                pendingRemoveAssetID
                            );

                            ImGui::SetCursorScreenPos(ImVec2(cellStart.x, cellStart.y + SZ::Px(7.0f)));
                            DrawAssetStatePill(browserState);

                            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                            {
                                if (referenceCount > 0)
                                    ImGui::SetTooltip("Referenced by %u actor(s) in the current scene.", referenceCount);

                                else if (record.BuiltIn)
                                    ImGui::SetTooltip("Built-in engine asset.");

                                else if (runtimeModeActive)
                                    ImGui::SetTooltip("Asset operations are limited while runtime mode is active.");

                                else
                                    ImGui::SetTooltip("Project asset is currently removable.");
                            }
                        }

                        if (rowHovered && !isSelectedAsset)
                        {
                            ImGui::TableSetBgColor(
                                ImGuiTableBgTarget_RowBg0,
                                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Background3)
                            );
                        }

                        // Actions
                        ImGui::TableSetColumnIndex(3);
                        {
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + SZ::Px(4.0f));
                            const std::string inspectLabel = "Inspect##" + record.ID;

                            if (QUI::EditorButton(inspectLabel.c_str(), Scale(ImVec2(82.0f, 0.0f))))
                                pendingDetailsAssetID = record.ID;
                        }
                    }

                    ImGui::EndTable();
                }

                ImGui::PopStyleColor(4);
                ImGui::PopStyleVar();
            }

            if (useSplitDetailsLayout)
            {
                ImGui::EndChild();
            }

            if (!pendingDetailsAssetID.empty())
            {
                selectedDetailsAssetID = pendingDetailsAssetID;
                openAssetDetailsPopup = true;
            }

            if (!pendingReimportAssetID.empty())
            {
                selectedDetailsAssetID = pendingReimportAssetID;
                editorState.QueueRequest(Quiet::Editor::EditorRequestType::ReimportAsset, pendingReimportAssetID);
            }

            if (!pendingRemoveAssetID.empty())
            {
                selectedDetailsAssetID = pendingRemoveAssetID;
                confirmRemoveAssetID = pendingRemoveAssetID;
                ImGui::OpenPopup("Remove Asset");
            }

            bool removeAssetConfirmed = false;
            bool removeAssetCancelled = false;

            DrawRemoveAssetConfirmationPopup(
                projectContext,
                confirmRemoveAssetID,
                removeAssetConfirmed,
                removeAssetCancelled,
                GetUIScale()
            );

            if (removeAssetConfirmed && !confirmRemoveAssetID.empty())
            {
                const std::string removedAssetID = confirmRemoveAssetID;
                const bool removed = projectContext.RemoveAssetByID(removedAssetID);

                confirmRemoveAssetID.clear();

                if (!removed)
                {
                    removeErrorMessage = "Failed to remove the selected asset from the registry.";

                    editorState.AssetOperationFeedback.Set(
                        Editor::EditorFeedbackSeverity::Error,
                        "Remove failed",
                        removeErrorMessage
                    );

                    ImGui::OpenPopup("Remove Asset Failed");
                }
                else
                {
                    if (selectedDetailsAssetID == removedAssetID)
                    {
                        selectedDetailsAssetID.clear();
                    }

                    editorState.AssetOperationFeedback.Set(
                        Editor::EditorFeedbackSeverity::Success,
                        "Asset removed",
                        "Asset '" + removedAssetID + "' was removed from the registry. The source file was not deleted."
                    );
                }
            }
            else if (removeAssetCancelled)
            {
                confirmRemoveAssetID.clear();
            }
            if (useSplitDetailsLayout)
            {
                ImGui::SameLine();
                ImGui::BeginChild("##AssetDetailsSideRegion", ImVec2(detailsPanelWidth, 0.0f),
                    ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            }
            else
            {
                ImGui::Spacing();
            }

            const bool selectedAssetStillExists = selectedDetailsAssetID.empty() ||
                Project::AssetRegistryIO::FindByID(registry, selectedDetailsAssetID) != nullptr;

            if (!selectedAssetStillExists)
            {
                selectedDetailsAssetID.clear();
            }

            DrawAssetOperationFeedback(editorState);
            DrawAssetDetailsInline(
                projectContext,
                meshLibrary,
                textureLibrary,
                scene,
                selectedDetailsAssetID,
                GetUIScale()
            );

                if (useSplitDetailsLayout)
                {
                    ImGui::EndChild();
                }
            }
            ImGui::EndChild();
            ImGui::PopStyleVar();

            if (openAssetDetailsPopup)
            {
                ImGui::OpenPopup("Asset Details");
            }

            DrawAssetDetailsPopup(projectContext, meshLibrary, textureLibrary, selectedDetailsAssetID, GetUIScale());
            DrawFailurePopup("Remove Asset Failed", removeErrorMessage);
        }
        ImGui::End();
    }
}