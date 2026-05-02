#include "Renderer/UI/ImGuiLayer.h"

#include "Quiet/Editor/EditorActionID.h"
#include "Quiet/Editor/EditorActions.h"
#include "Quiet/Editor/EditorCommands.h"
#include "Quiet/Editor/EditorState.h"
#include "Quiet/Project/AssetRecord.h"
#include "Quiet/Project/AssetType.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/Vulkan/Mesh/MeshLibrary.h"
#include "Renderer/Vulkan/Mesh/MeshPrimitives.h"
#include "Scene/Scene.h"

namespace
{
    void DrawPrimitiveCreateMenu(Quiet::Scene::Scene& scene,
                                 Quiet::Editor::EditorCommands& editorCommands,
                                 const Quiet::Project::ProjectContext& projectContext,
                                 const Quiet::Renderer::Vulkan::MeshLibrary& meshLibrary)
    {
        if (!ImGui::BeginMenu("Primitive Mesh")) return;

        for (const std::string& primitiveID : Quiet::Renderer::Vulkan::MeshPrimitives::GetBuiltInAssetIDs())
        {
            const bool loaded = meshLibrary.Has(primitiveID);
            const std::string label = primitiveID + " Actor";

            if (!loaded) ImGui::BeginDisabled();
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

            if (!loaded)
            {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
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
        if (!ImGui::BeginMenu("Imported Mesh")) return;

        bool foundImportedMesh = false;
        for (const Quiet::Project::AssetRecord& record : projectContext.GetAssetRegistry().Records)
        {
            if (record.Type != Quiet::Project::AssetType::Mesh || record.BuiltIn) continue;

            foundImportedMesh = true;
            const bool loaded = meshLibrary.Has(record.ID);
            const std::string label = record.ID + " Actor";

            if (!loaded) ImGui::BeginDisabled();
            if (ImGui::MenuItem(label.c_str()))
            {
                editorCommands.CreateStaticMeshActor(
                    scene,
                    label,
                    record.ID,
                    projectContext,
                    meshLibrary
                );
            }

            if (!loaded)
            {
                ImGui::EndDisabled();
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("Mesh asset '%s' is registered but not loaded.", record.ID.c_str());
            }
            else if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort) && !record.Path.empty())
                ImGui::SetTooltip("%s", record.Path.c_str());
        }
        if (!foundImportedMesh)
            ImGui::TextDisabled("No imported mesh assets.");

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
            if (!isEditMode) ImGui::BeginDisabled();

            if (ImGui::Button("Create"))
                ImGui::OpenPopup("CreateActorPopup");

            if (!isEditMode) ImGui::EndDisabled();

            if (ImGui::BeginPopup("CreateActorPopup"))
            {
                if (ImGui::MenuItem("Empty Actor"))
                    editorActions.Execute(Editor::EditorActionID::CreateEmptyActor);

                if (ImGui::MenuItem("Directional Light"))
                {
                    editorCommands.CreateDirectionalLightActor(scene);
                }

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

            ImGui::Separator();

            std::optional<Editor::EditorActionID> pendingAction;
            std::optional<Core::UUID> pendingActorSelection;

            auto& actors = scene.GetActors();
            for (auto & actor : actors)
            {
                const bool isSelected =
                    editorState.Selection.SelectedActorID.has_value() &&
                    editorState.Selection.SelectedActorID.value() == actor.ID;

                const std::string displayName = actor.Name.empty() ? "Unnamed Actor" : actor.Name;
                const std::string label = displayName + "##" + Core::UUIDToString(actor.ID);

                if (!isEditMode) ImGui::BeginDisabled();

                if (ImGui::Selectable(label.c_str(), isSelected) && isEditMode)
                    editorState.Selection.SelectedActorID = actor.ID;

                if (!isEditMode) ImGui::EndDisabled();

                if (ImGui::BeginPopupContextItem(label.c_str()))
                {
                    const bool canEdit = (editorState.Scene.Mode == Editor::SceneMode::Edit);

                    if (ImGui::MenuItem("Duplicate",
                                        editorActions.GetShortcut(Editor::EditorActionID::DuplicateSelected),
                                        false,
                                        canEdit))
                    {
                        pendingActorSelection = actor.ID;
                        pendingAction = Editor::EditorActionID::DuplicateSelected;
                    }

                    if (ImGui::MenuItem("Delete",
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

            if (pendingAction.has_value() && pendingActorSelection.has_value())
            {
                editorState.Selection.SelectedActorID = pendingActorSelection.value();
                editorActions.Execute(pendingAction.value());
            }
        }
        ImGui::End();
    }
}
