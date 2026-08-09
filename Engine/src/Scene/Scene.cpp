#include "Scene.h"

#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "Scene/Camera/CameraPilotUtils.h"
#include "Camera/CameraController.h"
#include "Quiet/Core/Log.h"


namespace Quiet::Scene
{
    Scene::Scene()
    {
        QUIET_CORE_INFO("Initialized scene");
    }

    Actor& Scene::CreateActor(const std::string &name)
    {
        Actor actor{};
        actor.ID = Core::UUIDGenerator::New();
        actor.Name = name;

        m_Actors.push_back(std::move(actor));
        AddActorToHierarchyRoot(m_Actors.back().ID);
        return m_Actors.back();
    }

    Actor& Scene::CreateActorWithID(Core::UUID id, const std::string& name)
    {
        Actor actor{};
        actor.ID = id;
        actor.Name = name;
        
        m_Actors.push_back(std::move(actor));
        AddActorToHierarchyRoot(m_Actors.back().ID);
        return m_Actors.back();
    }

    Actor& Scene::DuplicateActor(Core::UUID sourceID)
    {
        Actor* duplicate = TryDuplicateActor(sourceID);
        if (!duplicate)
        {
            QUIET_CORE_ERROR("Failed to duplicate actor. Actor not found.");
            throw std::runtime_error("Scene::DuplicateActor failed. Source actor not found.");
        }

        return *duplicate;
    }

    Actor* Scene::TryDuplicateActor(Core::UUID sourceID)
    {
        const Actor* source = FindActorByID(sourceID);
        if (!source)
        {
            QUIET_CORE_WARN("TryDuplicateActor failed. Source actor not found.");
            return nullptr;
        }

        Actor duplicate = *source;
        duplicate.ID = Quiet::Core::UUIDGenerator::New();
        duplicate.Name = source->Name.empty() ? "Actor Copy" : source->Name + " Copy";

        m_Actors.push_back(std::move(duplicate));
        AddActorToHierarchyRoot(m_Actors.back().ID);
        return &m_Actors.back();
    }

    bool Scene::DestroyActorByID(Core::UUID id)
    {
        const auto it = std::find_if(
            m_Actors.begin(),
            m_Actors.end(),
            [id](const Actor& actor)
            {
                return actor.ID == id;
            }
        );

        if (it == m_Actors.end())
            return false;

        m_Actors.erase(it);
        RemoveActorFromHierarchy(id);
        return true;
    }

    void Scene::Clear()
    {
        m_RenderSettings = SceneRenderSettings{};
        m_Hierarchy = SceneHierarchyState{};
        m_Actors.clear();
        m_RuntimeMode = RuntimeMode::None;
    }

    void Scene::ResetHierarchyFromActors()
    {
        m_Hierarchy.RootNodes.clear();
        m_Hierarchy.RootNodes.reserve(m_Actors.size());
        m_Hierarchy.Folders.clear();

        for (const Actor& actor : m_Actors)
        {
            AddActorToHierarchyRoot(actor.ID);
        }
    }

    void Scene::SetHierarchy(SceneHierarchyState hierarchy)
    {
        m_Hierarchy = std::move(hierarchy);
        SanitizeHierarchy();
    }

    Core::UUID Scene::CreateHierarchyFolder(const std::string &name)
    {
        SceneHierarchyFolder folder{};
        folder.ID = Core::UUIDGenerator::New();
        folder.Name = name.empty() ? "Folder" : name;
        folder.Expanded = true;

        const Core::UUID folderID = folder.ID;

        m_Hierarchy.Folders.push_back(std::move(folder));
        m_Hierarchy.RootNodes.push_back(SceneHierarchyNode{
            SceneHierarchyNodeType::Folder,
            folderID
        });

        return folderID;
    }

    SceneHierarchyFolder * Scene::FindHierarchyFolder(Core::UUID folderID)
    {
        const auto it = std::find_if(
                m_Hierarchy.Folders.begin(),
                m_Hierarchy.Folders.end(),
                [folderID](const SceneHierarchyFolder& folder)
                {
                    return folder.ID == folderID;
                }
            );

        return it != m_Hierarchy.Folders.end() ? &(*it) : nullptr;
    }

    const SceneHierarchyFolder * Scene::FindHierarchyFolder(Core::UUID folderID) const
    {
        const auto it = std::find_if(
                m_Hierarchy.Folders.begin(),
                m_Hierarchy.Folders.end(),
                [folderID](const SceneHierarchyFolder& folder)
                {
                    return folder.ID == folderID;
                }
            );

        return it != m_Hierarchy.Folders.end() ? &(*it) : nullptr;
    }

    bool Scene::RenameHierarchyFolder(Core::UUID folderID, const std::string &newName)
    {
        SceneHierarchyFolder* folder = FindHierarchyFolder(folderID);
        if (!folder) return false;

        folder->Name = newName.empty() ? "Folder" : newName;
        return true;
    }

    bool Scene::DeleteEmptyHierarchyFolder(Core::UUID folderID)
    {
        const auto folderIt = std::find_if(
        m_Hierarchy.Folders.begin(),
        m_Hierarchy.Folders.end(),
        [folderID](const SceneHierarchyFolder& folder)
            {
                return folder.ID == folderID;
            }
        );

        if (folderIt == m_Hierarchy.Folders.end()) return false;
        if (!folderIt->Children.empty()) return false;
        m_Hierarchy.Folders.erase(folderIt);

        const auto removeFolderNode = [folderID](std::vector<SceneHierarchyNode>& nodes)
        {
            nodes.erase(
                std::remove_if(
                    nodes.begin(),
                    nodes.end(),
                    [folderID](const SceneHierarchyNode& node)
                    {
                        return node.Type == SceneHierarchyNodeType::Folder &&
                               node.ID == folderID;
                    }
                ),
                nodes.end()
            );
        };

        removeFolderNode(m_Hierarchy.RootNodes);
        for (SceneHierarchyFolder& folder : m_Hierarchy.Folders)
        {
            removeFolderNode(folder.Children);
        }

        return true;
    }

    bool Scene::MoveActorToHierarchyFolder(Core::UUID actorID, Core::UUID folderID)
    {
        if (!FindActorByID(actorID)) return false;

        SceneHierarchyFolder* targetFolder = FindHierarchyFolder(folderID);
        if (!targetFolder) return false;

        const bool alreadyInTargetFolder = std::any_of(
            targetFolder->Children.begin(),
            targetFolder->Children.end(),
            [actorID](const SceneHierarchyNode& node)
            {
                return node.Type == SceneHierarchyNodeType::Actor &&
                       node.ID == actorID;
            }
        );

        if (alreadyInTargetFolder) return false;
        RemoveActorFromHierarchy(actorID);

        targetFolder = FindHierarchyFolder(folderID);
        if (!targetFolder) return false;

        targetFolder->Children.push_back(SceneHierarchyNode{
            SceneHierarchyNodeType::Actor,
            actorID
        });

        return true;
    }

    bool Scene::MoveActorToHierarchyRoot(Core::UUID actorID)
    {
        if (!FindActorByID(actorID)) return false;

        const bool alreadyInRoot = std::any_of(
            m_Hierarchy.RootNodes.begin(),
            m_Hierarchy.RootNodes.end(),
            [actorID](const SceneHierarchyNode& node)
            {
                return node.Type == SceneHierarchyNodeType::Actor &&
                       node.ID == actorID;
            }
        );

        if (alreadyInRoot) return false;

        RemoveActorFromHierarchy(actorID);
        AddActorToHierarchyRoot(actorID);

        return true;
    }

    bool Scene::MoveHierarchyRootNode(SceneHierarchyNodeType nodeType, Core::UUID nodeID, std::size_t destinationIndex)
    {
        if (destinationIndex >= m_Hierarchy.RootNodes.size()) return false;

        const auto sourceIt = std::find_if(
        m_Hierarchy.RootNodes.begin(),
        m_Hierarchy.RootNodes.end(),
        [nodeType, nodeID](const SceneHierarchyNode& node)
            {
                return node.Type == nodeType &&
                       node.ID == nodeID;
            }
        );

        if (sourceIt == m_Hierarchy.RootNodes.end()) return false;

        const std::size_t sourceIndex = static_cast<std::size_t>(
            std::distance(m_Hierarchy.RootNodes.begin(), sourceIt)
        );

        if (sourceIndex == destinationIndex) return false;

        SceneHierarchyNode movedNode = *sourceIt;
        m_Hierarchy.RootNodes.erase(sourceIt);

        using DifferenceType = std::vector<SceneHierarchyNode>::difference_type;

        m_Hierarchy.RootNodes.insert(
            m_Hierarchy.RootNodes.begin() +
                static_cast<DifferenceType>(destinationIndex),
            movedNode
        );

        return true;
    }

    void Scene::AddActorToHierarchyRoot(const Core::UUID actorID)
    {
        if (!actorID.IsValid()) return;

        const bool alreadyInRoot = std::any_of(
            m_Hierarchy.RootNodes.begin(),
            m_Hierarchy.RootNodes.end(),
            [actorID](const SceneHierarchyNode& node)
            {
                return node.Type == SceneHierarchyNodeType::Actor &&
                       node.ID == actorID;
            }
        );

        if (alreadyInRoot) return;

        m_Hierarchy.RootNodes.push_back(SceneHierarchyNode{
            SceneHierarchyNodeType::Actor,
            actorID
        });
    }

    void Scene::RemoveActorFromHierarchy(const Core::UUID actorID)
    {
        const auto removeActorNode = [actorID](std::vector<SceneHierarchyNode>& nodes)
        {
            nodes.erase(
                std::remove_if(
                    nodes.begin(),
                    nodes.end(),
                    [actorID](const SceneHierarchyNode& node)
                    {
                        return node.Type == SceneHierarchyNodeType::Actor &&
                               node.ID == actorID;
                    }
                ),
                nodes.end()
            );
        };

        removeActorNode(m_Hierarchy.RootNodes);
        for (SceneHierarchyFolder& folder : m_Hierarchy.Folders)
        {
            removeActorNode(folder.Children);
        }
    }

    void Scene::SanitizeHierarchy()
    {
        std::unordered_set<Core::UUID> validActorIDs;
        validActorIDs.reserve(m_Actors.size());

        for (const Actor& actor : m_Actors)
        {
            if (actor.ID.IsValid())
                validActorIDs.insert(actor.ID);
        }

        std::unordered_set<Core::UUID> validFolderIDs;
        validFolderIDs.reserve(m_Hierarchy.Folders.size());

        std::unordered_set<Core::UUID> placedActorIDs;
        placedActorIDs.reserve(validActorIDs.size());

        std::vector<SceneHierarchyFolder> sanitizedFolders;
        sanitizedFolders.reserve(m_Hierarchy.Folders.size());

        for (SceneHierarchyFolder& folder : m_Hierarchy.Folders)
        {
            if (!folder.ID.IsValid())
            {
                QUIET_CORE_WARN("Skipping hierarchy folder with invalid ID.");
                continue;
            }

            if (validFolderIDs.contains(folder.ID))
            {
                QUIET_CORE_WARN("Skipping duplicate hierarchy folder '{}'.", Core::UUIDToString(folder.ID));
                continue;
            }

            validFolderIDs.insert(folder.ID);

            std::unordered_set<Core::UUID> childActorIDs;
            std::vector<SceneHierarchyNode> sanitizedChildren;
            sanitizedChildren.reserve(folder.Children.size());

            for (const SceneHierarchyNode& childNode : folder.Children)
            {
                if (childNode.Type != SceneHierarchyNodeType::Actor)
                {
                    QUIET_CORE_WARN("Skipping unsupported non-actor child node in hierarchy folder '{}'.", Core::UUIDToString(folder.ID));
                    continue;
                }

                if (!validActorIDs.contains(childNode.ID))
                {
                    QUIET_CORE_WARN("Skipping hierarchy child actor '{}' because the actor does not exist.", Core::UUIDToString(childNode.ID));
                    continue;
                }

                if (childActorIDs.contains(childNode.ID))
                {
                    QUIET_CORE_WARN(
                        "Skipping duplicate actor '{}' inside hierarchy folder '{}'.",
                        Core::UUIDToString(childNode.ID),
                        Core::UUIDToString(folder.ID)
                    );
                    continue;
                }

                if (placedActorIDs.contains(childNode.ID))
                {
                    QUIET_CORE_WARN(
                        "Skipping actor '{}' in hierarchy folder '{}' because it is already placed elsewhere.",
                        Core::UUIDToString(childNode.ID),
                        Core::UUIDToString(folder.ID)
                    );
                    continue;
                }

                childActorIDs.insert(childNode.ID);
                placedActorIDs.insert(childNode.ID);
                sanitizedChildren.push_back(childNode);
            }

            folder.Children = std::move(sanitizedChildren);
            sanitizedFolders.push_back(std::move(folder));
        }

        m_Hierarchy.Folders = std::move(sanitizedFolders);

        validFolderIDs.clear();
        validFolderIDs.reserve(m_Hierarchy.Folders.size());

        for (const SceneHierarchyFolder& folder : m_Hierarchy.Folders)
        {
            validFolderIDs.insert(folder.ID);
        }

        std::unordered_set<Core::UUID> placedFolderIDs;
        placedFolderIDs.reserve(validFolderIDs.size());

        std::vector<SceneHierarchyNode> sanitizedRootNodes;
        sanitizedRootNodes.reserve(m_Hierarchy.RootNodes.size() + m_Actors.size());

        for (const SceneHierarchyNode& rootNode : m_Hierarchy.RootNodes)
        {
            if (rootNode.Type == SceneHierarchyNodeType::Actor)
            {
                if (!validActorIDs.contains(rootNode.ID))
                {
                    QUIET_CORE_WARN(
                        "Skipping hierarchy root actor '{}' because the actor does not exist.",
                        Core::UUIDToString(rootNode.ID)
                    );
                    continue;
                }

                if (placedActorIDs.contains(rootNode.ID))
                {
                    QUIET_CORE_WARN(
                        "Skipping duplicate hierarchy root actor '{}'.",
                        Core::UUIDToString(rootNode.ID)
                    );
                    continue;
                }

                placedActorIDs.insert(rootNode.ID);
                sanitizedRootNodes.push_back(rootNode);
                continue;
            }

            if (rootNode.Type == SceneHierarchyNodeType::Folder)
            {
                if (!validFolderIDs.contains(rootNode.ID))
                {
                    QUIET_CORE_WARN(
                        "Skipping hierarchy root folder '{}' because the folder does not exist.",
                        Core::UUIDToString(rootNode.ID)
                    );
                    continue;
                }

                if (placedFolderIDs.contains(rootNode.ID))
                {
                    QUIET_CORE_WARN(
                        "Skipping duplicate hierarchy root folder '{}'.",
                        Core::UUIDToString(rootNode.ID)
                    );
                    continue;
                }

                placedFolderIDs.insert(rootNode.ID);
                sanitizedRootNodes.push_back(rootNode);
                continue;
            }
        }

        for (const Actor& actor : m_Actors)
        {
            if (!actor.ID.IsValid()) continue;
            if (placedActorIDs.contains(actor.ID)) continue;

            sanitizedRootNodes.push_back(SceneHierarchyNode{
                SceneHierarchyNodeType::Actor,
                actor.ID
            });

            placedActorIDs.insert(actor.ID);

            QUIET_CORE_WARN(
                "Hierarchy metadata was missing actor '{}'. Appended actor to root.",
                Core::UUIDToString(actor.ID)
            );
        }

        m_Hierarchy.RootNodes = std::move(sanitizedRootNodes);
    }

    void Scene::BeginRuntime(RuntimeMode mode)
    {
        if (mode == RuntimeMode::None)
        {
            QUIET_CORE_WARN("Scene::BeginRuntime called with RuntimeMode::None.");
            return;
        }

        if (m_RuntimeMode != RuntimeMode::None)
        {
            QUIET_CORE_WARN("Scene runtime already active. Restarting runtime mode.");
            EndRuntime();
        }
        m_RuntimeMode = mode;
        OnRuntimeBegin(mode);
    }

    void Scene::EndRuntime()
    {
        if (m_RuntimeMode == RuntimeMode::None) return;
        const RuntimeMode previousMode = m_RuntimeMode;
        OnRuntimeEnd(previousMode);
        m_RuntimeMode = RuntimeMode::None;
    }

    void Scene::HandleRuntimeInput(const Input::Input& input, float deltaTime)
    {
        if (!IsRuntimeActive()) return;

        switch (m_RuntimeMode)
        {
            case RuntimeMode::Simulate:
                HandleSimulateInput(input, deltaTime);
                break;

            case RuntimeMode::Play:
                HandlePlayInput(input, deltaTime);
                break;

            case RuntimeMode::None:
            default:
                break;
        }
    }

    void Scene::Update(float deltaTime)
    {
        if (!IsRuntimeActive()) return;
        UpdateRuntime(deltaTime);
    }

    Actor* Scene::FindActorByID(Core::UUID id)
    {
        for (auto& actor : m_Actors)
        {
            if (actor.GetID() == id) return &actor;
        }
        return nullptr;
    }

    const Actor* Scene::FindActorByID(Core::UUID id) const
    {
        for (const auto& actor : m_Actors)
        {
            if (actor.GetID() == id) return &actor;
        }
        return nullptr;
    }

    Actor* Scene::FindPrimaryCameraActor()
    {
        for (auto& actor : m_Actors)
        {
            const CameraComponent* camera = actor.TryGetComponent<CameraComponent>();
            if (camera && camera->Primary) return &actor;
        }
        return nullptr;
    }

    const Actor* Scene::FindPrimaryCameraActor() const
    {
        for (const auto& actor : m_Actors)
        {
            const CameraComponent* camera = actor.TryGetComponent<CameraComponent>();
            if (camera && camera->Primary) return &actor;
        }
        return nullptr;
    }

    void Scene::OnRuntimeBegin(RuntimeMode mode)
    {
        switch (mode)
        {
            case RuntimeMode::Simulate:
                QUIET_CORE_INFO("Scene runtime begin: Simulate");
                OnSimulateBegin();
                break;

            case RuntimeMode::Play:
                QUIET_CORE_INFO("Scene runtime begin: Play");
                OnPlayBegin();
                break;

            case RuntimeMode::None:
            default:
                break;
        }
    }

    void Scene::OnRuntimeEnd(RuntimeMode mode)
    {
        switch (mode)
        {
            case RuntimeMode::Simulate:
                QUIET_CORE_INFO("Scene runtime end: Simulate");
                OnSimulateEnd();
                break;

            case RuntimeMode::Play:
                QUIET_CORE_INFO("Scene runtime end: Play");
                OnPlayEnd();
                break;

            case RuntimeMode::None:
            default:
                break;
        }
    }

    void Scene::OnSimulateBegin()
    {

    }

    void Scene::OnSimulateEnd()
    {

    }

    void Scene::OnPlayBegin()
    {

    }

    void Scene::OnPlayEnd()
    {

    }

    void Scene::HandleSimulateInput(const Input::Input &input, float deltaTime)
    {
        (void)input;
        (void)deltaTime;
    }

    void Scene::HandlePlayInput(const Input::Input& input, float deltaTime)
    {
        Actor* cameraActor = FindPrimaryCameraActor();
        if (!cameraActor) return;

        auto& transform = cameraActor->GetComponent<TransformComponent>().WorldTransform;
        CameraPilotState state = BuildPilotStateFromCameraTransform(transform);
        CameraController::UpdateFromInput(state, input, deltaTime);
        ApplyPilotStateToCameraTransform(state, transform);
    }

    void Scene::UpdateRuntime(float deltaTime)
    {
        for (auto& actor : m_Actors)
        {
            RotatorComponent* rotator = actor.TryGetComponent<RotatorComponent>();
            if (!rotator) continue;

            auto& transform = actor.GetComponent<TransformComponent>().WorldTransform;
            transform.RotationDegrees.z += rotator->RotationSpeedDegreesPerSecond * deltaTime;
        }
    }

}
