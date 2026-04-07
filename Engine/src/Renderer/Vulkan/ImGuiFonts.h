#pragma once

#include "imgui.h"

#include <filesystem>
#include <initializer_list>
#include <string>

namespace Quiet::Renderer::Vulkan
{
    struct EditorFonts
    {
        ImFont* UI = nullptr;
        ImFont* Bold = nullptr;
        ImFont* Mono = nullptr;
    };

    inline std::filesystem::path FindFirstExistingFontPath(
        std::initializer_list<std::filesystem::path> candidates)
    {
        for (const auto& candidate : candidates)
        {
            if (candidate.empty()) continue;
            if (std::filesystem::exists(candidate)) return candidate;
            if (!candidate.is_absolute())
            {
                const auto fromCurrentPath = std::filesystem::current_path() / candidate;
                if (std::filesystem::exists(fromCurrentPath)) return fromCurrentPath;
            }
        }
        return {};
    }

    inline ImFont* AddFontSafe(ImGuiIO& io,
                               const std::filesystem::path& path,
                               float sizePixels,
                               const ImFontConfig& config)
    {
        if (!path.empty() && std::filesystem::exists(path))
        {
            if (ImFont* font = io.Fonts->AddFontFromFileTTF(
                    path.string().c_str(),
                    sizePixels,
                    &config))
            {
                return font;
            }
        }

        return nullptr;
    }

    inline EditorFonts SetupEditorFonts(ImGuiIO& io,
                                        float uiSize = 28.0f,
                                        float boldSize = 28.0f,
                                        float monoSize = 24.0f)
    {
        io.Fonts->Clear();

        ImFontConfig uiConfig{};
        uiConfig.OversampleH = 2;
        uiConfig.OversampleV = 2;
        uiConfig.PixelSnapH = false;
        uiConfig.RasterizerMultiply = 1.05f;

        ImFontConfig boldConfig = uiConfig;
        ImFontConfig monoConfig = uiConfig;

        const std::filesystem::path uiFontPath = FindFirstExistingFontPath({
            "Assets/Fonts/Inter-Regular.ttf",
            "Assets/Fonts/Inter-Medium.ttf",
            "Assets/Fonts/SegoeUI.ttf",
            "C:/Windows/Fonts/segoeui.ttf"
        });

        const std::filesystem::path boldFontPath = FindFirstExistingFontPath({
            "Assets/Fonts/Inter-SemiBold.ttf",
            "Assets/Fonts/Inter-Bold.ttf",
            "Assets/Fonts/SegoeUI-Bold.ttf",
            "C:/Windows/Fonts/segoeuib.ttf"
        });

        const std::filesystem::path monoFontPath = FindFirstExistingFontPath({
            "Assets/Fonts/JetBrainsMono-Regular.ttf",
            "Assets/Fonts/CascadiaMono.ttf",
            "C:/Windows/Fonts/consola.ttf"
        });

        EditorFonts fonts{};
        fonts.UI = AddFontSafe(io, uiFontPath, uiSize, uiConfig);
        fonts.Bold = AddFontSafe(io, boldFontPath, boldSize, boldConfig);
        fonts.Mono = AddFontSafe(io, monoFontPath, monoSize, monoConfig);

        if (!fonts.UI)
        {
            fonts.UI = io.Fonts->AddFontDefault();
        }

        if (!fonts.Bold)
        {
            fonts.Bold = fonts.UI;
        }

        if (!fonts.Mono)
        {
            fonts.Mono = fonts.UI;
        }

        io.FontDefault = fonts.UI;
        io.FontGlobalScale = 1.0f;

        return fonts;
    }
}
