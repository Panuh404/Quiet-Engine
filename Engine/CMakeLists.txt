# ENGINE
add_library(Quiet_Engine STATIC)

target_sources(Quiet_Engine
    PRIVATE
        # Core
        src/Core/Application.cpp
        src/Core/Log.cpp
        src/Core/UUID.cpp

        # Platform
        src/Platform/Window.cpp
        src/Platform/Shell.cpp

        # Project
        src/Project/AssetRegistry.cpp
        src/Project/ProjectAssetLoader.cpp
        src/Project/ProjectContext.cpp
        src/Project/ProjectDescriptor.cpp
        src/Project/ProjectRegistry.cpp
        src/Project/ProjectWorkflow.cpp

        # Input
        src/Input/Input.cpp

        # Audio
        src/Audio/AudioAsset.h
        src/Audio/AudioBus.h
        src/Audio/AudioLibrary.cpp
        src/Audio/AudioLibrary.h
        src/Audio/AudioSubsystem.h
        src/Audio/AudioSubsystem.cpp
        src/Audio/AudioWorld.h
        src/Audio/AudioWorld.cpp

        # Physics
        src/Physics/PhysicsContactEvent.h
        src/Physics/PhysicsSubsystem.h
        src/Physics/PhysicsSubsystem.cpp
        src/Physics/PhysicsWorld.cpp
        src/Physics/PhysicsWorld.h

        # Scene
        src/Scene/Actor.h
        src/Scene/Scene.cpp
        src/Scene/Scene.h
        src/Scene/SceneAudioSettings.h
        src/Scene/SceneDocument.cpp
        src/Scene/SceneHierarchy.h
        src/Scene/ScenePhysicsSettings.h
        src/Scene/SceneRenderSettings.h
        src/Scene/SceneSerializer.cpp
        src/Scene/SceneSerializer.h
        src/Scene/Transform.h

        # Scene - Camera
        src/Scene/Camera/CameraController.cpp
        src/Scene/Camera/CameraController.h
        src/Scene/Camera/CameraPilotState.h
        src/Scene/Camera/CameraPilotUtils.h
        src/Scene/Camera/FreeCamera.cpp
        src/Scene/Camera/FreeCamera.h

        # Scene - Components
        src/Scene/Components/ActorIdentityComponent.h
        src/Scene/Components/AudioListenerComponent.h
        src/Scene/Components/AudioSourceComponent.h
        src/Scene/Components/CameraComponent.h
        src/Scene/Components/CapsuleColliderComponent.h
        src/Scene/Components/DirectionalLightComponent.h
        src/Scene/Components/MaterialComponent.h
        src/Scene/Components/RotatorComponent.h
        src/Scene/Components/StaticMeshComponent.h
        src/Scene/Components/TransformComponent.h
        src/Scene/Components/RigidBodyComponent.h
        src/Scene/Components/BoxColliderComponent.h
        src/Scene/Components/SphereColliderComponent.h

        # Editor
        src/Editor/EditorActions.cpp
        src/Editor/EditorCommands.cpp
        src/Editor/EditorController.cpp
        src/Editor/EditorRuntimeController.cpp
        src/Editor/EditorSceneHistory.cpp
        src/Editor/EditorSceneWorkflow.cpp

        # Editor - Viewport
        src/Editor/Viewport/EditorViewportGizmo.cpp
        src/Editor/Viewport/EditorViewportGizmo.h
        src/Editor/Viewport/EditorViewportInteraction.cpp
        src/Editor/Viewport/EditorViewportOverlay.cpp
        src/Editor/Viewport/EditorViewportOverlay.h
        src/Editor/Viewport/EditorViewportPicking.cpp
        src/Editor/Viewport/EditorViewportPicking.h
        src/Editor/Viewport/EditorViewportProjection.cpp
        src/Editor/Viewport/EditorViewportProjection.h

        # Renderer - UI
        src/Renderer/UI/ImGuiFonts.h
        src/Renderer/UI/ImGuiLayer.cpp
        src/Renderer/UI/ImGuiLayer.h
        src/Renderer/UI/ImGuiTexturePreview.cpp
        src/Renderer/UI/ImGuiTexturePreview.h
        src/Renderer/UI/ImGuiTheme.h
        src/Renderer/UI/ImGuiToolbarCommon.cpp
        src/Renderer/UI/ImGuiToolbarCommon.h

        # Renderer - UI - Assets
        src/Renderer/UI/Assets/ImGuiAssetBrowserTypes.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserContext.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserSources.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserSources.cpp
        src/Renderer/UI/Assets/ImGuiAssetBrowserUtils.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserUtils.cpp
        src/Renderer/UI/Assets/ImGuiAssetBrowserOperations.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserOperations.cpp
        src/Renderer/UI/Assets/ImGuiAssetBrowserWidgets.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserWidgets.cpp
        src/Renderer/UI/Assets/ImGuiAssetPicker.h
        src/Renderer/UI/Assets/ImGuiAssetPicker.cpp
        src/Renderer/UI/Assets/ImGuiAssetBrowserToolbar.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserToolbar.cpp
        src/Renderer/UI/Assets/ImGuiAssetBrowserImport.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserImport.cpp
        src/Renderer/UI/Assets/ImGuiAssetBrowserContextMenu.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserContextMenu.cpp
        src/Renderer/UI/Assets/ImGuiAssetBrowserGridView.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserGridView.cpp
        src/Renderer/UI/Assets/ImGuiAssetBrowserListView.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserListView.cpp
        src/Renderer/UI/Assets/ImGuiAssetBrowserDetails.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserDetails.cpp
        src/Renderer/UI/Assets/ImGuiAssetBrowserDialogs.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserDialogs.cpp
        src/Renderer/UI/Assets/ImGuiAssetBrowserPanel.h
        src/Renderer/UI/Assets/ImGuiAssetBrowserPanel.cpp
        src/Renderer/UI/Assets/ImGuiAssetFileDialog.h
        src/Renderer/UI/Assets/ImGuiAssetFileDialog.cpp

        # Renderer - UI - Inspector
        src/Renderer/UI/Inspector/ImGuiInspectorContext.h
        src/Renderer/UI/Inspector/ImGuiInspectorPanel.cpp
        src/Renderer/UI/Inspector/ImGuiInspectorPanel.h
        src/Renderer/UI/Inspector/ImGuiInspectorSections.cpp
        src/Renderer/UI/Inspector/ImGuiInspectorSections.h
        src/Renderer/UI/Inspector/ImGuiInspectorWidgets.cpp
        src/Renderer/UI/Inspector/ImGuiInspectorWidgets.h
        src/Renderer/UI/Inspector/ImGuiInspectorState.h

        # Renderer - UI - Panels
        src/Renderer/UI/Panels/ImGuiInspector.cpp
        src/Renderer/UI/Panels/ImGuiAssetsBrowser.cpp
        src/Renderer/UI/Panels/ImGuiChrome.cpp
        src/Renderer/UI/Panels/ImGuiDiagnostics.cpp
        src/Renderer/UI/Panels/ImGuiDockspace.cpp
        src/Renderer/UI/Panels/ImGuiHierarchyPanel.cpp
        src/Renderer/UI/Panels/ImGuiProjectManager.cpp
        src/Renderer/UI/Panels/ImGuiSceneSettingsPanel.cpp
        src/Renderer/UI/Panels/ImGuiViewportPanels.cpp

        # Renderer - Vulkan - Mesh
        src/Renderer/Vulkan/Mesh/MeshImporter.cpp
        src/Renderer/Vulkan/Mesh/MeshImporter.h
        src/Renderer/Vulkan/Mesh/MeshImportResult.h
        src/Renderer/Vulkan/Mesh/MeshLibrary.cpp
        src/Renderer/Vulkan/Mesh/MeshLibrary.h
        src/Renderer/Vulkan/Mesh/MeshPrimitives.cpp
        src/Renderer/Vulkan/Mesh/MeshPrimitives.h
        src/Renderer/Vulkan/Mesh/StaticMesh.cpp
        src/Renderer/Vulkan/Mesh/StaticMesh.h
        src/Renderer/Vulkan/Mesh/StaticMeshMaterialResources.cpp
        src/Renderer/Vulkan/Mesh/StaticMeshMaterialResources.h
        src/Renderer/Vulkan/Mesh/StaticMeshPipeline.cpp
        src/Renderer/Vulkan/Mesh/StaticMeshPipeline.h
        src/Renderer/Vulkan/Mesh/StaticMeshRenderSystem.cpp
        src/Renderer/Vulkan/Mesh/StaticMeshRenderSystem.h

        # Renderer - Vulkan - Texture
        src/Renderer/Vulkan/Texture/StbImage.cpp
        src/Renderer/Vulkan/Texture/Texture2D.cpp
        src/Renderer/Vulkan/Texture/Texture2D.h
        src/Renderer/Vulkan/Texture/TextureImporter.cpp
        src/Renderer/Vulkan/Texture/TextureImporter.h
        src/Renderer/Vulkan/Texture/TextureImportResult.h
        src/Renderer/Vulkan/Texture/TextureLibrary.cpp
        src/Renderer/Vulkan/Texture/TextureLibrary.h
        src/Renderer/Vulkan/Texture/TexturePixelData.h

        # Renderer - Vulkan - Context
        src/Renderer/Vulkan/Context/GraphicsCommands.cpp
        src/Renderer/Vulkan/Context/GraphicsCommands.h
        src/Renderer/Vulkan/Context/LogicalDevice.cpp
        src/Renderer/Vulkan/Context/LogicalDevice.h
        src/Renderer/Vulkan/Context/PhysicalDevice.cpp
        src/Renderer/Vulkan/Context/PhysicalDevice.h
        src/Renderer/Vulkan/Context/VulkanBuffer.cpp
        src/Renderer/Vulkan/Context/VulkanBuffer.h
        src/Renderer/Vulkan/Context/VulkanInstance.cpp
        src/Renderer/Vulkan/Context/VulkanInstance.h
        src/Renderer/Vulkan/Context/VulkanSurface.cpp
        src/Renderer/Vulkan/Context/VulkanSurface.h

        # Renderer - Vulkan - Frame
        src/Renderer/Vulkan/Frame/CameraResources.cpp
        src/Renderer/Vulkan/Frame/CameraResources.h
        src/Renderer/Vulkan/Frame/DepthResources.cpp
        src/Renderer/Vulkan/Frame/DepthResources.h
        src/Renderer/Vulkan/Frame/FrameSync.cpp
        src/Renderer/Vulkan/Frame/FrameSync.h
        src/Renderer/Vulkan/Frame/SceneViewportTarget.cpp
        src/Renderer/Vulkan/Frame/SceneViewportTarget.h
        src/Renderer/Vulkan/Frame/Swapchain.cpp
        src/Renderer/Vulkan/Frame/Swapchain.h

        # Renderer - Vulkan - Shader
        src/Renderer/Vulkan/Shader/ShaderCompiler.cpp
        src/Renderer/Vulkan/Shader/ShaderCompiler.h
        src/Renderer/Vulkan/Shader/ShaderModule.cpp
        src/Renderer/Vulkan/Shader/ShaderModule.h

        # Renderer - Vulkan - Editor
        src/Renderer/Vulkan/Editor/EditorGridPipeline.h
        src/Renderer/Vulkan/Editor/EditorGridPipeline.cpp
        src/Renderer/Vulkan/Editor/EditorGridRenderSystem.h
        src/Renderer/Vulkan/Editor/EditorGridRenderSystem.cpp
        src/Renderer/Vulkan/Editor/SelectionMaskPipeline.h
        src/Renderer/Vulkan/Editor/SelectionMaskPipeline.cpp
        src/Renderer/Vulkan/Editor/SelectionMaskRenderSystem.h
        src/Renderer/Vulkan/Editor/SelectionMaskRenderSystem.cpp
        src/Renderer/Vulkan/Editor/SelectionOutlinePipeline.h
        src/Renderer/Vulkan/Editor/SelectionOutlinePipeline.cpp
        src/Renderer/Vulkan/Editor/SelectionOutlineRenderSystem.h
        src/Renderer/Vulkan/Editor/SelectionOutlineRenderSystem.cpp

        # Renderer - Vulkan
        src/Renderer/Vulkan/RenderTypes.h
        src/Renderer/Vulkan/VulkanRenderer.cpp
        src/Renderer/Vulkan/VulkanRenderer.h

        # ImGui
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
        ${imguizmo_SOURCE_DIR}/src/ImGuizmo.cpp
        ${imguizmo_SOURCE_DIR}/src/ImGuizmo.h
        ${imgui_file_dialog_SOURCE_DIR}/ImGuiFileDialog.cpp

    PUBLIC
        include/Quiet/Core/Application.h
        include/Quiet/Core/AppSpec.h
        include/Quiet/Core/Log.h
        include/Quiet/Core/UUID.h

        include/Quiet/Project/AssetRegistry.h
        include/Quiet/Project/AssetRecord.h
        include/Quiet/Project/AssetType.h
        include/Quiet/Project/ProjectAssetLoader.h
        include/Quiet/Project/ProjectContext.h
        include/Quiet/Project/ProjectDescriptor.h
        include/Quiet/Project/ProjectRegistry.h
        include/Quiet/Project/ProjectWorkflow.h

        include/Quiet/Input/Input.h
        include/Quiet/Input/KeyCodes.h
        include/Quiet/Input/MouseCodes.h

        include/Quiet/Editor/EditorActionID.h
        include/Quiet/Editor/EditorActions.h
        include/Quiet/Editor/EditorCommands.h
        include/Quiet/Editor/EditorController.h
        include/Quiet/Editor/EditorRuntimeController.h
        include/Quiet/Editor/EditorSceneHistory.h
        include/Quiet/Editor/EditorSceneWorkflow.h
        include/Quiet/Editor/EditorState.h

        include/Quiet/Renderer/RenderConventions.h
        include/Quiet/Platform/Window.h
        include/Quiet/Platform/Shell.h
        include/Quiet/Math/Direction.h
        include/Quiet/Math/Geometry.h
        include/Quiet/Scene/SceneDocument.h
)

target_include_directories(Quiet_Engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${imguizmo_SOURCE_DIR}/src
        ${imgui_file_dialog_SOURCE_DIR}
        ${stb_SOURCE_DIR}
)

target_compile_features(Quiet_Engine
        PUBLIC
        cxx_std_20
)

target_compile_definitions(Quiet_Engine
        PUBLIC
        QUIET_ENGINE_RESOURCES_PATH="Resources"
)

target_link_libraries(Quiet_Engine
    PUBLIC
        glfw
        Vulkan::Vulkan
        spdlog::spdlog
        glm
        nlohmann_json::nlohmann_json
        assimp

    PRIVATE
        EnTT::EnTT
        miniaudio
        Jolt
)

if (MSVC)
    target_compile_options(Quiet_Engine PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(Quiet_Engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(Quiet::Engine ALIAS Quiet_Engine)

# Engine resource files for IDE visibility
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Resources")
    file(GLOB_RECURSE QUIET_ENGINE_RESOURCE_FILES CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/Resources/*"
    )

    if(QUIET_ENGINE_RESOURCE_FILES)
        target_sources(Quiet_Engine PRIVATE ${QUIET_ENGINE_RESOURCE_FILES})
        source_group(
                TREE "${CMAKE_CURRENT_SOURCE_DIR}/Resources"
                PREFIX "Resources"
                FILES ${QUIET_ENGINE_RESOURCE_FILES}
        )
        set_source_files_properties(${QUIET_ENGINE_RESOURCE_FILES}
                PROPERTIES HEADER_FILE_ONLY ON
        )
    endif()
endif()

if (WIN32 AND MINGW)
    # Get MinGW bin dir from the compiler path
    get_filename_component(MINGW_BIN_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

    foreach(dll libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
        add_custom_command(TARGET Quiet_Engine POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MINGW_BIN_DIR}/${dll}"
                "$<TARGET_FILE_DIR:Quiet_Engine>"
                VERBATIM
        )
    endforeach()
endif()
