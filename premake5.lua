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

--include premake file for vendor 
include "Quiet/vendor"

project "Quiet"
    location "Quiet"
    kind "SharedLib"
    language "C++"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "qtpch.h"
    pchsource "Quiet/src/qtpch.cpp"

    files{
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs{
		"%{prj.name}/src",
        "%{prj.name}/vendor/spdlog/include",
        "%{IncludeDir.GLFW}"
	}

    links{
        "GLFW",
        "opengl32.lib"
    }

    filter "system:windows"
        cppdialect "C++17"
        staticruntime "On"
        systemversion "latest"

        defines{
            "QT_PLATFORM_WINDOWS",
            "QT_BUILD_DLL"
        }

        postbuildcommands {
			("{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/Sandbox")
		}

    filter "configurations:Debug"
        defines "QT_DEBUG"
        buildoptions "/MDd"
        symbols "on"

    filter "configurations:Release"
        defines "QT_RELEASE"
        buildoptions "/MD"
        optimize "on"

    filter "configurations:Dist"
        defines "QT_DIST"
        buildoptions "/MD"
        optimize "on"

project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files{
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs{
        "Quiet/src",
        "Quiet/vendor/spdlog/include"
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
        buildoptions "/MDd"
        symbols "on"

    filter "configurations:Release"
        defines "QT_RELEASE"
        buildoptions "/MD"
        optimize "on"

    filter "configurations:Dist"
        defines "QT_DIST"
        buildoptions "/MD"
        optimize "on"