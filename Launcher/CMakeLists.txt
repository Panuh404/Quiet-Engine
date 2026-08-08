add_executable(Quiet_Launcher)

target_sources(Quiet_Launcher
    PRIVATE
        src/main.cpp
        src/LauncherApplication.h
        src/LauncherApplication.cpp
        src/LauncherRenderer.h
        src/LauncherRenderer.cpp
)

target_include_directories(Quiet_Launcher
    PRIVATE
        ${CMAKE_SOURCE_DIR}/Engine/src
        ${imgui_SOURCE_DIR}
)

target_link_libraries(Quiet_Launcher
    PRIVATE
        Quiet::Engine
)

if(WIN32)
    enable_language(RC)

    target_sources(Quiet_Launcher
        PRIVATE
            Resources/Windows/QuietLauncher.rc
    )
endif()

quiet_copy_engine_resources(Quiet_Launcher)