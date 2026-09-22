add_executable(Quiet_Cooker)

target_sources(Quiet_Cooker
    PRIVATE
        src/main.cpp
        src/QMeshWriter.h
        src/QMeshWriter.cpp
)

target_link_libraries(Quiet_Cooker
    PRIVATE
        Quiet::Engine
        meshoptimizer
)