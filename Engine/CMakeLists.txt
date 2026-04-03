# ENGINE
add_library(Quiet_Engine STATIC
        src/Scene/FreeCamera.cpp
        src/Scene/FreeCamera.h
        src/Scene/Transform.h
        src/Scene/Actor.h
        src/Scene/Scene.cpp
        src/Scene/Scene.h
        src/Input/Input.cpp
        src/Input/Input.h
        src/Scene/StaticMeshComponent.h
        src/Scene/TransformComponent.h
        src/Scene/RotatorComponent.h)

target_sources(Quiet_Engine
    PRIVATE
        src/Core/Application.cpp
        src/Core/Log.cpp
        src/Platform/Window.cpp
        src/Renderer/Vulkan/VulkanInstance.cpp
        src/Renderer/Vulkan/VulkanInstance.h
        src/Renderer/Vulkan/VulkanSurface.cpp
        src/Renderer/Vulkan/VulkanSurface.h
        src/Renderer/Vulkan/PhysicalDevice.cpp
        src/Renderer/Vulkan/PhysicalDevice.h
        src/Renderer/Vulkan/LogicalDevice.cpp
        src/Renderer/Vulkan/LogicalDevice.h
        src/Renderer/Vulkan/Swapchain.cpp
        src/Renderer/Vulkan/Swapchain.h
        src/Renderer/Vulkan/GraphicsCommands.h
        src/Renderer/Vulkan/GraphicsCommands.cpp
        src/Renderer/Vulkan/FrameSync.h
        src/Renderer/Vulkan/FrameSync.cpp
        src/Renderer/Vulkan/DepthResources.h
        src/Renderer/Vulkan/DepthResources.cpp
        src/Renderer/Vulkan/TrianglePipeline.h
        src/Renderer/Vulkan/TrianglePipeline.cpp
        src/Renderer/Vulkan/StaticMesh.h
        src/Renderer/Vulkan/StaticMesh.cpp
        src/Renderer/Vulkan/VulkanBuffer.h
        src/Renderer/Vulkan/VulkanBuffer.cpp
        src/Renderer/Vulkan/CameraResources.h
        src/Renderer/Vulkan/CameraResources.cpp

        src/Scene/FreeCamera.h
        src/Scene/FreeCamera.cpp

    PUBLIC
        include/Quiet/Core/Application.h
        include/Quiet/Core/Log.h
        include/Quiet/Platform/Window.h
)

target_include_directories(Quiet_Engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_features(Quiet_Engine PUBLIC cxx_std_20)

target_link_libraries(Quiet_Engine
    PUBLIC
        glfw
        Vulkan::Vulkan
        spdlog::spdlog
        glm
)

if (MSVC)
    target_compile_options(Quiet_Engine PRIVATE /W4 /permissive- /EHsc)
else()
    target_compile_options(Quiet_Engine PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_library(Quiet::Engine ALIAS Quiet_Engine)

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