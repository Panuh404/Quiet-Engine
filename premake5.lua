workspace "Quiet"
    architecture "x64"
    startproject "Sandbox"

    configurations{
        "Debug",
        "Release",
        "Dist"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

--include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "Quiet/vendor/GLFW/include"
IncludeDir["Glad"] = "Quiet/vendor/Glad/include"
IncludeDir["ImGui"] = "Quiet/vendor/ImGui"
IncludeDir["glm"] = "Quiet/vendor/glm"

--include premake file for vendor 
include "Quiet/vendor"

project "Quiet"
    location "Quiet"
    kind "SharedLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "qtpch.h"
    pchsource "Quiet/src/qtpch.cpp"

    files{
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
        "%{prj.name}/vendor/glm/glm/**.hpp",
        "%{prj.name}/vendor/glm/glm/**.inl"
    }

    includedirs{
		"%{prj.name}/src",
        "%{prj.name}/vendor/spdlog/include",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
	}

    defines{
        "_CRT_SECURE_NO_WARNINGS"
    }

    links{
        "GLFW",
        "Glad",
        "ImGui",
        "opengl32.lib"
    }

    filter "system:windows"
        cppdialect "C++17"
        staticruntime "On"
        systemversion "latest"

        defines{
            "QT_PLATFORM_WINDOWS",
            "QT_BUILD_DLL",
            "GLFW_INCLUDE_NONE"
        }

        postbuildcommands {
			("{COPY} %{cfg.buildtarget.relpath} \"../bin/" .. outputdir .. "/Sandbox/\"")
		}

    filter "configurations:Debug"
        defines "QT_DEBUG"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        defines "QT_RELEASE"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        defines "QT_DIST"
        runtime "Release"
        optimize "on"

project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files{
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs{
        "Quiet/src",
        "Quiet/vendor/",
        "Quiet/vendor/spdlog/include",
        "%{IncludeDir.glm}"
    }

    links{
        "Quiet"
    }

    filter "system:windows"
        systemversion "latest"

        defines{
            "QT_PLATFORM_WINDOWS"
        }

    filter "configurations:Debug"
        defines "QT_DEBUG"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        defines "QT_RELEASE"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        defines "QT_DIST"
        runtime "Release"
        optimize "on"