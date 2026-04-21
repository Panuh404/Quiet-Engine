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
    };

    struct EditorRequest
    {
        EditorRequestType Type{};
        std::string Argument{};
    };

    struct EditorState
    {
        std::optional<Core::UUID> SelectedActorID;
        std::optional<Core::UUID> PilotedCameraActorID;

        std::uint32_t ViewportWidth = 0;
        std::uint32_t ViewportHeight = 0;

        bool ViewportFocused = false;
        bool ViewportHovered = false;
        bool ViewportImageHovered = false;

        float ViewportScreenX = 0.0f;
        float ViewportScreenY = 0.0f;

        GizmoOperation CurrentGizmoOperation = GizmoOperation::Translate;
        GizmoMode CurrentGizmoMode = GizmoMode::World;
        SceneMode CurrentSceneMode = SceneMode::Edit;

        bool GizmoSnapEnabled = false;
        float TranslateSnap[3] = { 0.5f, 0.5f, 0.5f };
        float RotateSnap = 15.0f;
        float ScaleSnap = 0.1f;

        bool RequestProjectManagerPopup = false; // Local UI trigger

        bool CanUndo = false;
        bool CanRedo = false;
        bool SceneDirty = false;
        std::string SceneDisplayName = "DefaultScene.json";

        std::array<char, 260> ProjectToOpenBuffer{};
        std::array<char, 128> NewProjectNameBuffer{};
        std::array<char, 260> ProjectScenePathBuffer{};

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
