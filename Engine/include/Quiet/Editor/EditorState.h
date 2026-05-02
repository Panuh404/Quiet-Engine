#pragma once

#include <array>
#include <cstddef>
#include <deque>
#include <optional>
#include <string>

#include "Quiet/Core/UUID.h"

namespace Quiet::Editor
{
    enum class GizmoOperation
    {
        Translate,
        Rotate,
        Scale
    };

    enum class GizmoMode
    {
        Local,
        World
    };

    enum class SceneMode
    {
        Edit,
        Simulate,
        Play
    };

    enum class EditorRequestType
    {
        FocusSelected,
        NewScene, SaveScene, LoadScene,
        EnterSimulate, StopSimulate,
        EnterPlay, StopPlay,
        Undo, Redo,
        OpenProject, CreateProject,
        OpenProjectScene, SetStartupScene,
        ReloadAssets
    };

    struct EditorRequest
    {
        EditorRequestType Type{};
        std::string Argument{};
    };

    struct EditorSelectionState
    {
        std::optional<Core::UUID> SelectedActorID;
        std::optional<Core::UUID> PilotedCameraActorID;
    };

    struct EditorViewportState
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;

        bool Focused = false;
        bool Hovered = false;
        bool ImageHovered = false;

        bool ShowDebugIcons = true;

        float ScreenX = 0.0f;
        float ScreenY = 0.0f;
    };

    struct EditorGizmoState
    {
        GizmoOperation Operation = GizmoOperation::Translate;
        GizmoMode Mode = GizmoMode::World;

        bool SnapEnabled = false;
        float TranslateSnap[3] = { 0.5f, 0.5f, 0.5f };
        float RotateSnap = 15.0f;
        float ScaleSnap = 0.1f;
    };

    struct EditorSceneState
    {
        SceneMode Mode = SceneMode::Edit;
        bool Dirty = false;
        std::string DisplayName = "DefaultScene.json";
    };

    struct EditorHistoryState
    {
        bool CanUndo = false;
        bool CanRedo = false;
    };

    struct EditorProjectManagerState
    {
        bool RequestOpenPopup = false;

        std::array<char, 260> ProjectToOpenBuffer{};
        std::array<char, 128> NewProjectNameBuffer{};
        std::array<char, 260> ProjectScenePathBuffer{};
    };

    struct EditorState
    {
        EditorSelectionState Selection{};
        EditorViewportState Viewport{};
        EditorGizmoState Gizmo{};
        EditorSceneState Scene{};
        EditorHistoryState History{};
        EditorProjectManagerState ProjectManager{};

        std::deque<EditorRequest> PendingRequests{};

        void QueueRequest(EditorRequestType type)
        {
            PendingRequests.push_back(EditorRequest{ type });
        }

        void QueueRequest(EditorRequestType type, std::string argument)
        {
            PendingRequests.push_back(EditorRequest{ type, std::move(argument) });
        }

        [[nodiscard]] bool HasPendingRequests() const
        {
            return !PendingRequests.empty();
        }

        [[nodiscard]] std::optional<EditorRequest> PopNextRequest()
        {
            if (PendingRequests.empty()) return std::nullopt;

            EditorRequest request = std::move(PendingRequests.front());
            PendingRequests.pop_front();
            return request;
        }
    };
}
