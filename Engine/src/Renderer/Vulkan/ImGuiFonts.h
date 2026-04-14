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
        ImFont* Icons = nullptr;
        bool HasIcons = false;
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

    inline ImFont* AddIconFontSafe(ImGuiIO& io,
                                   const std::filesystem::path& path,
                                   float sizePixels)
    {
        if (path.empty() || !std::filesystem::exists(path))
        {
            return nullptr;
        }

        static const ImWchar iconRanges[] = { 0xf000, 0xf8ff, 0 };

        ImFontConfig iconConfig{};
        iconConfig.PixelSnapH = true;
        iconConfig.GlyphMinAdvanceX = sizePixels * 0.90f;
        iconConfig.GlyphOffset = ImVec2(0.0f, 1.0f);

        return io.Fonts->AddFontFromFileTTF(
            path.string().c_str(),
            sizePixels,
            &iconConfig,
            iconRanges
        );
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
            "Resources/Fonts/Inter-Regular.ttf",
            "Resources/Fonts/Inter-Medium.ttf",
            "C:/Windows/Fonts/segoeui.ttf"
        });

        const std::filesystem::path boldFontPath = FindFirstExistingFontPath({
            "Resources/Fonts/Inter-SemiBold.ttf",
            "Resources/Fonts/Inter-Bold.ttf",
            "C:/Windows/Fonts/segoeuib.ttf"
        });

        const std::filesystem::path monoFontPath = FindFirstExistingFontPath({
            "Resources/Fonts/JetBrainsMono-Regular.ttf",
            "C:/Windows/Fonts/consola.ttf"
        });

        const std::filesystem::path iconFontPath = FindFirstExistingFontPath({
            "resources/Fonts/fa-solid-900.ttf"
        });

        EditorFonts fonts{};

        fonts.UI = AddFontSafe(io, uiFontPath, uiSize, uiConfig);
        if (!fonts.UI)
        {
            fonts.UI = io.Fonts->AddFontDefault();
        }

        fonts.Bold = AddFontSafe(io, boldFontPath, boldSize, boldConfig);
        if (!fonts.Bold)
        {
            fonts.Bold = fonts.UI;
        }

        fonts.Mono = AddFontSafe(io, monoFontPath, monoSize, monoConfig);
        if (!fonts.Mono)
        {
            fonts.Mono = fonts.UI;
        }

        fonts.Icons = AddIconFontSafe(io, iconFontPath, uiSize);
        fonts.HasIcons = (fonts.Icons != nullptr);

        io.FontDefault = fonts.UI;
        io.FontGlobalScale = 1.0f;

        return fonts;
    }
}