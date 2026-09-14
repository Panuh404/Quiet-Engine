cmake_minimum_required(VERSION 3.28)

project(Quiet VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(QUIET_BUILD_SANDBOX ON)
option(QUIET_BUILD_EDITOR OFF)
option(QUIET_BUILD_LAUNCHER "Build Quiet Launcher" OFF)

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

# Assimp
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_NO_OWN_ZLIB OFF CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_OBJ_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_GLTF_IMPORTER ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_FBX_IMPORTER ON CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ALL_EXPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)

FetchContent_Declare(
        glfw
        GIT_REPOSITORY  https://github.com/glfw/glfw.git
        GIT_TAG         3.4
        GIT_SHALLOW     TRUE
)
FetchContent_Declare(
        spdlog
        GIT_REPOSITORY  https://github.com/gabime/spdlog.git
        GIT_TAG         v1.17.0
        GIT_SHALLOW     TRUE
)
FetchContent_Declare(
        glm
        GIT_REPOSITORY  https://github.com/g-truc/glm.git
        GIT_TAG         1.0.3
        GIT_SHALLOW     TRUE
)
FetchContent_Declare(
        imgui
        GIT_REPOSITORY  https://github.com/ocornut/imgui.git
        GIT_TAG         v1.91.9b-docking
        GIT_SHALLOW     TRUE
)
FetchContent_Declare(
        imguizmo
        GIT_REPOSITORY  https://github.com/CedricGuillemet/ImGuizmo.git
        GIT_TAG         master
        GIT_SHALLOW     TRUE
)
FetchContent_Declare(
        json
        GIT_REPOSITORY  https://github.com/nlohmann/json
        GIT_TAG         v3.12.0
        GIT_SHALLOW     TRUE
)
FetchContent_Declare(
        assimp
        GIT_REPOSITORY  https://github.com/assimp/assimp.git
        GIT_TAG         v6.0.4
        GIT_SHALLOW     TRUE
)
FetchContent_Declare(
        imgui_file_dialog
        GIT_REPOSITORY  https://github.com/aiekick/ImGuiFileDialog.git
        GIT_TAG         v0.6.7
)
FetchContent_Declare(
        stb
        GIT_REPOSITORY  https://github.com/nothings/stb.git
        GIT_TAG         master
        GIT_SHALLOW     TRUE
)
FetchContent_Declare(
        entt
        GIT_REPOSITORY  https://github.com/skypjack/entt.git
        GIT_TAG         v4.0.0
        GIT_SHALLOW     TRUE
)

# Jolt Physics
set(JPH_BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Quiet currently does not use LTO. Jolt's default LTO setting
# requires the parent application to use compatible LTO as well.
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)

# Match Quiet's normal C++ RTTI configuration.
set(CPP_RTTI_ENABLED ON CACHE BOOL "" FORCE)

# Physics debug rendering will be integrated deliberately later.
set(DEBUG_RENDERER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(DEBUG_RENDERER_IN_DISTRIBUTION OFF CACHE BOOL "" FORCE)

# Quiet does not use Jolt's internal profiler yet.
set(PROFILER_IN_DEBUG_AND_RELEASE OFF CACHE BOOL "" FORCE)
set(PROFILER_IN_DISTRIBUTION OFF CACHE BOOL "" FORCE)

# Not required by the runtime integration.
set(ENABLE_OBJECT_STREAM OFF CACHE BOOL "" FORCE)
set(ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

# Don't let third-party warnings block Quiet builds.
set(ENABLE_ALL_WARNINGS OFF CACHE BOOL "" FORCE)

# These are Jolt compute backends, not the regular rigid-body solver.
set(JPH_USE_DX12 OFF CACHE BOOL "" FORCE)
set(JPH_USE_VK OFF CACHE BOOL "" FORCE)
set(JPH_USE_MTL OFF CACHE BOOL "" FORCE)
set(JPH_USE_CPU_COMPUTE OFF CACHE BOOL "" FORCE)

# Keep floating-point exception handling under Quiet's control.
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        jolt_physics
        GIT_REPOSITORY  https://github.com/jrouwe/JoltPhysics.git
        GIT_TAG         v5.6.0
        GIT_SHALLOW     TRUE
        SOURCE_SUBDIR   Build
)

# miniaudio
set(MINIAUDIO_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(MINIAUDIO_INSTALL OFF CACHE BOOL "" FORCE)

# Quiet will add optional DSP/acoustic integrations deliberately.
set(MINIAUDIO_NO_EXTRA_NODES ON CACHE BOOL "" FORCE)

# Initial Quiet audio support targets miniaudio's built-in
# WAV, FLAC and MP3 decoders. Vorbis/Opus can be added later.
set(MINIAUDIO_NO_LIBVORBIS ON CACHE BOOL "" FORCE)
set(MINIAUDIO_NO_LIBOPUS ON CACHE BOOL "" FORCE)

FetchContent_Declare(
        miniaudio
        GIT_REPOSITORY  https://github.com/mackron/miniaudio.git
        GIT_TAG         0.11.25
        GIT_SHALLOW     TRUE
)

# Lua
FetchContent_Declare(
        lua
        URL             https://www.lua.org/ftp/lua-5.4.9.tar.gz
        URL_HASH        SHA256=2335b6c582a52654f94612bf10d2f4672805d05329aa6568b1d8cd9e5c6fb8e6
)

# sol2
set(SOL2_TESTS OFF CACHE BOOL "" FORCE)
set(SOL2_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SOL2_INTEROP_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SOL2_DYNAMIC_LOADING_EXAMPLES OFF CACHE BOOL "" FORCE)
set(SOL2_SINGLE OFF CACHE BOOL "" FORCE)
set(SOL2_DOCS OFF CACHE BOOL "" FORCE)
set(SOL2_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
        sol2
        GIT_REPOSITORY  https://github.com/ThePhD/sol2.git
        GIT_TAG         v3.5.0
        GIT_SHALLOW     TRUE
)

FetchContent_MakeAvailable(
        glfw
        spdlog
        glm
        imgui
        imguizmo
        json
        assimp
        imgui_file_dialog
        stb
        entt
        miniaudio
        jolt_physics
        lua
        sol2
)

# Assimp forces the MinGW runtime and winpthread to link statically when
# BUILD_SHARED_LIBS is disabled. Quiet intentionally uses the normal MinGW
# runtime DLLs instead, so remove Assimp's forced static-runtime flags.
#
# Keep -lwinpthread: Assimp requires it for minizip, but without -Bstatic it
# resolves through the normal MinGW runtime configuration.
if(WIN32 AND MINGW AND TARGET assimp)
    get_target_property(QUIET_ASSIMP_INTERFACE_LINK_LIBRARIES assimp INTERFACE_LINK_LIBRARIES)

    if(QUIET_ASSIMP_INTERFACE_LINK_LIBRARIES)
        list(REMOVE_ITEM QUIET_ASSIMP_INTERFACE_LINK_LIBRARIES
                "-static-libgcc"
                "-static-libstdc++"
                "-Wl,-Bstatic"
                "-lstdc++"
        )

        set_property(TARGET assimp PROPERTY INTERFACE_LINK_LIBRARIES "${QUIET_ASSIMP_INTERFACE_LINK_LIBRARIES}")
    endif()
endif()

# Lua ships as portable C sources without a native CMake library target.
# Quiet builds only the embeddable Lua library; lua.c and luac.c are the
# standalone interpreter/compiler frontends and are intentionally excluded.
add_library(Quiet_Lua STATIC
        ${lua_SOURCE_DIR}/src/lapi.c
        ${lua_SOURCE_DIR}/src/lauxlib.c
        ${lua_SOURCE_DIR}/src/lbaselib.c
        ${lua_SOURCE_DIR}/src/lcode.c
        ${lua_SOURCE_DIR}/src/lcorolib.c
        ${lua_SOURCE_DIR}/src/lctype.c
        ${lua_SOURCE_DIR}/src/ldblib.c
        ${lua_SOURCE_DIR}/src/ldebug.c
        ${lua_SOURCE_DIR}/src/ldo.c
        ${lua_SOURCE_DIR}/src/ldump.c
        ${lua_SOURCE_DIR}/src/lfunc.c
        ${lua_SOURCE_DIR}/src/lgc.c
        ${lua_SOURCE_DIR}/src/linit.c
        ${lua_SOURCE_DIR}/src/liolib.c
        ${lua_SOURCE_DIR}/src/llex.c
        ${lua_SOURCE_DIR}/src/lmathlib.c
        ${lua_SOURCE_DIR}/src/lmem.c
        ${lua_SOURCE_DIR}/src/loadlib.c
        ${lua_SOURCE_DIR}/src/lobject.c
        ${lua_SOURCE_DIR}/src/lopcodes.c
        ${lua_SOURCE_DIR}/src/loslib.c
        ${lua_SOURCE_DIR}/src/lparser.c
        ${lua_SOURCE_DIR}/src/lstate.c
        ${lua_SOURCE_DIR}/src/lstring.c
        ${lua_SOURCE_DIR}/src/lstrlib.c
        ${lua_SOURCE_DIR}/src/ltable.c
        ${lua_SOURCE_DIR}/src/ltablib.c
        ${lua_SOURCE_DIR}/src/ltm.c
        ${lua_SOURCE_DIR}/src/lundump.c
        ${lua_SOURCE_DIR}/src/lutf8lib.c
        ${lua_SOURCE_DIR}/src/lvm.c
        ${lua_SOURCE_DIR}/src/lzio.c
)

target_include_directories(Quiet_Lua PUBLIC ${lua_SOURCE_DIR}/src)

if(WIN32)
    target_compile_definitions(Quiet_Lua PRIVATE LUA_USE_WINDOWS)
elseif(APPLE)
    target_compile_definitions(Quiet_Lua PRIVATE LUA_USE_MACOSX)
elseif(UNIX)
    target_compile_definitions(Quiet_Lua PRIVATE LUA_USE_LINUX)
    target_link_libraries(Quiet_Lua PRIVATE ${CMAKE_DL_LIBS} m)
endif()

add_library(Quiet::Lua ALIAS Quiet_Lua)

find_package(Vulkan REQUIRED)

# ------------------------------------------------------------------------------
# Shared paths / helpers
# ------------------------------------------------------------------------------
set(QUIET_ENGINE_RESOURCES_DIR      "${CMAKE_SOURCE_DIR}/Engine/Resources")
set(QUIET_ENGINE_FONTS_DIR          "${QUIET_ENGINE_RESOURCES_DIR}/Fonts")
set(QUIET_ENGINE_TEXTURES_DIR       "${QUIET_ENGINE_RESOURCES_DIR}/Textures")
set(QUIET_ENGINE_SHADER_SOURCE_DIR  "${QUIET_ENGINE_RESOURCES_DIR}/Shaders")

function(quiet_copy_engine_resources target_name)
    if(NOT TARGET ${target_name})
        return()
    endif()

    if(EXISTS "${QUIET_ENGINE_FONTS_DIR}")
        add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target_name}>/Resources/Fonts"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${QUIET_ENGINE_FONTS_DIR}"
                "$<TARGET_FILE_DIR:${target_name}>/Resources/Fonts"
                COMMENT "Copying engine fonts for ${target_name}"
        )
    endif()

    if(EXISTS "${QUIET_ENGINE_TEXTURES_DIR}")
        add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:${target_name}>/Resources/Textures"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${QUIET_ENGINE_TEXTURES_DIR}"
                "$<TARGET_FILE_DIR:${target_name}>/Resources/Textures"
                COMMENT "Copying engine textures for ${target_name}"
        )
    endif()
endfunction()

# ------------------------------------------------------------------------------
# Subdirectories
# ------------------------------------------------------------------------------

add_subdirectory(Engine)

if(QUIET_BUILD_LAUNCHER)
    add_subdirectory(Launcher)
endif()

if(QUIET_BUILD_SANDBOX)
    add_subdirectory(Sandbox)
endif()

if(QUIET_BUILD_EDITOR)
    add_subdirectory(Editor)
endif()


# ------------------------------------------------------------------------------
# Shader compilation
# ------------------------------------------------------------------------------
set(QUIET_SHADER_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/Build/Binaries/$<CONFIG>/Resources/Shaders")

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

if(TARGET Quiet_Sandbox)
    add_dependencies(Quiet_Sandbox Quiet_Shaders)
endif()

if(TARGET Quiet_Editor)
    add_dependencies(Quiet_Editor Quiet_Shaders)
endif()

if(TARGET Quiet_Launcher AND TARGET Quiet_Editor)
    add_dependencies(Quiet_Launcher Quiet_Editor)
endif()