add_executable(Quiet_Cooker)

target_sources(Quiet_Cooker
    PRIVATE
        src/main.cpp

        src/Package/PackageBuilder.h
        src/Package/PackageBuilder.cpp
        src/Package/PackageLayout.h
        src/Package/PackageLayout.cpp

        src/Core/CookCache.h
        src/Core/CookCache.cpp
        src/Core/CookContext.h

        src/Audio/AudioCooker.h
        src/Audio/AudioCooker.cpp

        src/Script/ScriptCooker.h
        src/Script/ScriptCooker.cpp

        src/Mesh/MeshCooker.h
        src/Mesh/MeshCooker.cpp
        src/Mesh/QMeshWriter.h
        src/Mesh/QMeshWriter.cpp

        src/Texture/QTextureCompressor.h
        src/Texture/QTextureCompressor.cpp
        src/Texture/QTextureMipGenerator.h
        src/Texture/QTextureMipGenerator.cpp
        src/Texture/QTextureWriter.h
        src/Texture/QTextureWriter.cpp
        src/Texture/TextureCooker.h
        src/Texture/TextureCooker.cpp
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

target_include_directories(Quiet_Cooker
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)