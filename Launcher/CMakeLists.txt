add_executable(Quiet_Launcher)

target_sources(Quiet_Launcher
        PRIVATE
        src/main.cpp
        src/LauncherApplication.h
        src/LauncherApplication.cpp
)

target_link_libraries(Quiet_Launcher
        PRIVATE
        Quiet::Engine
)

quiet_copy_engine_resources(Quiet_Launcher)