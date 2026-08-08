add_executable(Quiet_Editor)

target_sources(Quiet_Editor
    PRIVATE
        src/main.cpp
)

target_link_libraries(Quiet_Editor
    PRIVATE
        Quiet::Engine
)

if(WIN32)
    enable_language(RC)

    target_sources(Quiet_Editor
        PRIVATE
            Resources/Windows/QuietEditor.rc
    )
endif()

quiet_copy_engine_resources(Quiet_Editor)