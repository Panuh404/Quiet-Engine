#include "Renderer/UI/ImGuiLayer.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "Quiet/Editor/EditorActionID.h"
#include "Quiet/Editor/EditorActions.h"
#include "Quiet/Editor/EditorCommands.h"
#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/AssetRecord.h"
#include "Quiet/Project/AssetType.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/UI/ImGuiToolbarCommon.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Mesh/MeshPrimitives.h"
#include "Scene/Scene.h"

namespace QUI = Quiet::Renderer::UI;
namespace SZ = Quiet::Renderer::UI::EditorSizing;

namespace
{
    constexpr const char* kActorDragDropPayload = "QUIET_ACTOR_ROW";
    constexpr const char* kRenameFolderPopup = "Rename Folder";

    std::optional<Quiet::Core::UUID> g_RenameFolderID;
    std::array<char, 128> g_RenameFolderNameBuffer{};
    bool g_RequestOpenRenameFolderPopup = false;

    std::optional<Quiet::Core::UUID> g_DraggedActorID;
    std::optional<std::tuple<Quiet::Scene::SceneHierarchyNodeType, Quiet::Core::UUID>> g_DraggedRootNode;

    std::size_t CalculateDropDestinationIndex(const std::size_t sourceIndex, const std::size_t targetIndex, const bool dropAfterTarget)
    {
        const std::size_t insertionSlot = targetIndex + (dropAfterTarget ? 1u : 0u);
        if (sourceIndex < insertionSlot) return insertionSlot - 1u;
        return insertionSlot;
    }

    std::optional<std::size_t> FindRootNodeIndex(const Quiet::Scene::SceneHierarchyState& hierarchy,
                                                 const Quiet::Scene::SceneHierarchyNodeType nodeType,
                                                 const Quiet::Core::UUID nodeID)
    {
        for (std::size_t index = 0; index < hierarchy.RootNodes.size(); ++index)
        {
            const Quiet::Scene::SceneHierarchyNode& node = hierarchy.RootNodes[index];
            if (node.Type == nodeType && node.ID == nodeID) return index;
        }

        return std::nullopt;
    }

    std::optional<std::tuple<Quiet::Scene::SceneHierarchyNodeType, Quiet::Core::UUID, std::size_t>>BuildRootNodeDropMove(
        const Quiet::Scene::SceneHierarchyState& hierarchy,
        const std::size_t targetRootIndex,
        const bool dropAfterTarget,
        const Quiet::Scene::SceneHierarchyNodeType targetType,
        const Quiet::Core::UUID targetID)
    {
        if (!g_DraggedRootNode.has_value()) return std::nullopt;

        const auto [sourceType, sourceID] = g_DraggedRootNode.value();
        if (sourceType == targetType && sourceID == targetID) return std::nullopt;

        const auto sourceRootIndex = FindRootNodeIndex(hierarchy, sourceType, sourceID);
        if (!sourceRootIndex.has_value()) return std::nullopt;

        const std::size_t destinationIndex =
            CalculateDropDestinationIndex(
                sourceRootIndex.value(),
                targetRootIndex,
                dropAfterTarget
            );

        if (destinationIndex == sourceRootIndex.value()) return std::nullopt;

        return std::make_tuple(sourceType, sourceID, destinationIndex);
    }

    void ResetHierarchyDragState()
    {
        g_DraggedActorID.reset();
        g_DraggedRootNode.reset();
    }

    void DrawActorDropMarker(const ImVec2& rowMin, const ImVec2& rowMax, const bool after)
    {
        const float y = after ? rowMax.y : rowMin.y;
        const float thickness = SZ::Px(2.0f);
        const float radius = SZ::Px(3.0f);

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const ImVec2 lineStart(rowMin.x + SZ::Px(6.0f), y);
        const ImVec2 lineEnd(rowMax.x - SZ::Px(6.0f), y);

        const ImU32 color = ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::AccentHover);

        drawList->AddLine(lineStart, lineEnd, color, thickness);
        drawList->AddCircleFilled(lineStart, radius, color);
    }

    void DrawFolderDropHighlight(const ImVec2& rowMin, const ImVec2& rowMax)
    {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 color = ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::AccentHover);

        drawList->AddRect(
            rowMin,
            rowMax,
            color,
            SZ::Px(7.0f),
            0,
            SZ::Px(2.0f)
        );
    }

    void BeginActorDragSource(const Quiet::Scene::Actor& actor)
    {
        if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) return;

        g_DraggedActorID = actor.GetID();
        g_DraggedRootNode.reset();

        const int payloadValue = 1;
        ImGui::SetDragDropPayload(
            kActorDragDropPayload,
            &payloadValue,
            sizeof(payloadValue)
        );

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        const std::string& actorName = actor.GetName();
        ImGui::TextUnformatted(actorName.empty() ? "Unnamed Actor" : actorName.c_str());
        ImGui::PopStyleColor();

        ImGui::EndDragDropSource();
    }

    void BeginRootActorDragSource(const Quiet::Scene::Actor& actor)
    {
        if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) return;

        g_DraggedActorID = actor.GetID();
        g_DraggedRootNode = std::make_tuple(Quiet::Scene::SceneHierarchyNodeType::Actor, actor.GetID());

        const int payloadValue = 1;
        ImGui::SetDragDropPayload(
            kActorDragDropPayload,
            &payloadValue,
            sizeof(payloadValue)
        );

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        const std::string& actorName = actor.GetName();
        ImGui::TextUnformatted(actorName.empty() ? "Unnamed Actor" : actorName.c_str());
        ImGui::PopStyleColor();

        ImGui::EndDragDropSource();
    }

    void BeginFolderDragSource(const Quiet::Scene::SceneHierarchyFolder& folder)
    {
        if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) return;

        g_DraggedActorID.reset();
        g_DraggedRootNode = std::make_tuple(
            Quiet::Scene::SceneHierarchyNodeType::Folder,
            folder.ID
        );

        const int payloadValue = 1;
        ImGui::SetDragDropPayload(
            kActorDragDropPayload,
            &payloadValue,
            sizeof(payloadValue)
        );

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::TextUnformatted(folder.Name.empty() ? "Folder" : folder.Name.c_str());
        ImGui::PopStyleColor();

        ImGui::EndDragDropSource();
    }

    bool DrawHierarchyHeader(const bool canCreate)
    {
        bool createPressed = false;

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float createButtonWidth = SZ::Px(76.0f);
        const bool hasInlineButtonSpace = availableWidth >= SZ::Px(280.0f);

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(8.0f, 6.0f)));

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted("Scene Hierarchy");
        ImGui::PopStyleColor();

        if (hasInlineButtonSpace)
        {
            ImGui::SameLine();

            const float buttonX =
                ImGui::GetCursorPosX() +
                ImGui::GetContentRegionAvail().x -
                createButtonWidth;

            ImGui::SetCursorPosX(buttonX);

            createPressed = QUI::EditorButton(
                "+ Create",
                ImVec2(createButtonWidth, 0.0f),
                canCreate
            );
        }

        ImGui::PopStyleVar();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::TextWrapped("Organize scene actors with folders. Drag rows to reorder or group them.");
        ImGui::PopStyleColor();

        if (!hasInlineButtonSpace)
        {
            ImGui::Spacing();

            createPressed = QUI::EditorButton(
                "+ Create",
                ImVec2(-1.0f, 0.0f),
                canCreate
            );
        }

        ImGui::Spacing();

        ImGui::PushStyleColor(ImGuiCol_Separator, QUI::EditorColor::BorderSoft);
        ImGui::Separator();
        ImGui::PopStyleColor();

        return createPressed;
    }

    void DrawHierarchyEmptyState()
    {
        ImGui::Spacing();

        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(12.0f, 10.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        ImGui::BeginChild(
            "##HierarchyEmptyState",
            ImVec2(0.0f, SZ::Px(92.0f)),
            ImGuiChildFlags_Borders
        );
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted("No hierarchy items");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped("Use + Create to add an actor, light, mesh actor, or folder.");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    const char* GetActorTypeLabel(const Quiet::Scene::Actor& actor)
    {
        if (actor.HasComponent<Quiet::Scene::CameraComponent>())
            return "Camera";

        if (actor.HasComponent<Quiet::Scene::DirectionalLightComponent>())
            return "Light";

        if (actor.HasComponent<Quiet::Scene::StaticMeshComponent>())
            return "Mesh";

        if (actor.HasComponent<Quiet::Scene::RotatorComponent>())
            return "Rotator";

        return "Actor";
    }

    const char* GetActorTypeIcon(const Quiet::Scene::Actor& actor)
    {
        if (actor.HasComponent<Quiet::Scene::CameraComponent>())
            return QUI::Icons::Focus;

        if (actor.HasComponent<Quiet::Scene::DirectionalLightComponent>())
            return QUI::Icons::World;

        if (actor.HasComponent<Quiet::Scene::StaticMeshComponent>())
            return QUI::Icons::Local;

        if (actor.HasComponent<Quiet::Scene::RotatorComponent>())
            return QUI::Icons::Rotate;

        return QUI::Icons::Local;
    }

    bool DrawActorRow(const Quiet::Scene::Actor& actor, const bool selected, const bool enabled)
    {
        const std::string actorID = Quiet::Core::UUIDToString(actor.GetID());
        const std::string& actorName = actor.GetName();
        const std::string displayName = actorName.empty() ? "Unnamed Actor" : actorName;

        const float rowHeight = SZ::Px(38.0f);
        const float rowRounding = SZ::Px(7.0f);
        const float iconAreaWidth = SZ::Px(34.0f);
        const float leftPadding = SZ::Px(9.0f);
        const float rightPadding = SZ::Px(8.0f);

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const ImVec2 rowMax = ImVec2(rowMin.x + availableWidth, rowMin.y + rowHeight);

        ImGui::PushID(actorID.c_str());
        if (!enabled) ImGui::BeginDisabled();

        ImGui::InvisibleButton("##ActorRow", ImVec2(availableWidth, rowHeight));

        const bool hovered = enabled && ImGui::IsItemHovered();
        const bool clicked = enabled && ImGui::IsItemClicked(ImGuiMouseButton_Left);

        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (!enabled) ImGui::EndDisabled();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec4 backgroundColor = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
        ImVec4 borderColor = QUI::EditorColor::BorderSoft;

        if (selected)
        {
            backgroundColor = QUI::EditorColor::AccentDim;
            borderColor = QUI::EditorColor::AccentSoft;
        }
        else if (hovered)
        {
            backgroundColor = QUI::EditorColor::Background3;
            borderColor = QUI::EditorColor::Border;
        }
        else
        {
            backgroundColor = ImVec4(
                QUI::EditorColor::Background2.x,
                QUI::EditorColor::Background2.y,
                QUI::EditorColor::Background2.z,
                0.42f
            );
        }

        drawList->AddRectFilled(
            rowMin,
            rowMax,
            ImGui::ColorConvertFloat4ToU32(backgroundColor),
            rowRounding
        );
        drawList->AddRect(
            rowMin,
            rowMax,
            ImGui::ColorConvertFloat4ToU32(borderColor),
            rowRounding
        );

        if (selected)
        {
            const ImVec2 accentMin = ImVec2(rowMin.x, rowMin.y + SZ::Px(5.0f));
            const ImVec2 accentMax = ImVec2(rowMin.x + SZ::Px(3.0f), rowMax.y - SZ::Px(5.0f));

            drawList->AddRectFilled(
                accentMin,
                accentMax,
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::AccentHover),
                SZ::Px(3.0f)
            );
        }

        const char* icon = GetActorTypeIcon(actor);
        const char* typeLabel = GetActorTypeLabel(actor);

        const ImVec2 iconSize = ImGui::CalcTextSize(icon);
        const ImVec2 nameSize = ImGui::CalcTextSize(displayName.c_str());
        const ImVec2 typeSize = ImGui::CalcTextSize(typeLabel);

        const float centerY = rowMin.y + (rowHeight * 0.5f);

        const ImVec2 iconPos = ImVec2(
            rowMin.x + leftPadding,
            centerY - iconSize.y * 0.5f
        );

        const ImVec2 namePos = ImVec2(
            rowMin.x + iconAreaWidth,
            centerY - nameSize.y * 0.5f
        );

        drawList->PushClipRect(rowMin, rowMax, true);
        drawList->AddText(
            iconPos,
            ImGui::ColorConvertFloat4ToU32(
                selected ? QUI::EditorColor::AccentHover : QUI::EditorColor::TextMuted
            ),
            icon
        );
        drawList->AddText(
            namePos,
            ImGui::ColorConvertFloat4ToU32(
                enabled ? QUI::EditorColor::Text : QUI::EditorColor::TextDim
            ),
            displayName.c_str()
        );

        if (availableWidth >= SZ::Px(230.0f))
        {
            const float badgeHeight = SZ::Px(20.0f);
            const float badgePaddingX = SZ::Px(8.0f);
            const float badgeWidth = typeSize.x + badgePaddingX * 2.0f;

            const ImVec2 badgeMin = ImVec2(
                rowMax.x - badgeWidth - rightPadding,
                centerY - badgeHeight * 0.5f
            );

            const ImVec2 badgeMax = ImVec2(
                badgeMin.x + badgeWidth,
                badgeMin.y + badgeHeight
            );

            drawList->AddRectFilled(
                badgeMin,
                badgeMax,
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Background3),
                SZ::Px(6.0f)
            );

            drawList->AddRect(
                badgeMin,
                badgeMax,
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::BorderSoft),
                SZ::Px(6.0f)
            );

            drawList->AddText(
                ImVec2(
                    badgeMin.x + badgePaddingX,
                    centerY - typeSize.y * 0.5f
                ),
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::TextMuted),
                typeLabel
            );
        }
        drawList->PopClipRect();

        ImGui::PopID();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", actorID.c_str());


        return clicked;
    }

    bool DrawFolderRow(const Quiet::Scene::SceneHierarchyFolder& folder, const bool enabled)
    {
        const std::string folderID = Quiet::Core::UUIDToString(folder.ID);
        const std::string displayName = folder.Name.empty() ? "Folder" : folder.Name;

        const float rowHeight = SZ::Px(38.0f);
        const float rowRounding = SZ::Px(7.0f);
        const float iconAreaWidth = SZ::Px(34.0f);
        const float leftPadding = SZ::Px(9.0f);
        const float rightPadding = SZ::Px(8.0f);

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const ImVec2 rowMax = ImVec2(rowMin.x + availableWidth, rowMin.y + rowHeight);

        ImGui::PushID(folderID.c_str());
        if (!enabled)
        {
            ImGui::BeginDisabled();
        }

        ImGui::InvisibleButton("##FolderRow", ImVec2(availableWidth, rowHeight));

        const bool hovered = enabled && ImGui::IsItemHovered();
        const bool clicked = enabled && ImGui::IsItemClicked(ImGuiMouseButton_Left);

        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (!enabled) ImGui::EndDisabled();

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        const ImVec4 backgroundColor = hovered
            ? QUI::EditorColor::Background3
            : ImVec4(
                QUI::EditorColor::Background2.x,
                QUI::EditorColor::Background2.y,
                QUI::EditorColor::Background2.z,
                0.46f
            );

        drawList->AddRectFilled(
            rowMin,
            rowMax,
            ImGui::ColorConvertFloat4ToU32(backgroundColor),
            rowRounding
        );

        drawList->AddRect(
            rowMin,
            rowMax,
            ImGui::ColorConvertFloat4ToU32(
                hovered ? QUI::EditorColor::Border : QUI::EditorColor::BorderSoft
            ),
            rowRounding
        );

        const char* disclosureIcon = folder.Expanded ? "v" : ">";
        const char* icon = QUI::Icons::Load;
        const char* typeLabel = "Folder";

        const ImVec2 disclosureSize = ImGui::CalcTextSize(disclosureIcon);
        const ImVec2 iconSize = ImGui::CalcTextSize(icon);
        const ImVec2 nameSize = ImGui::CalcTextSize(displayName.c_str());
        const ImVec2 typeSize = ImGui::CalcTextSize(typeLabel);

        const float centerY = rowMin.y + rowHeight * 0.5f;

        const ImVec2 disclosurePos = ImVec2(
            rowMin.x + leftPadding,
            centerY - disclosureSize.y * 0.5f
        );

        const ImVec2 iconPos = ImVec2(
            rowMin.x + leftPadding + SZ::Px(15.0f),
            centerY - iconSize.y * 0.5f
        );

        const ImVec2 namePos = ImVec2(
            rowMin.x + iconAreaWidth + SZ::Px(15.0f),
            centerY - nameSize.y * 0.5f
        );

        drawList->PushClipRect(rowMin, rowMax, true);

        drawList->AddText(
            disclosurePos,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::TextMuted),
            disclosureIcon
        );

        drawList->AddText(
            iconPos,
            ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::AccentSoft),
            icon
        );

        drawList->AddText(
            namePos,
            ImGui::ColorConvertFloat4ToU32(
                enabled ? QUI::EditorColor::Text : QUI::EditorColor::TextDim
            ),
            displayName.c_str()
        );

        if (availableWidth >= SZ::Px(230.0f))
        {
            const float badgeHeight = SZ::Px(20.0f);
            const float badgePaddingX = SZ::Px(8.0f);
            const float badgeWidth = typeSize.x + badgePaddingX * 2.0f;

            const ImVec2 badgeMin = ImVec2(
                rowMax.x - badgeWidth - rightPadding,
                centerY - badgeHeight * 0.5f
            );

            const ImVec2 badgeMax = ImVec2(
                badgeMin.x + badgeWidth,
                badgeMin.y + badgeHeight
            );

            drawList->AddRectFilled(
                badgeMin,
                badgeMax,
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::Background3),
                SZ::Px(6.0f)
            );

            drawList->AddRect(
                badgeMin,
                badgeMax,
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::BorderSoft),
                SZ::Px(6.0f)
            );

            drawList->AddText(
                ImVec2(
                    badgeMin.x + badgePaddingX,
                    centerY - typeSize.y * 0.5f
                ),
                ImGui::ColorConvertFloat4ToU32(QUI::EditorColor::TextMuted),
                typeLabel
            );
        }

        drawList->PopClipRect();
        ImGui::PopID();

        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            ImGui::SetTooltip("%s", folderID.c_str());

        return clicked;
    }

    void BeginFolderRename(const Quiet::Scene::SceneHierarchyFolder& folder)
    {
        g_RenameFolderID = folder.ID;

        std::snprintf(
            g_RenameFolderNameBuffer.data(),
            g_RenameFolderNameBuffer.size(),
            "%s",
            folder.Name.empty() ? "Folder" : folder.Name.c_str()
        );

        g_RequestOpenRenameFolderPopup = true;
        ImGui::CloseCurrentPopup();
    }

    void DrawRenameFolderPopup(Quiet::Scene::Scene& scene, Quiet::Editor::EditorCommands& editorCommands)
    {
        if (g_RequestOpenRenameFolderPopup)
        {
            ImGui::OpenPopup(kRenameFolderPopup);
            g_RequestOpenRenameFolderPopup = false;
        }

        if (!g_RenameFolderID.has_value()) return;

        ImGui::SetNextWindowSize(SZ::Px(ImVec2(380.0f, 0.0f)), ImGuiCond_Appearing );

        bool open = true;
        if (ImGui::BeginPopupModal(kRenameFolderPopup, &open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextUnformatted("Folder Name");
            ImGui::PopStyleColor();

            ImGui::PushItemWidth(SZ::Px(340.0f));

            if (ImGui::IsWindowAppearing())
                ImGui::SetKeyboardFocusHere();

            const bool submittedByEnter = ImGui::InputText(
                "##FolderName",
                g_RenameFolderNameBuffer.data(),
                g_RenameFolderNameBuffer.size(),
                ImGuiInputTextFlags_EnterReturnsTrue |
                ImGuiInputTextFlags_AutoSelectAll
            );

            ImGui::PopItemWidth();
            ImGui::Spacing();

            const std::string newName = g_RenameFolderNameBuffer.data();
            const bool canRename = g_RenameFolderID.has_value();

            const float buttonWidth = SZ::Px(110.0f);
            const float totalButtonWidth =
                buttonWidth * 2.0f + ImGui::GetStyle().ItemSpacing.x;

            ImGui::SetCursorPosX(
                ImGui::GetCursorPosX() +
                ImGui::GetContentRegionAvail().x -
                totalButtonWidth
            );

            if (QUI::EditorButton("Cancel", ImVec2(buttonWidth, 0.0f)))
            {
                g_RenameFolderID.reset();
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            if (QUI::EditorButton("Rename", ImVec2(buttonWidth, 0.0f), canRename) ||
                (submittedByEnter && canRename))
            {
                editorCommands.RenameHierarchyFolder(
                    scene,
                    g_RenameFolderID.value(),
                    newName
                );

                g_RenameFolderID.reset();
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        if (!open)
        {
            g_RenameFolderID.reset();
            g_RequestOpenRenameFolderPopup = false;
        }
    }

    void DrawPrimitiveCreateMenu(Quiet::Scene::Scene& scene,
                                 Quiet::Editor::EditorCommands& editorCommands,
                                 const Quiet::Project::ProjectContext& projectContext,
                                 const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        if (!QUI::EditorBeginMenu("Primitive Mesh")) return;

        for (const std::string& primitiveID : Quiet::Renderer::Vulkan::MeshPrimitives::GetBuiltInAssetIDs())
        {
            const bool loaded = meshLibrary.Has(primitiveID);
            const std::string label = primitiveID + " Actor";

            if (QUI::EditorMenuItem(label.c_str(), nullptr, false, loaded))
            {
                editorCommands.CreateStaticMeshActor(
                    scene,
                    label,
                    primitiveID,
                    projectContext,
                    meshLibrary
                );
            }

            if (!loaded && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
            {
                ImGui::SetTooltip("Primitive mesh '%s' is registered but not loaded.", primitiveID.c_str());
            }
        }
        ImGui::EndMenu();
    }

    void DrawImportedMeshCreateMenu(Quiet::Scene::Scene& scene,
                                    Quiet::Editor::EditorCommands& editorCommands,
                                    const Quiet::Project::ProjectContext& projectContext,
                                    const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        if (!QUI::EditorBeginMenu("Imported Mesh")) return;

        bool foundImportedMesh = false;
        for (const Quiet::Project::AssetRecord& record : projectContext.GetAssetRegistry().Records)
        {
            if (record.Type != Quiet::Project::AssetType::Mesh || record.BuiltIn) continue;

            foundImportedMesh = true;
            const bool loaded = meshLibrary.Has(record.ID);
            const std::string label = record.ID + " Actor";

            if (QUI::EditorMenuItem(label.c_str(), nullptr, false, loaded))
            {
                editorCommands.CreateStaticMeshActor(
                    scene,
                    label,
                    record.ID,
                    projectContext,
                    meshLibrary
                );
            }

            if (!loaded && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                ImGui::SetTooltip("Mesh asset '%s' is registered but not loaded.", record.ID.c_str());

            else if (loaded && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && !record.Path.empty())
                ImGui::SetTooltip("%s", record.Path.c_str());
        }
        if (!foundImportedMesh)
            QUI::DrawMutedText("No imported mesh assets.");

        ImGui::EndMenu();
    }
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildHierarchy(Scene::Scene& scene,
                                    Editor::EditorActions& editorActions,
                                    Editor::EditorCommands& editorCommands,
                                    Editor::EditorState& editorState,
                                    const Project::ProjectContext& projectContext,
                                    const Vulkan::MeshLibrary& meshLibrary)
    {
        ImGui::Begin("Scene");
        {
            const bool isEditMode = (editorState.Scene.Mode == Editor::SceneMode::Edit);

            if (DrawHierarchyHeader(isEditMode))
                ImGui::OpenPopup("CreateActorPopup");

            if (ImGui::BeginPopup("CreateActorPopup"))
            {
                if (EditorMenuItem("Empty Actor"))
                    editorActions.Execute(Editor::EditorActionID::CreateEmptyActor);

                if (EditorMenuItem("Directional Light"))
                    editorCommands.CreateDirectionalLightActor(scene);

                if (EditorMenuItem("Folder"))
                    editorCommands.CreateHierarchyFolder(scene);

                DrawPrimitiveCreateMenu(scene, editorCommands, projectContext, meshLibrary);
                DrawImportedMeshCreateMenu(scene, editorCommands, projectContext, meshLibrary);

                ImGui::EndPopup();
            }

            std::optional<Editor::EditorActionID> pendingAction;
            std::optional<Core::UUID> pendingActorSelection;
            std::optional<std::tuple<Scene::SceneHierarchyNodeType, Core::UUID, std::size_t>> pendingRootNodeMove;
            std::optional<std::pair<Core::UUID, Core::UUID>> pendingActorMoveToFolder;
            std::optional<Core::UUID> pendingActorMoveToRoot;
            std::optional<Core::UUID> pendingFolderDelete;

            Scene::SceneHierarchyState& hierarchy = scene.GetHierarchy();

            if (hierarchy.RootNodes.empty())
            {
                DrawHierarchyEmptyState();
            }
            else
            {
                for (std::size_t rootNodeIndex = 0; rootNodeIndex < hierarchy.RootNodes.size(); ++rootNodeIndex)
                {
                    const Scene::SceneHierarchyNode& rootNode = hierarchy.RootNodes[rootNodeIndex];

                    if (rootNode.Type == Scene::SceneHierarchyNodeType::Folder)
                    {
                        Scene::SceneHierarchyFolder* folder = scene.FindHierarchyFolder(rootNode.ID);
                        if (!folder) continue;

                        if (DrawFolderRow(*folder, isEditMode))
                        {
                            folder->Expanded = !folder->Expanded;
                            editorState.Selection.SelectedActorID.reset();
                        }

                        if (isEditMode)
                        {
                            BeginFolderDragSource(*folder);
                        }

                        const ImVec2 folderRowMin = ImGui::GetItemRectMin();
                        const ImVec2 folderRowMax = ImGui::GetItemRectMax();

                        if (isEditMode && ImGui::BeginDragDropTarget())
                        {
                            const float mouseY = ImGui::GetMousePos().y;
                            const float rowHeight = folderRowMax.y - folderRowMin.y;

                            const bool dropBefore = mouseY < folderRowMin.y + rowHeight * 0.25f;
                            const bool dropAfter = mouseY > folderRowMax.y - rowHeight * 0.25f;

                            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                                kActorDragDropPayload,
                                ImGuiDragDropFlags_AcceptBeforeDelivery |
                                ImGuiDragDropFlags_AcceptNoDrawDefaultRect
                            );

                            if (payload)
                            {
                                const bool wantsRootReorder = g_DraggedRootNode.has_value() && (dropBefore || dropAfter);
                                if (wantsRootReorder)
                                {
                                    auto pendingMove = BuildRootNodeDropMove(hierarchy, rootNodeIndex, dropAfter, Scene::SceneHierarchyNodeType::Folder, folder->ID);
                                    if (pendingMove.has_value())
                                    {
                                        DrawActorDropMarker(folderRowMin, folderRowMax, dropAfter);
                                        if (payload->IsDelivery())
                                        {
                                            pendingRootNodeMove = pendingMove.value();
                                            ResetHierarchyDragState();
                                        }
                                    }
                                }
                                else if (g_DraggedActorID.has_value())
                                {
                                    DrawFolderDropHighlight(folderRowMin, folderRowMax);
                                    if (payload->IsDelivery())
                                    {
                                        pendingActorMoveToFolder = std::make_pair(g_DraggedActorID.value(), folder->ID);
                                        folder->Expanded = true;
                                        ResetHierarchyDragState();
                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }

                        const std::string folderContextID = "FolderContextMenu_" + Core::UUIDToString(folder->ID);
                        if (ImGui::BeginPopupContextItem(folderContextID.c_str()))
                        {
                            const bool canEdit = editorState.Scene.Mode == Editor::SceneMode::Edit;
                            const bool canDelete = canEdit && folder->Children.empty();

                            const bool canMoveUp = canEdit && rootNodeIndex > 0;
                            const bool canMoveDown = canEdit && rootNodeIndex + 1 < hierarchy.RootNodes.size();

                            if (EditorMenuItem("Move Up", nullptr, false, canMoveUp))
                            {
                                pendingRootNodeMove = std::make_tuple(Scene::SceneHierarchyNodeType::Folder, folder->ID, rootNodeIndex - 1);
                            }

                            if (EditorMenuItem("Move Down", nullptr, false, canMoveDown))
                            {
                                pendingRootNodeMove = std::make_tuple(Scene::SceneHierarchyNodeType::Folder, folder->ID, rootNodeIndex + 1);
                            }

                            ImGui::Separator();

                            if (EditorMenuItem("Rename", nullptr, false, canEdit))
                            {
                                BeginFolderRename(*folder);
                            }

                            if (EditorMenuItem("Delete Empty Folder", nullptr, false, canDelete))
                            {
                                pendingFolderDelete = folder->ID;
                            }

                            if (!folder->Children.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                            {
                                ImGui::SetTooltip("Only empty folders can be deleted.");
                            }

                            ImGui::EndPopup();
                        }

                        if (folder->Expanded && !folder->Children.empty())
                        {
                            const float childIndent = SZ::Px(20.0f);

                            ImGui::Indent(childIndent);

                            const ImVec2 childGuideStart = ImGui::GetCursorScreenPos();
                            const float childGuideX = childGuideStart.x - SZ::Px(10.0f);
                            const float childGuideY0 = childGuideStart.y + SZ::Px(3.0f);

                            for (const Scene::SceneHierarchyNode& childNode : folder->Children)
                            {
                                if (childNode.Type != Scene::SceneHierarchyNodeType::Actor) continue;
                                Scene::Actor* childActor = scene.FindActorByID(childNode.ID);
                                if (!childActor) continue;

                                const Core::UUID childActorID = childActor->GetID();

                                const bool isSelected =
                                    editorState.Selection.SelectedActorID.has_value() &&
                                    editorState.Selection.SelectedActorID.value() == childActorID;

                                if (DrawActorRow(*childActor, isSelected, isEditMode))
                                {
                                    editorState.Selection.SelectedActorID = childActorID;
                                }

                                if (isEditMode)
                                {
                                    BeginActorDragSource(*childActor);
                                }

                                const std::string childActorContextID = "FolderChildActorContextMenu_" + Core::UUIDToString(childActorID);
                                if (ImGui::BeginPopupContextItem(childActorContextID.c_str()))
                                {
                                    const bool canEdit = editorState.Scene.Mode == Editor::SceneMode::Edit;

                                    if (EditorMenuItem("Move To Root", nullptr, false, canEdit))
                                    {
                                        pendingActorMoveToRoot = childActorID;
                                    }

                                    ImGui::Separator();

                                    if (EditorMenuItem("Duplicate", editorActions.GetShortcut(Editor::EditorActionID::DuplicateSelected), false, canEdit))
                                    {
                                        pendingActorSelection = childActorID;
                                        pendingAction = Editor::EditorActionID::DuplicateSelected;
                                    }

                                    if (EditorMenuItem("Delete", editorActions.GetShortcut(Editor::EditorActionID::DeleteSelected), false, canEdit))
                                    {
                                        pendingActorSelection = childActorID;
                                        pendingAction = Editor::EditorActionID::DeleteSelected;
                                    }

                                    ImGui::EndPopup();
                                }
                            }
                            const float childGuideY1 = ImGui::GetCursorScreenPos().y - SZ::Px(4.0f);

                            if (childGuideY1 > childGuideY0)
                            {
                                ImGui::GetWindowDrawList()->AddLine(
                                    ImVec2(childGuideX, childGuideY0),
                                    ImVec2(childGuideX, childGuideY1),
                                    ImGui::ColorConvertFloat4ToU32(EditorColor::BorderSoft),
                                    SZ::Px(1.0f)
                                );
                            }

                            ImGui::Unindent(childIndent);
                        }
                        continue;
                    }

                    Scene::Actor* actor = scene.FindActorByID(rootNode.ID);
                    if (!actor) continue;

                    const Core::UUID actorID = actor->GetID();

                    const bool isSelected =
                        editorState.Selection.SelectedActorID.has_value() &&
                        editorState.Selection.SelectedActorID.value() == actorID;

                    if (DrawActorRow(*actor, isSelected, isEditMode))
                    {
                        editorState.Selection.SelectedActorID = actorID;
                    }

                    const ImVec2 rowMin = ImGui::GetItemRectMin();
                    const ImVec2 rowMax = ImGui::GetItemRectMax();

                    if (isEditMode)
                    {
                        BeginRootActorDragSource(*actor);

                        if (ImGui::BeginDragDropTarget())
                        {
                            const float mouseY = ImGui::GetMousePos().y;
                            const float rowMidY = rowMin.y + (rowMax.y - rowMin.y) * 0.5f;
                            const bool dropAfterTarget = mouseY > rowMidY;

                            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                                kActorDragDropPayload,
                                ImGuiDragDropFlags_AcceptBeforeDelivery |
                                ImGuiDragDropFlags_AcceptNoDrawDefaultRect
                            );

                            if (payload && g_DraggedRootNode.has_value())
                            {
                                auto pendingMove = BuildRootNodeDropMove(
                                    hierarchy,
                                    rootNodeIndex,
                                    dropAfterTarget,
                                    Scene::SceneHierarchyNodeType::Actor,
                                    actorID
                                );

                                if (pendingMove.has_value())
                                {
                                    DrawActorDropMarker(rowMin, rowMax, dropAfterTarget);

                                    if (payload->IsDelivery())
                                    {
                                        pendingRootNodeMove = pendingMove.value();
                                        ResetHierarchyDragState();
                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }

                    const std::string actorContextID = "ActorContextMenu_" + Core::UUIDToString(actorID);

                    if (ImGui::BeginPopupContextItem(actorContextID.c_str()))
                    {
                        const bool canEdit = editorState.Scene.Mode == Editor::SceneMode::Edit;
                        const bool canMoveUp = canEdit && rootNodeIndex > 0;
                        const bool canMoveDown = canEdit && rootNodeIndex + 1 < hierarchy.RootNodes.size();

                        if (EditorMenuItem("Move Up", nullptr, false, canMoveUp))
                            pendingRootNodeMove = std::make_tuple(Scene::SceneHierarchyNodeType::Actor, actorID, rootNodeIndex - 1);

                        if (EditorMenuItem("Move Down", nullptr, false, canMoveDown))
                            pendingRootNodeMove = std::make_tuple(Scene::SceneHierarchyNodeType::Actor, actorID, rootNodeIndex + 1);

                        ImGui::Separator();

                        if (EditorMenuItem("Duplicate", editorActions.GetShortcut(Editor::EditorActionID::DuplicateSelected), false, canEdit))
                        {
                            pendingActorSelection = actorID;
                            pendingAction = Editor::EditorActionID::DuplicateSelected;
                        }

                        if (EditorMenuItem("Delete", editorActions.GetShortcut(Editor::EditorActionID::DeleteSelected), false, canEdit))
                        {
                            pendingActorSelection = actorID;
                            pendingAction = Editor::EditorActionID::DeleteSelected;
                        }

                        ImGui::EndPopup();
                    }
                }

                if (isEditMode)
                {
                    ImGui::Spacing();

                    const float dropWidth = ImGui::GetContentRegionAvail().x;
                    const float dropHeight = SZ::Px(42.0f);

                    if (dropWidth > SZ::Px(32.0f))
                    {
                        const ImVec2 dropMin = ImGui::GetCursorScreenPos();

                        ImGui::InvisibleButton(
                            "##HierarchyRootDropTarget",
                            ImVec2(dropWidth, dropHeight)
                        );

                        const ImVec2 dropMax = ImGui::GetItemRectMax();

                        if (ImGui::BeginDragDropTarget())
                        {
                            const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
                                kActorDragDropPayload,
                                ImGuiDragDropFlags_AcceptBeforeDelivery |
                                ImGuiDragDropFlags_AcceptNoDrawDefaultRect
                            );

                            if (payload && (g_DraggedActorID.has_value() || g_DraggedRootNode.has_value()))
                            {
                                DrawFolderDropHighlight(dropMin, dropMax);
                                ImDrawList* drawList = ImGui::GetWindowDrawList();

                                const char* hint = g_DraggedRootNode.has_value()
                                    ? "Drop here to move to bottom"
                                    : "Drop here to move actor to root";

                                const ImVec2 hintSize = ImGui::CalcTextSize(hint);
                                const ImVec2 hintPos = ImVec2(
                                    dropMin.x + (dropWidth - hintSize.x) * 0.5f,
                                    dropMin.y + (dropHeight - hintSize.y) * 0.5f
                                );

                                drawList->AddText(
                                    hintPos,
                                    ImGui::ColorConvertFloat4ToU32(EditorColor::TextMuted),
                                    hint
                                );

                                if (payload->IsDelivery())
                                {
                                    if (g_DraggedRootNode.has_value() && !hierarchy.RootNodes.empty())
                                    {
                                        const auto [sourceType, sourceID] = g_DraggedRootNode.value();
                                        pendingRootNodeMove = std::make_tuple(
                                            sourceType,
                                            sourceID,
                                            hierarchy.RootNodes.size() - 1
                                        );
                                    }
                                    else if (g_DraggedActorID.has_value())
                                    {
                                        pendingActorMoveToRoot = g_DraggedActorID.value();
                                    }
                                    ResetHierarchyDragState();
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }
                }
            }

            DrawRenameFolderPopup(scene, editorCommands);

            if (pendingFolderDelete.has_value())
            {
                editorCommands.DeleteEmptyHierarchyFolder(
                    scene,
                    pendingFolderDelete.value()
                );
            }
            else if (pendingRootNodeMove.has_value())
            {
                editorCommands.MoveHierarchyRootNode(
                    scene,
                    std::get<0>(pendingRootNodeMove.value()),
                    std::get<1>(pendingRootNodeMove.value()),
                    std::get<2>(pendingRootNodeMove.value())
                );
            }
            else if (pendingActorMoveToFolder.has_value())
            {
                editorCommands.MoveActorToHierarchyFolder(
                    scene,
                    pendingActorMoveToFolder->first,
                    pendingActorMoveToFolder->second
                );
            }
            else if (pendingActorMoveToRoot.has_value())
            {
                editorCommands.MoveActorToHierarchyRoot(
                    scene,
                    pendingActorMoveToRoot.value()
                );
            }
            else if (pendingAction.has_value() && pendingActorSelection.has_value())
            {
                editorState.Selection.SelectedActorID = pendingActorSelection.value();
                editorActions.Execute(pendingAction.value());
            }
        }
        if (ImGui::GetDragDropPayload() == nullptr)
        {
            ResetHierarchyDragState();
        }

        ImGui::End();
    }
}
