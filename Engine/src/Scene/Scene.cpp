#include "Scene.h"

#include <optional>
#include <algorithm>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "Audio/AudioWorld.h"
#include "Physics/PhysicsWorld.h"
#include "Scene/Camera/CameraPilotUtils.h"
#include "Camera/CameraController.h"
#include "Quiet/Core/Log.h"


namespace Quiet::Scene
{
    Scene::Scene()
    {
        QUIET_CORE_INFO("Initialized scene");
    }

    Scene::Scene(const Scene& other) : Scene()
    {
        *this = other;
    }

    Scene& Scene::operator=(const Scene& other)
    {
        if (this == &other) return *this;

        Clear();

        m_PhysicsSettings = other.m_PhysicsSettings;
        m_RenderSettings = other.m_RenderSettings;
        for (const Actor& source : other.m_Actors)
        {
            Actor& target = CreateActorWithID(source.GetID(), source.GetName());
            target.GetComponent<TransformComponent>() = source.GetComponent<TransformComponent>();

            if (const auto* component = source.TryGetComponent<StaticMeshComponent>())
                target.AddComponent<StaticMeshComponent>(*component);

            if (const auto* component = source.TryGetComponent<MaterialComponent>())
                target.AddComponent<MaterialComponent>(*component);

            if (const auto* component = source.TryGetComponent<DirectionalLightComponent>())
                target.AddComponent<DirectionalLightComponent>(*component);

            if (const auto* component = source.TryGetComponent<RotatorComponent>())
                target.AddComponent<RotatorComponent>(*component);

            if (const auto* component = source.TryGetComponent<CameraComponent>())
                target.AddComponent<CameraComponent>(*component);

            if (const auto* component = source.TryGetComponent<AudioSourceComponent>())
                target.AddComponent<AudioSourceComponent>(*component);

            if (const auto* component = source.TryGetComponent<AudioListenerComponent>())
                target.AddComponent<AudioListenerComponent>(*component);

            if (const auto* component = source.TryGetComponent<RigidBodyComponent>())
                target.AddComponent<RigidBodyComponent>(*component);

            if (const auto* component = source.TryGetComponent<BoxColliderComponent>())
                target.AddComponent<BoxColliderComponent>(*component);

            if (const auto* component = source.TryGetComponent<SphereColliderComponent>())
                target.AddComponent<SphereColliderComponent>(*component);

            if (const auto* component = source.TryGetComponent<CapsuleColliderComponent>())
                target.AddComponent<CapsuleColliderComponent>(*component);
        }

        m_Hierarchy = other.m_Hierarchy;
        m_RuntimeMode = RuntimeMode::None;
        return *this;
    }

    Scene::~Scene() = default;

    Actor& Scene::CreateActor(const std::string& name)
    {
        return CreateActorWithID(Core::UUIDGenerator::New(), name);
    }

    Actor& Scene::CreateActorWithID(Core::UUID id, const std::string& name)
    {
        const entt::entity entity = m_Registry.create();

        m_Registry.emplace<ActorIdentityComponent>(entity, ActorIdentityComponent{ id, name });
        m_Registry.emplace<TransformComponent>(entity);

        Actor actor(m_Registry, entity);
        m_Actors.push_back(std::move(actor));

        AddActorToHierarchyRoot(id);
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

        const std::string duplicateName = source->GetName().empty() ? "Actor Copy" : source->GetName() + " Copy";

        const TransformComponent transform = source->GetComponent<TransformComponent>();
        std::optional<StaticMeshComponent> staticMesh;
        std::optional<MaterialComponent> material;
        std::optional<DirectionalLightComponent> directionalLight;
        std::optional<RotatorComponent> rotator;
        std::optional<CameraComponent> camera;
        std::optional<AudioSourceComponent> audioSource;
        std::optional<AudioListenerComponent> audioListener;
        std::optional<RigidBodyComponent> rigidBody;
        std::optional<BoxColliderComponent> boxCollider;
        std::optional<SphereColliderComponent> sphereCollider;
        std::optional<CapsuleColliderComponent> capsuleCollider;

        if (const auto* component = source->TryGetComponent<StaticMeshComponent>())
            staticMesh = *component;

        if (const auto* component = source->TryGetComponent<MaterialComponent>())
            material = *component;

        if (const auto* component = source->TryGetComponent<DirectionalLightComponent>())
            directionalLight = *component;

        if (const auto* component = source->TryGetComponent<RotatorComponent>())
            rotator = *component;

        if (const auto* component = source->TryGetComponent<CameraComponent>())
            camera = *component;

        if (const auto* component = source->TryGetComponent<AudioSourceComponent>())
            audioSource = *component;

        if (const auto* component = source->TryGetComponent<AudioListenerComponent>())
            audioListener = *component;

        if (const auto* component = source->TryGetComponent<RigidBodyComponent>())
            rigidBody = *component;

        if (const auto* component = source->TryGetComponent<BoxColliderComponent>())
            boxCollider = *component;

        if (const auto* component = source->TryGetComponent<SphereColliderComponent>())
            sphereCollider = *component;

        if (const auto* component = source->TryGetComponent<CapsuleColliderComponent>())
            capsuleCollider = *component;

        Actor& duplicate = CreateActor(duplicateName);
        duplicate.GetComponent<TransformComponent>() = transform;

        if (staticMesh)         duplicate.AddComponent<StaticMeshComponent>(*staticMesh);
        if (material)           duplicate.AddComponent<MaterialComponent>(*material);
        if (directionalLight)   duplicate.AddComponent<DirectionalLightComponent>(*directionalLight);
        if (rotator)            duplicate.AddComponent<RotatorComponent>(*rotator);
        if (camera)             duplicate.AddComponent<CameraComponent>(*camera);
        if (audioSource)        duplicate.AddComponent<AudioSourceComponent>(*audioSource);
        if (audioListener)      duplicate.AddComponent<AudioListenerComponent>(*audioListener);
        if (rigidBody)          duplicate.AddComponent<RigidBodyComponent>(*rigidBody);
        if (boxCollider)        duplicate.AddComponent<BoxColliderComponent>(*boxCollider);
        if (sphereCollider)     duplicate.AddComponent<SphereColliderComponent>(*sphereCollider);
        if (capsuleCollider)    duplicate.AddComponent<CapsuleColliderComponent>(*capsuleCollider);
        return &duplicate;
    }

    bool Scene::DestroyActorByID(Core::UUID id)
    {
        const auto it =
            std::find_if(
                m_Actors.begin(),
                m_Actors.end(),
                [id](const Actor& actor)
                {
                    return actor.GetID() == id;
                }
            );

        if (it == m_Actors.end()) return false;

        const entt::entity entity = it->m_Entity;
        if (m_Registry.valid(entity))
        {
            m_Registry.destroy(entity);
        }

        m_Actors.erase(it);
        RemoveActorFromHierarchy(id);
        return true;
    }

    void Scene::Clear()
    {
        m_AudioWorld.reset();
        m_PhysicsWorld.reset();
        m_PhysicsContactEvents.clear();
        m_PhysicsSettings = ScenePhysicsSettings{};
        m_RenderSettings = SceneRenderSettings{};
        m_Hierarchy = SceneHierarchyState{};
        m_Registry.clear();
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
            AddActorToHierarchyRoot(actor.GetID());
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
            const Core::UUID actorID = actor.GetID();
            if (actorID.IsValid())
            {
                validActorIDs.insert(actorID);
            }
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
            const Core::UUID actorID = actor.GetID();
            if (!actorID.IsValid()) continue;
            if (placedActorIDs.contains(actorID)) continue;

            sanitizedRootNodes.push_back(SceneHierarchyNode{ SceneHierarchyNodeType::Actor, actorID });
            placedActorIDs.insert(actorID);

            QUIET_CORE_WARN("Hierarchy metadata was missing actor '{}'. Appended actor to root.", Core::UUIDToString(actorID));
        }

        m_Hierarchy.RootNodes = std::move(sanitizedRootNodes);
    }

    void Scene::BeginRuntime(RuntimeMode mode, Physics::PhysicsSubsystem& physicsSubsystem, Audio::AudioSubsystem& audioSubsystem)
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
        m_PhysicsContactEvents.clear();
        m_PhysicsWorld = std::make_unique<Physics::PhysicsWorld>(physicsSubsystem, m_PhysicsSettings.Gravity);
        m_PhysicsWorld->BuildBodies(m_Registry);

        m_AudioWorld = std::make_unique<Audio::AudioWorld>(audioSubsystem);
        m_AudioWorld->BuildSources(m_Registry);

        m_RuntimeMode = mode;
        OnRuntimeBegin(mode);
    }

    void Scene::EndRuntime()
    {
        if (m_RuntimeMode == RuntimeMode::None) return;
        const RuntimeMode previousMode = m_RuntimeMode;
        OnRuntimeEnd(previousMode);
        m_AudioWorld.reset();
        m_PhysicsWorld.reset();
        m_PhysicsContactEvents.clear();
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

    bool Scene::ApplyPhysicsImpulse(Core::UUID actorID, const glm::vec3 &impulse)
    {
        if (!m_PhysicsWorld || !IsRuntimeActive()) return false;

        const entt::entity entity = FindEntityByID(actorID);
        if (entity == entt::null) return false;

        return m_PhysicsWorld->ApplyImpulse(entity, impulse);
    }

    bool Scene::SetPhysicsLinearVelocity(Core::UUID actorID, const glm::vec3 &velocity)
    {
        if (!m_PhysicsWorld || !IsRuntimeActive()) return false;

        const entt::entity entity = FindEntityByID(actorID);
        if (entity == entt::null) return false;

        return m_PhysicsWorld->SetLinearVelocity(entity, velocity);
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

    entt::entity Scene::FindEntityByID(Core::UUID id) const
    {
        auto view = m_Registry.view<const ActorIdentityComponent>();
        for (const entt::entity entity : view)
        {
            const auto& identity = m_Registry.get<ActorIdentityComponent>(entity);
            if (identity.ID == id) return entity;
        }

        return entt::null;
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

    const Actor* Scene::FindPrimaryAudioListenerActor() const
    {
        for (const Actor& actor : m_Actors)
        {
            const AudioListenerComponent* listener = actor.TryGetComponent<AudioListenerComponent>();
            if (listener && listener->Primary) return &actor;
        }
        return nullptr;
    }

    Actor* Scene::FindPrimaryAudioListenerActor()
    {
        for (Actor& actor : m_Actors)
        {
            const AudioListenerComponent* listener = actor.TryGetComponent<AudioListenerComponent>();
            if (listener && listener->Primary) return &actor;
        }

        return nullptr;
    }

    const DirectionalLightComponent * Scene::FindActiveDirectionalLight() const
    {
        for (const Actor& actor : m_Actors)
        {
            const auto* light = actor.TryGetComponent<DirectionalLightComponent>();
            if (light && light->Enabled) return light;
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
        auto view = m_Registry.view<TransformComponent, RotatorComponent>();

        for (const entt::entity entity : view)
        {
            const auto* rigidBody = m_Registry.try_get<RigidBodyComponent>(entity);
            if (rigidBody && rigidBody->MotionType != RigidBodyMotionType::Kinematic) continue;

            auto& transform = m_Registry.get<TransformComponent>(entity).WorldTransform;
            const auto& rotator = m_Registry.get<RotatorComponent>(entity);

            transform.RotationDegrees.z += rotator.RotationSpeedDegreesPerSecond * deltaTime;
        }

        m_PhysicsContactEvents.clear();
        if (m_PhysicsWorld)
        {
            m_PhysicsWorld->Update(deltaTime, m_Registry);
            m_PhysicsContactEvents = m_PhysicsWorld->ConsumeContactEvents();
        }
    }
}
