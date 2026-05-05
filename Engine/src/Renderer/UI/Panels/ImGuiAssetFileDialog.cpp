#include "ImGuiAssetFileDialog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>

#include <imgui.h>

#include "ImGuiFileDialog.h"
#include "Quiet/Project/ProjectContext.h"

namespace
{
    constexpr const char* kAssetFileDialogKey = "Quiet_SelectAssetFile";
    constexpr const char* kSupportedAssetFilters =
        "All Supported" "{.obj,.fbx,.gltf,.glb,.dae,.png,.jpg,.jpeg,.tga,.bmp},"
        "Meshes (*.obj *.fbx *.gltf *.glb *.dae)" "{.obj,.fbx,.gltf,.glb,.dae},"
        "Textures (*.png *.jpg *.jpeg *.tga *.bmp)" "{.png,.jpg,.jpeg,.tga,.bmp}";

    std::string ExtractAssetIDFromPath(const std::string& path)
    {
        return std::filesystem::path(path).stem().string();
    }

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

     ImVec2 CalculateAssetDialogSize()
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();

        const ImVec2 workSize = viewport
            ? viewport->WorkSize
            : ImGui::GetIO().DisplaySize;

        const float width = std::clamp(workSize.x * 0.78f, 820.0f, 1500.0f);
        const float height = std::clamp(workSize.y * 0.78f, 560.0f, 960.0f);

        return { width, height };
    }

    void ConfigureAssetFileDialogStyles()
    {
        auto* dialog = ImGuiFileDialog::Instance();

        dialog->ClearFilesStyle();
        dialog->SetFileStyle(IGFD_FileStyleByTypeDir, "", ImVec4(0.45f, 0.78f, 1.00f, 1.0f), "");

        const ImVec4 meshColor{ 0.42f, 0.88f, 0.58f, 1.0f };
        const ImVec4 textureColor{ 0.90f, 0.72f, 1.00f, 1.0f };

        dialog->SetFileStyle(IGFD_FileStyleByExtention, ".obj",  meshColor, "");
        dialog->SetFileStyle(IGFD_FileStyleByExtention, ".fbx",  meshColor, "");
        dialog->SetFileStyle(IGFD_FileStyleByExtention, ".gltf", meshColor, "");
        dialog->SetFileStyle(IGFD_FileStyleByExtention, ".glb",  meshColor, "");
        dialog->SetFileStyle(IGFD_FileStyleByExtention, ".dae",  meshColor, "");

        dialog->SetFileStyle(IGFD_FileStyleByExtention, ".png",  textureColor, "");
        dialog->SetFileStyle(IGFD_FileStyleByExtention, ".jpg",  textureColor, "");
        dialog->SetFileStyle(IGFD_FileStyleByExtention, ".jpeg", textureColor, "");
        dialog->SetFileStyle(IGFD_FileStyleByExtention, ".tga",  textureColor, "");
        dialog->SetFileStyle(IGFD_FileStyleByExtention, ".bmp",  textureColor, "");
    }

    void PushAssetDialogStyle()
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 8.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 15.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.0f, 6.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));

        ImGui::PushStyleColor(ImGuiCol_WindowBg,        ImVec4(0.075f, 0.082f, 0.100f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg,         ImVec4(0.075f, 0.082f, 0.100f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg,         ImVec4(0.095f, 0.105f, 0.125f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border,          ImVec4(0.22f, 0.25f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBg,         ImVec4(0.13f, 0.145f, 0.17f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,  ImVec4(0.18f, 0.20f, 0.24f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,   ImVec4(0.22f, 0.25f, 0.30f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Header,          ImVec4(0.18f, 0.24f, 0.33f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,   ImVec4(0.24f, 0.32f, 0.44f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,    ImVec4(0.30f, 0.40f, 0.55f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Button,          ImVec4(0.16f, 0.20f, 0.27f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,   ImVec4(0.24f, 0.31f, 0.42f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,    ImVec4(0.30f, 0.40f, 0.55f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,     ImVec4(0.055f, 0.060f, 0.075f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,   ImVec4(0.32f, 0.38f, 0.48f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, ImVec4(0.42f, 0.50f, 0.62f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,  ImVec4(0.52f, 0.62f, 0.76f, 1.0f));
    }

    void PopAssetDialogStyle()
    {
        ImGui::PopStyleColor(17);
        ImGui::PopStyleVar(8);
    }
}

namespace Quiet::Renderer::UI
{
    Project::AssetType DetectImportAssetType(const std::filesystem::path &path)
    {
        const std::string ext = ToLowerCopy(path.extension().string());

        if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".dae")
            return Project::AssetType::Mesh;

        if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp")
            return Project::AssetType::Texture;

        return Project::AssetType::Unknown;
    }

    void OpenAssetFileDialog(const Project::ProjectContext& projectContext)
    {
        ConfigureAssetFileDialogStyles();

        IGFD::FileDialogConfig config;
        config.path = projectContext.GetAssetsPath().string();
        config.countSelectionMax = 1;
        config.flags = ImGuiFileDialogFlags_Modal;


        ImGuiFileDialog::Instance()->OpenDialog(
            kAssetFileDialogKey,
            "Import Asset",
            kSupportedAssetFilters,
            config
        );
    }

    std::optional<AssetFileDialogResult> DrawAssetFileDialog(const Project::ProjectContext &projectContext)
    {
        const ImVec2 dialogSize = CalculateAssetDialogSize();
        const ImVec2 minSize{
            std::min(760.0f, dialogSize.x),
            std::min(500.0f, dialogSize.y)
        };

        ImGui::SetNextWindowSize(dialogSize, ImGuiCond_Appearing);
        PushAssetDialogStyle();

        const bool dialogOpen = ImGuiFileDialog::Instance()->Display(
            kAssetFileDialogKey,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking,
            minSize,
            dialogSize
        );

        PopAssetDialogStyle();
        if (!dialogOpen) return std::nullopt;

        std::optional<AssetFileDialogResult> result;
        if (ImGuiFileDialog::Instance()->IsOk())
        {
            const std::string selectedPath = ImGuiFileDialog::Instance()->GetFilePathName();
            const std::filesystem::path selectedFile(selectedPath);
            const Project::AssetType type = DetectImportAssetType(selectedFile);

            if (type != Project::AssetType::Unknown)
            {
                result = AssetFileDialogResult{
                    type,
                    ExtractAssetIDFromPath(selectedPath),
                    projectContext.NormalizeAssetPath(selectedPath).generic_string()
                };
            }
        }

        ImGuiFileDialog::Instance()->Close();
        return result;
    }
}