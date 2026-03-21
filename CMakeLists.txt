cmake_minimum_required(VERSION 3.28)

project(Quiet VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(QUIET_BUILD_SANDBOX ON)
option(QUIET_BUILD_EDITOR OFF)

# Output Directory Config
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/Build/Binaries/Debug")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Build/Binaries/Release")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/Build/Binaries/Debug")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Build/Binaries/Release")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/Build/Binaries/Debug")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Build/Binaries/Release")

if (MSVC)
    set(CMAKE_PDB_OUTPUT_DIRECTORY_DEBUG   "${CMAKE_SOURCE_DIR}/Build/Intermediate/Debug")
    set(CMAKE_PDB_OUTPUT_DIRECTORY_RELEASE "${CMAKE_SOURCE_DIR}/Build/Intermediate/Release")
endif()


add_subdirectory(Engine)

if (QUIET_BUILD_SANDBOX)
    add_subdirectory(Sandbox)
endif()

if (QUIET_BUILD_EDITOR)
    add_subdirectory(Editor)
endif()