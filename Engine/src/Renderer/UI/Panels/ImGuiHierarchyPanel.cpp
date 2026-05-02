#include "Renderer/UI/ImGuiLayer.h"

#include "Quiet/Editor/EditorActionID.h"
#include "Quiet/Editor/EditorActions.h"
#include "Quiet/Editor/EditorCommands.h"
#include "Quiet/Editor/EditorState.h"
#include "Renderer/Vulkan/Mesh/MeshPrimitives.h"
#include "Scene/Scene.h"

namespace
{
    bool DrawToolbarAction(Quiet::Editor::EditorActions& actions,
                           Quiet::Editor::EditorActionID actionID,
                           const char* buttonLabel = nullptr)
    {
        const bool enabled = actions.CanExecute(actionID);
        if (!enabled) ImGui::BeginDisabled();

        const bool pressed = ImGui::Button(buttonLabel ? buttonLabel : actions.GetLabel(actionID));

        if (!enabled) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            const char* shortcut = actions.GetShortcut(actionID);
            if (shortcut[0] != '\0')
                ImGui::SetTooltip("%s", shortcut);
        }

        if (pressed) actions.Execute(actionID);
        return pressed;
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
            if (!isEditMode) ImGui::BeginDisabled();

            if (ImGui::Button("Create"))
                ImGui::OpenPopup("CreateActorPopup");

            if (!isEditMode) ImGui::EndDisabled();

            if (ImGui::BeginPopup("CreateActorPopup"))
            {
                if (ImGui::MenuItem("Empty Actor"))
                    editorActions.Execute(Editor::EditorActionID::CreateEmptyActor);

                ImGui::Separator();
                if (ImGui::BeginMenu("Primitive Mesh"))
                {
                    for (const std::string& primitiveID : Vulkan::MeshPrimitives::GetBuiltInAssetIDs())
                    {
                        const std::string label = primitiveID + " Actor";
                        if (ImGui::MenuItem(label.c_str()))
                        {
                            editorCommands.CreateStaticMeshActor(
                                scene,
                                label,
                                primitiveID,
                                projectContext,
                                meshLibrary
                            );
                        }
                    }
                    ImGui::EndMenu();
                }
                ImGui::EndPopup();
            }

            ImGui::Separator();

            std::optional<Editor::EditorActionID> pendingAction;
            std::optional<Core::UUID> pendingActorSelection;

            auto& actors = scene.GetActors();
            for (std::size_t i = 0; i < actors.size(); ++i)
            {
                const bool isSelected =
                    editorState.Selection.SelectedActorID.has_value() &&
                    editorState.Selection.SelectedActorID.value() == actors[i].ID;

                const std::string displayName = actors[i].Name.empty() ? "Unnamed Actor" : actors[i].Name;
                const std::string label = displayName + "##" + Core::UUIDToString(actors[i].ID);

                if (!isEditMode) ImGui::BeginDisabled();

                if (ImGui::Selectable(label.c_str(), isSelected) && isEditMode)
                    editorState.Selection.SelectedActorID = actors[i].ID;

                if (!isEditMode) ImGui::EndDisabled();

                if (ImGui::BeginPopupContextItem(label.c_str()))
                {
                    const bool canEdit = (editorState.Scene.Mode == Editor::SceneMode::Edit);

                    if (ImGui::MenuItem("Duplicate",
                                        editorActions.GetShortcut(Editor::EditorActionID::DuplicateSelected),
                                        false,
                                        canEdit))
                    {
                        pendingActorSelection = actors[i].ID;
                        pendingAction = Editor::EditorActionID::DuplicateSelected;
                    }

                    if (ImGui::MenuItem("Delete",
                                        editorActions.GetShortcut(Editor::EditorActionID::DeleteSelected),
                                        false,
                                        canEdit))
                    {
                        pendingActorSelection = actors[i].ID;
                        pendingAction = Editor::EditorActionID::DeleteSelected;
                    }

                    ImGui::EndPopup();
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
