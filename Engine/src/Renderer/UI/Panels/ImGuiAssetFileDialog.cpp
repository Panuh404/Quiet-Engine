#include "ImGuiAssetFileDialog.h"

#include <filesystem>
#include <optional>
#include <string>

#include <imgui.h>

#include "ImGuiFileDialog.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/ImGuiLayer.h"

namespace
{
    constexpr const char* kMeshFileDialogKey = "Quiet_SelectMeshAssetFile";

    std::string ExtractAssetIDFromPath(const std::string& path)
    {
        return std::filesystem::path(path).stem().string();
    }

    bool IsSupportedMeshExtension(const std::filesystem::path& path)
    {
        const std::string ext = path.extension().string();
        return ext == ".obj" ||
               ext == ".fbx" ||
               ext == ".gltf" ||
               ext == ".glb" ||
               ext == ".dae";
    }

    void ConfigureMeshFileDialogStyles()
    {
        auto* dialog = ImGuiFileDialog::Instance();

        dialog->SetFileStyle(
            IGFD_FileStyleByTypeDir,
            "",
            ImVec4(0.45f, 0.78f, 1.00f, 1.0f),
            "[DIR]"
        );

        dialog->SetFileStyle(
            IGFD_FileStyleByExtention,
            ".obj",
            ImVec4(0.35f, 0.90f, 0.55f, 1.0f),
            "[OBJ]"
        );

        dialog->SetFileStyle(
            IGFD_FileStyleByExtention,
            ".fbx",
            ImVec4(0.95f, 0.72f, 0.35f, 1.0f),
            "[FBX]"
        );

        dialog->SetFileStyle(
            IGFD_FileStyleByExtention,
            ".gltf",
            ImVec4(0.70f, 0.62f, 1.00f, 1.0f),
            "[GLTF]"
        );

        dialog->SetFileStyle(
            IGFD_FileStyleByExtention,
            ".glb",
            ImVec4(0.70f, 0.62f, 1.00f, 1.0f),
            "[GLB]"
        );

        dialog->SetFileStyle(
            IGFD_FileStyleByExtention,
            ".dae",
            ImVec4(0.98f, 0.55f, 0.55f, 1.0f),
            "[DAE]"
        );
    }
}

namespace Quiet::Renderer::UI
{
    void OpenMeshAssetFileDialog(const Project::ProjectContext& projectContext)
    {
        ConfigureMeshFileDialogStyles();

        IGFD::FileDialogConfig config;
        config.path = projectContext.GetAssetsPath().string();
        config.countSelectionMax = 1;
        config.flags = ImGuiFileDialogFlags_Modal;
        config.sidePane = nullptr;
        config.sidePaneWidth = 0.0f;

        ImGuiFileDialog::Instance()->OpenDialog(
            kMeshFileDialogKey,
            "Select Mesh Asset",
            "Mesh files{.obj,.fbx,.gltf,.glb,.dae}",
            config
        );
    }

    std::optional<MeshAssetFileDialogResult> DrawMeshAssetFileDialog(
        const Project::ProjectContext& projectContext)
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));

        const bool dialogOpen = ImGuiFileDialog::Instance()->Display(
            kMeshFileDialogKey,
            ImGuiWindowFlags_NoCollapse,
            ImVec2(900.0f, 560.0f),
            ImVec2(1400.0f, 900.0f)
        );

        ImGui::PopStyleVar(5);

        if (!dialogOpen)
        {
            return std::nullopt;
        }

        std::optional<MeshAssetFileDialogResult> result;

        if (ImGuiFileDialog::Instance()->IsOk())
        {
            const std::string selectedPath =
                ImGuiFileDialog::Instance()->GetFilePathName();

            const std::filesystem::path selectedFile(selectedPath);

            if (IsSupportedMeshExtension(selectedFile))
            {
                result = MeshAssetFileDialogResult{
                    ExtractAssetIDFromPath(selectedPath),
                    projectContext.NormalizeAssetPath(selectedPath).generic_string()
                };
            }
        }

        ImGuiFileDialog::Instance()->Close();
        return result;
    }
}