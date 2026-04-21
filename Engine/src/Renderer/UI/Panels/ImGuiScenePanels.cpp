#include "../ImGuiLayer.h"

#include <cstdio>
#include <optional>
#include <string>

#include <imgui.h>

#include "Renderer/Vulkan/MeshLibrary.h"
#include "Quiet/Editor/EditorActions.h"
#include "Quiet/Editor/EditorCommands.h"
#include "Quiet/Editor/EditorState.h"
#include "Scene/Actor.h"
#include "Scene/Scene.h"

namespace
{
    bool DrawToolbarAction(Quiet::Editor::EditorActions& actions,
                           Quiet::Editor::EditorActionID actionID,
                           const char* buttonLabel = nullptr)
    {
        const bool enabled = actions.CanExecute(actionID);
        if (!enabled) ImGui::BeginDisabled();

        const bool pressed = ImGui::Button(buttonLabel ? buttonLabel : actions.GetLabel(actionID)
        );

        if (!enabled) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
        {
            const char* shortcut = actions.GetShortcut(actionID);
            if (shortcut[0] != '\0')
            {
                ImGui::SetTooltip("%s", shortcut);
            }
        }
        if (pressed) actions.Execute(actionID);

        return pressed;
    }
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildInspector(Scene::Scene &scene,
                                    Editor::EditorCommands &commands,
                                    Editor::EditorState &editorState,
                                    const Vulkan::MeshLibrary &meshLibrary)
    {
        ImGui::Begin("Inspector");
        {
            Scene::Actor* selectedActor = nullptr;
            if (editorState.Selection.SelectedActorID.has_value())
            {
                selectedActor = scene.FindActorByID(editorState.Selection.SelectedActorID.value());
                if (!selectedActor) editorState.Selection.SelectedActorID.reset();
            }

            if (!selectedActor)
            {
                ImGui::Text("No actor selected.");
                ImGui::End();
                return;
            }

            const bool isEditMode = editorState.Scene.Mode == Editor::SceneMode::Edit;
            auto& actor = *selectedActor;
            auto& transform = actor.Transform.WorldTransform;

            auto beginTrackedItem = [&]()
            {
                if (isEditMode && ImGui::IsItemActivated())
                {
                    commands.BeginInteraction(scene);
                }
            };

            auto endTrackedItem = [&]()
            {
                if (isEditMode && ImGui::IsItemDeactivated())
                {
                    commands.EndInteraction(scene);
                }
            };

            char nameBuffer[256]{};
            std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", actor.Name.c_str());

            ImGui::Text("Actor");
            ImGui::Separator();

            if (!isEditMode)
            {
                ImGui::TextColored(
                    ImVec4(0.30f, 0.85f, 0.35f, 1.0f),
                    "Simulating runtime copy. Editing is disabled."
                );
                ImGui::Separator();
            }
            if (!isEditMode) ImGui::BeginDisabled();

            if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer)))
            {
                commands.RenameActor(scene, actor.ID, nameBuffer);
            }
            beginTrackedItem();
            endTrackedItem();

            if (ImGui::DragFloat3("Position", &transform.Position.x, 0.1f))
            {
                commands.SetActorPosition(scene, actor.ID, transform.Position);
            }
            beginTrackedItem();
            endTrackedItem();

            if (ImGui::DragFloat3("Rotation", &transform.RotationDegrees.x, 0.5f))
            {
                commands.SetActorRotation(scene, actor.ID, transform.RotationDegrees);
            }
            beginTrackedItem();
            endTrackedItem();

            if (ImGui::DragFloat3("Scale", &transform.Scale.x, 0.05f, 0.01f, 100.0f))
            {
                commands.SetActorScale(scene, actor.ID, transform.Scale);
            }
            beginTrackedItem();
            endTrackedItem();

            ImGui::Separator();
            ImGui::Text("Components");

            if (!actor.StaticMesh.has_value())
            {
                if (ImGui::Button("Add Static Mesh"))
                {
                    commands.AddStaticMesh(scene, actor.ID, meshLibrary);
                }
            }
            else
            {
                if (ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const auto names = meshLibrary.GetNames();

                    int currentIndex = 0;
                    for (int i = 0; i < static_cast<int>(names.size()); ++i)
                    {
                        if (names[i] == actor.StaticMesh->MeshResourceID)
                        {
                            currentIndex = i;
                            break;
                        }
                    }

                    const char* currentPreview = names.empty() ? "<none>" : names[currentIndex].c_str();

                    if (ImGui::BeginCombo("Mesh Resource", currentPreview))
                    {
                        for (int i = 0; i < static_cast<int>(names.size()); ++i)
                        {
                            const bool isSelected = (i == currentIndex);
                            if (ImGui::Selectable(names[i].c_str(), isSelected))
                            {
                                commands.SetStaticMeshResource(scene, actor.ID, names[i], meshLibrary);
                            }
                            if (isSelected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }

                    bool visible = actor.StaticMesh->Visible;
                    if (ImGui::Checkbox("Visible", &visible))
                    {
                        commands.SetStaticMeshVisible(scene, actor.ID, visible);
                    }

                    if (ImGui::Button("Remove Static Mesh"))
                    {
                        commands.RemoveStaticMesh(scene, actor.ID);
                    }
                }
            }

            if (!actor.Rotator.has_value())
            {
                if (ImGui::Button("Add Rotator"))
                {
                    commands.AddRotator(scene, actor.ID);
                }
            }
            else
            {
                if (ImGui::CollapsingHeader("Rotator", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    if (ImGui::DragFloat("Rotation Speed",
                                         &actor.Rotator->RotationSpeedDegreesPerSecond,
                                         1.0f))
                    {
                        commands.SetRotatorSpeed(
                            scene,
                            actor.ID,
                            actor.Rotator->RotationSpeedDegreesPerSecond
                        );
                    }
                    beginTrackedItem();
                    endTrackedItem();

                    if (ImGui::Button("Remove Rotator"))
                    {
                        commands.RemoveRotator(scene, actor.ID);
                    }
                }
            }

            if (!actor.Camera.has_value())
            {
                if (ImGui::Button("Add Camera"))
                {
                    commands.AddCamera(scene, actor.ID);
                }
            }
            else
            {
                if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
                {
                    const bool isPilotingThisCamera =
                        editorState.Selection.PilotedCameraActorID.has_value() &&
                        editorState.Selection.PilotedCameraActorID.value() == actor.ID;

                    if (isEditMode)
                    {
                        if (!isPilotingThisCamera)
                        {
                            if (ImGui::Button("Pilot Camera"))
                            {
                                editorState.Selection.PilotedCameraActorID = actor.ID;
                            }
                        }
                        else
                        {
                            if (ImGui::Button("Stop Piloting"))
                            {
                                editorState.Selection.PilotedCameraActorID.reset();
                            }
                        }
                    }

                    bool primary = actor.Camera->Primary;
                    if (ImGui::Checkbox("Primary", &primary))
                    {
                        commands.SetCameraPrimary(scene, actor.ID, primary);
                    }

                    float fov = actor.Camera->FieldOfViewDegrees;
                    if (ImGui::DragFloat("Field Of View", &fov, 0.25f, 1.0f, 179.0f))
                    {
                        commands.SetCameraFieldOfView(scene, actor.ID, fov);
                    }
                    beginTrackedItem();
                    endTrackedItem();

                    float nearClip = actor.Camera->NearClip;
                    if (ImGui::DragFloat("Near Clip", &nearClip, 0.01f, 0.001f, 100.0f))
                    {
                        commands.SetCameraNearClip(scene, actor.ID, nearClip);
                    }
                    beginTrackedItem();
                    endTrackedItem();

                    float farClip = actor.Camera->FarClip;
                    if (ImGui::DragFloat("Far Clip", &farClip, 1.0f, 1.0f, 100000.0f))
                    {
                        commands.SetCameraFarClip(scene, actor.ID, farClip);
                    }
                    beginTrackedItem();
                    endTrackedItem();

                    if (ImGui::Button("Remove Camera"))
                    {
                        commands.RemoveCamera(scene, actor.ID);
                    }
                }
            }

            if (!isEditMode)
            {
                ImGui::EndDisabled();
            }
        }
        ImGui::End();
    }

    void ImGuiLayer::BuildHierarchy(Scene::Scene &scene,
                                    Editor::EditorActions &editorActions,
                                    Editor::EditorState &editorState)
    {
        ImGui::Begin("Scene");
        {
            const bool isEditMode = (editorState.Scene.Mode == Editor::SceneMode::Edit);
            DrawToolbarAction(editorActions, Editor::EditorActionID::CreateEmptyActor, "Create Empty");

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

                if (!isEditMode)
                {
                    ImGui::BeginDisabled();
                }

                if (ImGui::Selectable(label.c_str(), isSelected) && isEditMode)
                {
                    editorState.Selection.SelectedActorID = actors[i].ID;
                }

                if (!isEditMode)
                {
                    ImGui::EndDisabled();
                }

                if (ImGui::BeginPopupContextItem(label.c_str()))
                {
                    const bool canEdit =
                        editorState.Scene.Mode == Editor::SceneMode::Edit;

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
