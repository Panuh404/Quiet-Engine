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

target_include_directories(Quiet_Sandbox PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../Samples")

quiet_copy_engine_resources(Quiet_Sandbox)
