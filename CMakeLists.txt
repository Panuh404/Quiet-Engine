cmake_minimum_required(VERSION 3.28)

project(Quiet VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(QUIET_BUILD_SANDBOX ON)
option(QUIET_BUILD_EDITOR OFF)

# ------------------------------------------------------------------------------
# Output directories
# ------------------------------------------------------------------------------

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/Build/Binaries/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Build/Binaries/Release")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/Build/Binaries/Debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Build/Binaries/Release")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/Build/Binaries/Debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Build/Binaries/Release")

if(MSVC)
    set(CMAKE_PDB_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/Build/Intermediate/Debug")
    set(CMAKE_PDB_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Build/Intermediate/Release")
endif()

# Helpful fallback for single-config generators
if(NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/Binaries")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/Binaries")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/Build/Binaries")
endif()

# ------------------------------------------------------------------------------
# Dependencies
# ------------------------------------------------------------------------------

include(FetchContent)

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG        3.4
        GIT_SHALLOW    TRUE
)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG        v1.17.0
        GIT_SHALLOW    TRUE
)
FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.3
        GIT_SHALLOW    TRUE
)
FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG docking
        GIT_SHALLOW TRUE
)
FetchContent_Declare(
        imguizmo
        GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG master
        GIT_SHALLOW TRUE
)
FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json
        GIT_TAG v3.12.0
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw spdlog glm imgui imguizmo json)

find_package(Vulkan REQUIRED)



# ------------------------------------------------------------------------------
# Shared paths / helpers
# ------------------------------------------------------------------------------

set(QUIET_ENGINE_RESOURCES_DIR "${CMAKE_SOURCE_DIR}/Engine/resources")
set(QUIET_ENGINE_SHADER_SOURCE_DIR "${QUIET_ENGINE_RESOURCES_DIR}/shaders")
if(NOT EXISTS "${QUIET_ENGINE_SHADER_SOURCE_DIR}")
    set(QUIET_ENGINE_SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Engine/Shaders")
endif()

function(quiet_copy_engine_resources target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    if(EXISTS "${QUIET_ENGINE_RESOURCES_DIR}")
        add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target_name}>/resources"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${QUIET_ENGINE_RESOURCES_DIR}"
                "$<TARGET_FILE_DIR:${target_name}>/resources"
                COMMENT "Copying engine resources for ${target_name}"
        )
    endif()
endfunction()


# ------------------------------------------------------------------------------
# Subdirectories
# ------------------------------------------------------------------------------

add_subdirectory(Engine)

if(QUIET_BUILD_SANDBOX)
    add_subdirectory(Sandbox)
endif()

if(QUIET_BUILD_EDITOR)
    add_subdirectory(Editor)
endif()

# ------------------------------------------------------------------------------
# Shader compilation
# ------------------------------------------------------------------------------

set(QUIET_SHADER_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/Build/Binaries/$<CONFIG>/Shaders")

file(GLOB QUIET_SHADER_SOURCES CONFIGURE_DEPENDS
        "${QUIET_ENGINE_SHADER_SOURCE_DIR}/*.vert"
        "${QUIET_ENGINE_SHADER_SOURCE_DIR}/*.frag"
        "${QUIET_ENGINE_SHADER_SOURCE_DIR}/*.comp"
        "${QUIET_ENGINE_SHADER_SOURCE_DIR}/*.geom"
        "${QUIET_ENGINE_SHADER_SOURCE_DIR}/*.tesc"
        "${QUIET_ENGINE_SHADER_SOURCE_DIR}/*.tese"
)

set(QUIET_SHADER_OUTPUTS)

foreach(SHADER ${QUIET_SHADER_SOURCES})
    get_filename_component(SHADER_NAME "${SHADER}" NAME)
    set(SHADER_OUTPUT "${QUIET_SHADER_OUTPUT_DIR}/${SHADER_NAME}.spv")

    add_custom_command(
            OUTPUT "${SHADER_OUTPUT}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${QUIET_SHADER_OUTPUT_DIR}"
            COMMAND "${Vulkan_GLSLC_EXECUTABLE}" "${SHADER}" -o "${SHADER_OUTPUT}"
            DEPENDS "${SHADER}"
            VERBATIM
    )

    list(APPEND QUIET_SHADER_OUTPUTS "${SHADER_OUTPUT}")
endforeach()

add_custom_target(Quiet_Shaders ALL DEPENDS ${QUIET_SHADER_OUTPUTS})

set(QUIET_FONT_SOURCE_DIR "${QUIET_ENGINE_RESOURCES_DIR}/Fonts")
set(QUIET_FONT_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/Build/Binaries/$<CONFIG>/Resources/Fonts")

if(EXISTS "${QUIET_FONT_SOURCE_DIR}")
    add_custom_target(Quiet_Font
            COMMAND ${CMAKE_COMMAND} -E make_directory "${QUIET_FONT_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${QUIET_FONT_SOURCE_DIR}"
            "${QUIET_FONT_OUTPUT_DIR}"
            COMMENT "Copying engine fonts"
    )

    if(TARGET Quiet_Sandbox)
        add_dependencies(Quiet_Sandbox Quiet_Font)
    endif()

    if(TARGET Quiet_Editor)
        add_dependencies(Quiet_Editor Quiet_Font)
    endif()
endif()

if(TARGET Quiet_Sandbox)
    add_dependencies(Quiet_Sandbox Quiet_Shaders)
endif()

if(TARGET Quiet_Editor)
    add_dependencies(Quiet_Editor Quiet_Shaders)
endif()