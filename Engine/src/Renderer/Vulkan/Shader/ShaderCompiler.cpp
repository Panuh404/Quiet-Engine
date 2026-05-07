#include "ShaderCompiler.h"

#include <cstdlib>
#include <filesystem>
#include <string>

#include "Quiet/Core/Log.h"

namespace Quiet::Renderer::Vulkan
{
    bool ShaderCompiler::CompileFile(const std::string& sourcePath, const std::string& outputPath)
    {
        const std::string command =
            "glslc \"" + sourcePath + "\" -o \"" + outputPath + "\"";

        const int result = std::system(command.c_str());

        if (result == 0)
        {
            QUIET_CORE_INFO("Compiled shader '{}' -> '{}'.", sourcePath, outputPath);
            return true;
        }

        QUIET_CORE_ERROR(
            "Failed to compile shader '{}' -> '{}'. Exit code={}.",
            sourcePath,
            outputPath,
            result
        );
        return false;
    }

    bool ShaderCompiler::CompileDirectory(const std::string& sourceDirectory, const std::string& outputDirectory)
    {
        try
        {
            bool allSucceeded = true;

            if (!std::filesystem::exists(sourceDirectory))
            {
                QUIET_CORE_WARN("Shader source directory does not exist: '{}'.", sourceDirectory);
                return false;
            }

            std::filesystem::create_directories(outputDirectory);

            std::uint32_t compileCount = 0;
            std::uint32_t failedCount = 0;

            for (const auto& entry : std::filesystem::directory_iterator(sourceDirectory))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                const std::string sourcePath = entry.path().string();
                if (!IsShaderSourceFile(sourcePath))
                {
                    continue;
                }

                const std::string outputPath = BuildOutputPath(sourcePath, outputDirectory);

                if (CompileFile(sourcePath, outputPath))
                {
                    ++compileCount;
                }
                else
                {
                    ++failedCount;
                    allSucceeded = false;
                }
            }

            QUIET_CORE_INFO(
                "Shader compile pass finished. compiled={} failed={} sourceDir='{}' outputDir='{}'.",
                compileCount,
                failedCount,
                sourceDirectory,
                outputDirectory
            );

            return allSucceeded;
        }
        catch (const std::exception& ex)
        {
            QUIET_CORE_ERROR(
                "Shader compile pass failed for sourceDir='{}' outputDir='{}'. Reason: {}",
                sourceDirectory,
                outputDirectory,
                ex.what()
            );
            return false;
        }
    }

    bool ShaderCompiler::IsShaderSourceFile(const std::string& path)
    {
        const std::filesystem::path shaderPath(path);
        const std::string ext = shaderPath.extension().string();

        return ext == ".vert" || ext == ".frag" || ext == ".comp";
    }

    std::string ShaderCompiler::BuildOutputPath(const std::string& sourcePath, const std::string& outputDirectory)
    {
        const std::filesystem::path inputPath(sourcePath);
        const std::string fileName = inputPath.filename().string();

        std::filesystem::path outPath = std::filesystem::path(outputDirectory) / (fileName + ".spv");
        return outPath.string();
    }
}
