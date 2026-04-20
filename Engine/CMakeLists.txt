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

        # Project
        src/Project/ProjectContext.cpp
        src/Project/ProjectDescriptor.cpp
        src/Project/ProjectRegistry.cpp

        # Input
        src/Input/Input.cpp

        # Scene
        src/Scene/Actor.h
        src/Scene/Scene.cpp
        src/Scene/Scene.h
        src/Scene/SceneDocument.cpp
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
        src/Scene/Components/CameraComponent.h
        src/Scene/Components/RotatorComponent.h
        src/Scene/Components/StaticMeshComponent.h
        src/Scene/Components/TransformComponent.h

        # Editor
        src/Editor/EditorActions.cpp
        src/Editor/EditorCommands.cpp
        src/Editor/EditorController.cpp
        src/Editor/EditorSceneHistory.cpp
        src/Editor/EditorViewportInteraction.cpp

        # Renderer - UI
        src/Renderer/UI/ImGuiFonts.h
        src/Renderer/UI/ImGuiLayer.cpp
        src/Renderer/UI/ImGuiLayer.h
        src/Renderer/UI/ImGuiTheme.h
        src/Renderer/UI/ImGuiToolbarCommon.cpp
        src/Renderer/UI/ImGuiToolbarCommon.h

        # Renderer - UI - Panels
        src/Renderer/UI/Panels/ImGuiChrome.cpp
        src/Renderer/UI/Panels/ImGuiDiagnostics.cpp
        src/Renderer/UI/Panels/ImGuiDockspace.cpp
        src/Renderer/UI/Panels/ImGuiProjectManager.cpp
        src/Renderer/UI/Panels/ImGuiScenePanels.cpp
        src/Renderer/UI/Panels/ImGuiViewportPanels.cpp

        # Renderer - Vulkan
        src/Renderer/Vulkan/CameraResources.cpp
        src/Renderer/Vulkan/CameraResources.h
        src/Renderer/Vulkan/DepthResources.cpp
        src/Renderer/Vulkan/DepthResources.h
        src/Renderer/Vulkan/FrameSync.cpp
        src/Renderer/Vulkan/FrameSync.h
        src/Renderer/Vulkan/GraphicsCommands.cpp
        src/Renderer/Vulkan/GraphicsCommands.h
        src/Renderer/Vulkan/LogicalDevice.cpp
        src/Renderer/Vulkan/LogicalDevice.h
        src/Renderer/Vulkan/MeshLibrary.cpp
        src/Renderer/Vulkan/MeshLibrary.h
        src/Renderer/Vulkan/PhysicalDevice.cpp
        src/Renderer/Vulkan/PhysicalDevice.h
        src/Renderer/Vulkan/RenderTypes.h
        src/Renderer/Vulkan/SceneViewportTarget.cpp
        src/Renderer/Vulkan/SceneViewportTarget.h
        src/Renderer/Vulkan/ShaderCompiler.cpp
        src/Renderer/Vulkan/ShaderCompiler.h
        src/Renderer/Vulkan/ShaderModule.cpp
        src/Renderer/Vulkan/ShaderModule.h
        src/Renderer/Vulkan/StaticMesh.cpp
        src/Renderer/Vulkan/StaticMesh.h
        src/Renderer/Vulkan/StaticMeshPipeline.cpp
        src/Renderer/Vulkan/StaticMeshPipeline.h
        src/Renderer/Vulkan/StaticMeshRenderSystem.cpp
        src/Renderer/Vulkan/StaticMeshRenderSystem.h
        src/Renderer/Vulkan/Swapchain.cpp
        src/Renderer/Vulkan/Swapchain.h
        src/Renderer/Vulkan/VulkanBuffer.cpp
        src/Renderer/Vulkan/VulkanBuffer.h
        src/Renderer/Vulkan/VulkanInstance.cpp
        src/Renderer/Vulkan/VulkanInstance.h
        src/Renderer/Vulkan/VulkanRenderer.cpp
        src/Renderer/Vulkan/VulkanRenderer.h
        src/Renderer/Vulkan/VulkanSurface.cpp
        src/Renderer/Vulkan/VulkanSurface.h

        # ImGui
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
        ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
        ${imguizmo_SOURCE_DIR}/ImGuizmo.h

    PUBLIC
        include/Quiet/Core/Application.h
        include/Quiet/Core/AppSpec.h
        include/Quiet/Core/Log.h
        include/Quiet/Core/UUID.h

        include/Quiet/Project/ProjectContext.h
        include/Quiet/Project/ProjectDescriptor.h
        include/Quiet/Project/ProjectRegistry.h

        include/Quiet/Input/Input.h
        include/Quiet/Input/KeyCodes.h
        include/Quiet/Input/MouseCodes.h

        include/Quiet/Editor/EditorActionID.h
        include/Quiet/Editor/EditorActions.h
        include/Quiet/Editor/EditorCommands.h
        include/Quiet/Editor/EditorController.h
        include/Quiet/Editor/EditorSceneHistory.h
        include/Quiet/Editor/EditorState.h

        include/Quiet/Renderer/RenderConventions.h
        include/Quiet/Platform/Window.h
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
        ${imguizmo_SOURCE_DIR}
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