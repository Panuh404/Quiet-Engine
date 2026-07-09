#include "Renderer/UI/Assets/ImGuiAssetFileDialog.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <optional>
#include <string>

#include <imgui.h>

#include "ImGuiFileDialog.h"
#include "Quiet/Project/ProjectContext.h"
#include "Renderer/UI/ImGuiTheme.h"

namespace Theme = Quiet::Renderer::UI::EditorColor;

namespace
{
    namespace SZ = Quiet::Renderer::UI::EditorSizing;
    constexpr const char* kAssetFileDialogKey = "Quiet_SelectAssetFile";
    constexpr const char* kSupportedAssetFilters =
        "All Supported" "{.obj,.fbx,.gltf,.glb,.dae,.png,.jpg,.jpeg,.tga,.bmp},"
        "Meshes (*.obj *.fbx *.gltf *.glb *.dae)" "{.obj,.fbx,.gltf,.glb,.dae},"
        "Textures (*.png *.jpg *.jpeg *.tga *.bmp)" "{.png,.jpg,.jpeg,.tga,.bmp}";

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

    void ConfigureAssetFileDialogStyles()
    {
        auto* dialog = ImGuiFileDialog::Instance();

        dialog->ClearFilesStyle();

        const ImVec4 folderColor  = ImVec4(0.34f, 0.58f, 0.92f, 1.0f);
        const ImVec4 meshColor    = ImVec4(0.35f, 0.78f, 0.50f, 1.0f);
        const ImVec4 textureColor = ImVec4(0.72f, 0.58f, 0.96f, 1.0f);

        dialog->SetFileStyle(IGFD_FileStyleByTypeDir, "", folderColor, "");

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
        // Calculate Dialog Size
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        const ImVec2 workSize = viewport
            ? viewport->WorkSize
            : ImGui::GetIO().DisplaySize;

        const float width = std::clamp(
            workSize.x * 0.78f,
            SZ::Px(820.0f),
            SZ::Px(1500.0f)
        );

        const float height = std::clamp(
            workSize.y * 0.78f,
            SZ::Px(560.0f),
            SZ::Px(960.0f)
        );

        const ImVec2 dialogSize = { width, height };
        const ImVec2 minSize{
            std::min(SZ::Px(760.0f), dialogSize.x),
            std::min(SZ::Px(500.0f), dialogSize.y)
        };

        ImGui::SetNextWindowSize(dialogSize, ImGuiCond_Appearing);

        // Push Asset Style
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,   SZ::Px(14.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,    SZ::Px(10.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding,    SZ::Px(7.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding,SZ::Px(8.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,    SZ::Px(14.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,    SZ::Px(ImVec2(14.0f, 12.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,     SZ::Px(ImVec2(10.0f, 7.0f)));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,      SZ::Px(ImVec2(9.0f, 8.0f)));

        ImGui::PushStyleColor(ImGuiCol_WindowBg,               Theme::Background1);
        ImGui::PushStyleColor(ImGuiCol_PopupBg,                Theme::Background1);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,                Theme::Background2);
        ImGui::PushStyleColor(ImGuiCol_Border,                 Theme::Border);
        ImGui::PushStyleColor(ImGuiCol_FrameBg,                Theme::Background3);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered,         Theme::Background4);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive,          Theme::AccentDim);
        ImGui::PushStyleColor(ImGuiCol_Header,                 Theme::Background3);
        ImGui::PushStyleColor(ImGuiCol_HeaderHovered,          Theme::AccentDim);
        ImGui::PushStyleColor(ImGuiCol_HeaderActive,           Theme::AccentSoft);
        ImGui::PushStyleColor(ImGuiCol_Button,                 Theme::Background3);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,          Theme::Background4);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,           Theme::AccentSoft);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarBg,            Theme::Background0);
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab,          ImVec4(0.090f, 0.105f, 0.135f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered,   ImVec4(0.140f, 0.170f, 0.220f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive,    Theme::AccentSoft);

        const bool dialogOpen = ImGuiFileDialog::Instance()->Display(
            kAssetFileDialogKey,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking,
            minSize,
            dialogSize
        );

        // Pop Asset Style
        ImGui::PopStyleColor(17);
        ImGui::PopStyleVar(8);

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
                    std::filesystem::path(selectedPath).stem().string(),
                    projectContext.NormalizeAssetPath(selectedPath).generic_string()
                };
            }
        }

        ImGuiFileDialog::Instance()->Close();
        return result;
    }
}