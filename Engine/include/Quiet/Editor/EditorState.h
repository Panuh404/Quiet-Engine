#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <utility>
#include <unordered_map>

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

    enum class ViewportNavigationMode : std::uint8_t
    {
        None,
        Fly,
        Orbit,
        Pan
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
        CreatePrimitiveActor,
        ReloadAssets, ReimportAsset
    };

    struct EditorRequest
    {
        EditorRequestType Type{};
        std::string Argument{};
    };

    enum class EditorFeedbackSeverity
    {
        Info,
        Success,
        Warning,
        Error
    };

    struct EditorFeedbackMessage
    {
        bool Visible = false;
        EditorFeedbackSeverity Severity = EditorFeedbackSeverity::Info;
        std::string Title{};
        std::string Message{};
        std::uint64_t Revision = 0;

        void Clear()
        {
            Visible = false;
            Severity = EditorFeedbackSeverity::Info;
            Title.clear();
            Message.clear();
        }

        void Set(EditorFeedbackSeverity severity, std::string title, std::string message)
        {
            ++Revision;

            Visible = true;
            Severity = severity;
            Title = std::move(title);
            Message = std::move(message);
        }
    };

    struct EditorSelectionState
    {
        std::optional<Core::UUID> SelectedActorID;
        std::optional<Core::UUID> HoveredActorID;
        std::optional<Core::UUID> PilotedCameraActorID;
    };

    struct EditorViewportAssetDropRequest
    {
        std::string AssetID{};

        float ScreenX = 0.0f;
        float ScreenY = 0.0f;

        [[nodiscard]] bool IsValid() const
        {
            return !AssetID.empty();
        }
    };

    struct EditorViewportState
    {
        std::uint32_t Width = 0;
        std::uint32_t Height = 0;

        bool Focused = false;
        bool Hovered = false;
        bool ImageHovered = false;

        bool ShowGrid = true;
        bool ShowDebugIcons = true;
        bool ShowPhysicsColliders = false;
        bool ShowSelectionFeedback = true;
        bool ShowHoverFeedback = true;
        bool ShowNavigationHints = true;

        float CameraMoveSpeed = 2.5f;

        ViewportNavigationMode NavigationCaptureRequested = ViewportNavigationMode::None;
        ViewportNavigationMode NavigationMode = ViewportNavigationMode::None;
        bool FlySpeedBoostActive = false;

        float ScreenX = 0.0f;
        float ScreenY = 0.0f;

        std::optional<EditorViewportAssetDropRequest> ActiveMeshAssetDrop{};
        std::optional<EditorViewportAssetDropRequest> PendingMeshAssetDrop{};
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

    struct EditorScriptingState
    {
        std::unordered_map<std::string, std::string> SourceErrors{};
    };

    struct EditorProjectManagerState
    {
        bool RequestOpenPopup = false;

        std::array<char, 260> ProjectToOpenBuffer{};
        std::array<char, 128> NewProjectNameBuffer{};
        std::array<char, 260> ProjectScenePathBuffer{};
    };

    struct EditorPanelFocusState
    {
        bool AssetBrowser = false;
    };

    struct EditorState
    {
        EditorSelectionState Selection{};
        EditorViewportState Viewport{};
        EditorGizmoState Gizmo{};
        EditorSceneState Scene{};
        EditorHistoryState History{};
        EditorScriptingState Scripting{};
        EditorProjectManagerState ProjectManager{};
        EditorPanelFocusState PanelFocus{};
        EditorFeedbackMessage AssetOperationFeedback{};

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
