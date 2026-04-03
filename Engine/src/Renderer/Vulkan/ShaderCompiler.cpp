#include "ShaderCompiler.h"

#include <cstdlib>
#include <filesystem>
#include <string>


namespace Quiet::Renderer::Vulkan
{
    bool ShaderCompiler::CompileFile(const std::string& sourcePath, const std::string& outputPath)
    {
        const std::string command = "glslc \"" + sourcePath + "\" -o \"" + outputPath + "\"";
        return std::system(command.c_str()) == 0;
    }

    bool ShaderCompiler::CompileDirectory(const std::string& sourceDirectory, const std::string& outputDirectory)
    {
        bool allSucceeded = true;

        if (!std::filesystem::exists(sourceDirectory))
        {
            return false;
        }

        std::filesystem::create_directories(outputDirectory);

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

            if (!CompileFile(sourcePath, outputPath))
            {
                allSucceeded = false;
            }
        }

        return allSucceeded;
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
