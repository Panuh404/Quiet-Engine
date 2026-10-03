add_executable(Quiet_Cooker)

target_sources(Quiet_Cooker
    PRIVATE
        src/CookCache.h
        src/CookCache.cpp
        src/main.cpp
        src/QMeshWriter.h
        src/QMeshWriter.cpp
        src/QTextureCompressor.h
        src/QTextureCompressor.cpp
        src/QTextureMipGenerator.h
        src/QTextureMipGenerator.cpp
        src/QTextureWriter.h
        src/QTextureWriter.cpp
)

target_link_libraries(Quiet_Cooker
    PRIVATE
        Quiet::Engine
        meshoptimizer
        libzstd_static
        ktx
        Quiet_CompressonatorCore
        miniaudio
)