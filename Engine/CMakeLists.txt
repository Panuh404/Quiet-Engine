# ENGINE
add_library(Quiet_Engine STATIC)

target_sources(Quiet_Engine
    PRIVATE
        src/Core/Application.cpp
        src/Core/Log.cpp
        src/Platform/Window.cpp
        src/Renderer/Vulkan/VulkanInstance.cpp
        src/Renderer/Vulkan/VulkanInstance.h
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