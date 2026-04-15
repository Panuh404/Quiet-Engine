# SANDBOX
add_executable(Quiet_Sandbox)

target_sources(Quiet_Sandbox
    PRIVATE
        src/main.cpp
)

target_link_libraries(Quiet_Sandbox
    PRIVATE
        Quiet::Engine
)

quiet_copy_engine_resources(Quiet_Sandbox)