#pragma once

#include <string>
#include <vector>

#include "Quiet/Core/UUID.h"

namespace Quiet::Scene
{
    enum class SceneHierarchyNodeType
    {
        Actor,
        Folder
    };

    struct SceneHierarchyNode
    {
        SceneHierarchyNodeType Type = SceneHierarchyNodeType::Actor;
        Core::UUID ID{};
    };

    struct SceneHierarchyFolder
    {
        Core::UUID ID{};
        std::string Name = "Folder";
        bool Expanded = true;
        std::vector<SceneHierarchyNode> Children;
    };

    struct SceneHierarchyState
    {
        std::vector<SceneHierarchyNode> RootNodes;
        std::vector<SceneHierarchyFolder> Folders;
    };

    [[nodiscard]] inline bool operator==(const SceneHierarchyNode& lhs, const SceneHierarchyNode& rhs)
    {
        return lhs.Type == rhs.Type &&
               lhs.ID == rhs.ID;
    }

    [[nodiscard]] inline bool operator!=(const SceneHierarchyNode& lhs, const SceneHierarchyNode& rhs)
    {
        return !(lhs == rhs);
    }

    [[nodiscard]] inline bool operator==(const SceneHierarchyFolder& lhs, const SceneHierarchyFolder& rhs)
    {
        return lhs.ID == rhs.ID &&
               lhs.Name == rhs.Name &&
               lhs.Children == rhs.Children;
    }

    [[nodiscard]] inline bool operator!=(const SceneHierarchyFolder& lhs, const SceneHierarchyFolder& rhs)
    {
        return !(lhs == rhs);
    }

    [[nodiscard]] inline bool operator==(const SceneHierarchyState& lhs, const SceneHierarchyState& rhs)
    {
        return lhs.RootNodes == rhs.RootNodes &&
               lhs.Folders == rhs.Folders;
    }

    [[nodiscard]] inline bool operator!=(const SceneHierarchyState& lhs, const SceneHierarchyState& rhs)
    {
        return !(lhs == rhs);
    }
}