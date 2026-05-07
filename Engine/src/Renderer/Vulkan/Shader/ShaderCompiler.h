#pragma once

#include <string>

namespace Quiet::Renderer::Vulkan
{
    class ShaderCompiler
    {
    public:
        static bool CompileFile(const std::string& sourcePath, const std::string& outputPath);
        static bool CompileDirectory(const std::string& sourceDirectory, const std::string& outputDirectory);

    private:
        static bool IsShaderSourceFile(const std::string& path);
        static std::string BuildOutputPath(const std::string& sourcePath, const std::string& outputDirectory);
    };
}