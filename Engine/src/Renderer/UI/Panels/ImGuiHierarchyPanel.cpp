#include "Renderer/UI/ImGuiLayer.h"

#include <string>

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
        ImGui::TextWrapped("Actors in the active scene.");
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
            ImGui::TextUnformatted("No actors in scene");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped("Use + Create to add an Empty Actor, Light, or Mesh Actor.");
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    const char* GetActorTypeLabel(const Quiet::Scene::Actor& actor)
    {
        if (actor.Camera.has_value()) return "Camera";
        if (actor.DirectionalLight.has_value()) return "Light";
        if (actor.StaticMesh.has_value()) return "Mesh";
        if (actor.Rotator.has_value()) return "Rotator";

        return "Actor";
    }

    const char* GetActorTypeIcon(const Quiet::Scene::Actor& actor)
    {
        if (actor.Camera.has_value()) return QUI::Icons::Focus;
        if (actor.DirectionalLight.has_value()) return QUI::Icons::World;
        if (actor.StaticMesh.has_value()) return QUI::Icons::Local;
        if (actor.Rotator.has_value()) return QUI::Icons::Rotate;

        return QUI::Icons::Local;
    }

    bool DrawActorRow(const Quiet::Scene::Actor& actor, const bool selected, const bool enabled)
    {
        const std::string actorID = Quiet::Core::UUIDToString(actor.ID);
        const std::string displayName = actor.Name.empty() ? "Unnamed Actor" : actor.Name;

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
                if (QUI::EditorMenuItem("Empty Actor"))
                    editorActions.Execute(Editor::EditorActionID::CreateEmptyActor);

                if (QUI::EditorMenuItem("Directional Light"))
                    editorCommands.CreateDirectionalLightActor(scene);

                DrawPrimitiveCreateMenu(
                    scene,
                    editorCommands,
                    projectContext,
                    meshLibrary
                );

                DrawImportedMeshCreateMenu(
                    scene,
                    editorCommands,
                    projectContext,
                    meshLibrary
                );

                ImGui::EndPopup();
            }

            ImGui::Spacing();
            DrawSectionTitle("Actors");

            std::optional<Editor::EditorActionID> pendingAction;
            std::optional<Core::UUID> pendingActorSelection;

            auto& actors = scene.GetActors();

            if (actors.empty())
                DrawHierarchyEmptyState();
            else
            {
                for (auto& actor : actors)
                {
                    const bool isSelected =
                        editorState.Selection.SelectedActorID.has_value() &&
                        editorState.Selection.SelectedActorID.value() == actor.ID;

                    if (DrawActorRow(actor, isSelected, isEditMode))
                        editorState.Selection.SelectedActorID = actor.ID;

                    const std::string actorContextID = "ActorContextMenu_" + Core::UUIDToString(actor.ID);

                    if (ImGui::BeginPopupContextItem(actorContextID.c_str()))
                    {
                        const bool canEdit = (editorState.Scene.Mode == Editor::SceneMode::Edit);

                        if (QUI::EditorMenuItem(
                                "Duplicate",
                                editorActions.GetShortcut(Editor::EditorActionID::DuplicateSelected),
                                false,
                                canEdit))
                        {
                            pendingActorSelection = actor.ID;
                            pendingAction = Editor::EditorActionID::DuplicateSelected;
                        }

                        if (QUI::EditorMenuItem(
                                "Delete",
                                editorActions.GetShortcut(Editor::EditorActionID::DeleteSelected),
                                false,
                                canEdit))
                        {
                            pendingActorSelection = actor.ID;
                            pendingAction = Editor::EditorActionID::DeleteSelected;
                        }

                        ImGui::EndPopup();
                    }
                }
            }

            if (pendingAction.has_value() && pendingActorSelection.has_value())
            {
                editorState.Selection.SelectedActorID = pendingActorSelection.value();
                editorActions.Execute(pendingAction.value());
            }
        }
        ImGui::End();
    }
}
