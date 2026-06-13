#include "Renderer/UI/ImGuiLayer.h"

#include <algorithm>
#include <glm/geometric.hpp>
#include <imgui.h>

#include "Quiet/Editor/EditorCommands.h"
#include "Quiet/Editor/EditorState.h"
#include "Renderer/UI/ImGuiTheme.h"
#include "Renderer/UI/ImGuiToolbarCommon.h"
#include "Scene/Scene.h"
#include "Scene/SceneRenderSettings.h"


namespace
{
    namespace QUI = Quiet::Renderer::UI;
    namespace SZ = Quiet::Renderer::UI::EditorSizing;

    struct TrackedSceneSettingsInteraction
    {
        Quiet::Scene::Scene& SceneRef;
        Quiet::Editor::EditorCommands& Commands;
        bool Enabled = false;

        void Begin() const
        {
            if (Enabled && ImGui::IsItemActivated()) Commands.BeginInteraction(SceneRef);
        }

        void End() const
        {
            if (Enabled && ImGui::IsItemDeactivated()) Commands.EndInteraction(SceneRef);
        }
    };

    void DrawSceneSettingsBanner(const char* message)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(10.0f, 8.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::AccentDim);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::AccentSoft);

        ImGui::BeginChild("##SceneSettingsBanner", ImVec2(0.0f, SZ::Px(44.0f)), ImGuiChildFlags_Borders);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextWrapped("%s", message);
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    void DrawPanelIntro()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, SZ::Px(ImVec2(14.0f, 12.0f)));

        ImGui::PushStyleColor(ImGuiCol_ChildBg, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border, QUI::EditorColor::BorderSoft);

        ImGui::BeginChild("##SceneSettingsIntro", ImVec2(0.0f, SZ::Px(92.0f)), ImGuiChildFlags_Borders);
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::Text);
            ImGui::TextUnformatted("Scene Settings");
            ImGui::PopStyleColor();

            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
            ImGui::TextWrapped(
                "Edit scene-wide lighting and ambient render settings. "
                "These values are saved with the scene."
            );
            ImGui::PopStyleColor();
        }
        ImGui::EndChild();

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }

    float GetPropertyLabelWidth()
    {
        const float availableWidth = ImGui::GetContentRegionAvail().x;
        return std::clamp(
            availableWidth * 0.30f,
            SZ::Px(104.0f),
            SZ::Px(142.0f)
        );
    }

    void BeginPropertyRow(const char* label)
    {
        const float rowStartX = ImGui::GetCursorPosX();
        const float labelWidth = GetPropertyLabelWidth();

        ImGui::AlignTextToFramePadding();

        ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextMuted);
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        ImGui::SameLine(rowStartX + labelWidth);
    }

    bool DrawFloatRow(const char* label, float& value, const float speed, const float minValue, const float maxValue,
                      const TrackedSceneSettingsInteraction& tracked)
    {
        ImGui::PushID(label);

        BeginPropertyRow(label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        const bool changed = ImGui::DragFloat(
            "##Value",
            &value,
            speed,
            minValue,
            maxValue,
            "%.3f"
        );

        tracked.Begin();
        tracked.End();

        ImGui::PopID();
        return changed;
    }

    bool DrawVec3Row(const char* label, glm::vec3& value, const float speed, const float minValue, const float maxValue,
                     const TrackedSceneSettingsInteraction& tracked)
    {
        ImGui::PushID(label);

        BeginPropertyRow(label);

        const float availableWidth = ImGui::GetContentRegionAvail().x;
        const float axisLabelWidth = SZ::Px(14.0f);
        const float spacing = ImGui::GetStyle().ItemInnerSpacing.x;

        const float fieldWidth = std::max(
            SZ::Px(54.0f),
            (availableWidth - axisLabelWidth * 3.0f - spacing * 5.0f) / 3.0f
        );

        bool changed = false;
        auto drawAxis = [&](const char* axisLabel, float& axisValue)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, QUI::EditorColor::TextDim);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(axisLabel);
            ImGui::PopStyleColor();

            ImGui::SameLine();

            ImGui::SetNextItemWidth(fieldWidth);

            changed |= ImGui::DragFloat(
                "##Axis",
                &axisValue,
                speed,
                minValue,
                maxValue,
                "%.3f"
            );

            tracked.Begin();
            tracked.End();
        };

        ImGui::PushID("X");
        drawAxis("X", value.x);
        ImGui::PopID();

        ImGui::SameLine();

        ImGui::PushID("Y");
        drawAxis("Y", value.y);
        ImGui::PopID();

        ImGui::SameLine();

        ImGui::PushID("Z");
        drawAxis("Z", value.z);
        ImGui::PopID();

        ImGui::PopID();

        return changed;
    }

    bool DrawColor3Row(const char* label, glm::vec3& value, const TrackedSceneSettingsInteraction& tracked)
    {
        ImGui::PushID(label);

        BeginPropertyRow(label);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);

        const bool changed = ImGui::ColorEdit3("##Value", &value.x, ImGuiColorEditFlags_Float);

        tracked.Begin();
        tracked.End();

        ImGui::PopID();
        return changed;
    }

    bool BeginSettingsCard(const char* title)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, SZ::Px(ImVec2(8.0f, 6.0f)));
        ImGui::PushStyleColor(ImGuiCol_Header, QUI::EditorColor::Background2);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, QUI::EditorColor::Background4);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive, QUI::EditorColor::AccentDim);

        const bool open = ImGui::CollapsingHeader(title, ImGuiTreeNodeFlags_DefaultOpen);

        ImGui::PopStyleColor(3);
        ImGui::PopStyleVar();

        return open;
    }

    void SameLineIfEnoughWidth(const float nextItemWidth)
    {
        const float spacing = ImGui::GetStyle().ItemSpacing.x;

        if (ImGui::GetContentRegionAvail().x >= nextItemWidth + spacing)
            ImGui::SameLine();
    }

    glm::vec3 NormalizeDirectionOrDefault(const glm::vec3& direction)
    {
        const float lengthSquared = glm::dot(direction, direction);
        if (lengthSquared <= 0.0001f)
            return Quiet::Scene::SceneRenderSettings{}.LightDirection;

        return glm::normalize(direction);
    }

    void NormalizeSceneLightDirection(Quiet::Scene::Scene& scene, Quiet::Editor::EditorCommands& commands)
    {
        const glm::vec3 normalizedDirection = NormalizeDirectionOrDefault(scene.GetRenderSettings().LightDirection);

        commands.BeginInteraction(scene);
        commands.SetSceneLightDirection(scene, normalizedDirection);
        commands.EndInteraction(scene);
    }

    void ResetSceneDirectionalLighting(Quiet::Scene::Scene& scene, Quiet::Editor::EditorCommands& commands)
    {
        const Quiet::Scene::SceneRenderSettings defaults{};

        commands.BeginInteraction(scene);
        commands.SetSceneLightDirection(scene, defaults.LightDirection);
        commands.SetSceneLightIntensity(scene, defaults.LightIntensity);
        commands.EndInteraction(scene);
    }

    void ResetSceneAmbient(Quiet::Scene::Scene& scene, Quiet::Editor::EditorCommands& commands)
    {
        const Quiet::Scene::SceneRenderSettings defaults{};

        commands.BeginInteraction(scene);
        commands.SetSceneAmbientColor(scene, defaults.AmbientColor);
        commands.SetSceneAmbientIntensity(scene, defaults.AmbientIntensity);
        commands.EndInteraction(scene);
    }

    void ResetSceneLighting(Quiet::Scene::Scene& scene, Quiet::Editor::EditorCommands& commands)
    {
        const Quiet::Scene::SceneRenderSettings defaults{};

        commands.BeginInteraction(scene);

        commands.SetSceneLightDirection(scene, defaults.LightDirection);
        commands.SetSceneLightIntensity(scene, defaults.LightIntensity);
        commands.SetSceneAmbientColor(scene, defaults.AmbientColor);
        commands.SetSceneAmbientIntensity(scene, defaults.AmbientIntensity);

        commands.EndInteraction(scene);
    }
}

namespace Quiet::Renderer::UI
{
    void ImGuiLayer::BuildSceneSettings(Scene::Scene &scene, Editor::EditorCommands &editorCommands, Editor::EditorState &editorState)
    {
        ImGui::Begin("Scene Settings");
        {
            const bool isEditMode = editorState.Scene.Mode == Editor::SceneMode::Edit;
            if (!isEditMode)
            {
                DrawSceneSettingsBanner("Simulating runtime copy. Scene settings editing is disabled.");
                ImGui::Spacing();
                ImGui::BeginDisabled();
            }

            DrawPanelIntro();
            ImGui::Spacing();

            const TrackedSceneSettingsInteraction tracked{
                scene,
                editorCommands,
                isEditMode
            };

            Scene::SceneRenderSettings& settings = scene.GetRenderSettings();

            if (BeginSettingsCard("Lighting"))
            {
                ImGui::PushID("SceneSettingsLighting");

                glm::vec3 lightDirection = settings.LightDirection;
                if (DrawVec3Row("Direction", lightDirection, 0.01f, -1.0f, 1.0f, tracked))
                    editorCommands.SetSceneLightDirection(scene, lightDirection);

                float lightIntensity = settings.LightIntensity;
                if (DrawFloatRow("Intensity", lightIntensity, 0.01f, 0.0f, 10.0f, tracked))
                    editorCommands.SetSceneLightIntensity(scene, lightIntensity);

                ImGui::Spacing();

                const float normalizeButtonWidth = SZ::Px(172.0f);
                const float resetLightingButtonWidth = SZ::Px(132.0f);

                if (QUI::EditorButton("Normalize Direction", ImVec2(normalizeButtonWidth, 0.0f), isEditMode))
                    NormalizeSceneLightDirection(scene, editorCommands);

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayShort))
                    ImGui::SetTooltip("Normalize the scene light direction vector.");

                SameLineIfEnoughWidth(resetLightingButtonWidth);

                if (QUI::EditorButton("Reset Lighting", ImVec2(resetLightingButtonWidth, 0.0f), isEditMode))
                    ResetSceneDirectionalLighting(scene, editorCommands);

                ImGui::PopID();
            }

            ImGui::Spacing();

            if (BeginSettingsCard("Ambient"))
            {
                ImGui::PushID("SceneSettingsAmbient");

                glm::vec3 ambientColor = settings.AmbientColor;
                if (DrawColor3Row("Color", ambientColor, tracked))
                    editorCommands.SetSceneAmbientColor(scene, ambientColor);

                float ambientIntensity = settings.AmbientIntensity;
                if (DrawFloatRow("Intensity", ambientIntensity, 0.01f, 0.0f, 5.0f, tracked))
                    editorCommands.SetSceneAmbientIntensity(scene, ambientIntensity);

                ImGui::Spacing();

                if (QUI::EditorButton("Reset Ambient", SZ::Px(ImVec2(132.0f, 0.0f)), isEditMode))
                    ResetSceneAmbient(scene, editorCommands);

                ImGui::PopID();
            }

            ImGui::Separator();

            if (QUI::EditorButton("Reset All", SZ::Px(ImVec2(132.0f, 0.0f)), isEditMode))
                ResetSceneLighting(scene, editorCommands);

            if (!isEditMode) ImGui::EndDisabled();
        }
        ImGui::End();
    }
}
