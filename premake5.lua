workspace "Quiet"
    architecture "x86_64"
    startproject "Quiet-Editor"

    configurations{
        "Debug",
        "Release",
        "Dist"
    }

    flags{
        "MultiProcessorCompile"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

--include directories relative to root folder (solution directory)
IncludeDir = {}
IncludeDir["GLFW"] = "Quiet-Engine/vendor/GLFW/include"
IncludeDir["Glad"] = "Quiet-Engine/vendor/Glad/include"
IncludeDir["ImGui"] = "Quiet-Engine/vendor/ImGui"
IncludeDir["glm"] = "Quiet-Engine/vendor/glm"
IncludeDir["stb"] = "Quiet-Engine/vendor/stb"

--include vendor dependencies group
group "Dependencies"
    include "Quiet-Engine/vendor"

group ""
---------------------------------------------
--- QUIET ENGINE ----------------------------
---------------------------------------------
project "Quiet-Engine"
    location "Quiet-Engine"
    kind "StaticLib"
	language "C++"
	cppdialect "C++17"
	staticruntime "on"

    targetdir ("_bin/" .. outputdir .. "/%{prj.name}")
    objdir ("_int/" .. outputdir .. "/%{prj.name}")

    pchheader "qtpch.h"
    pchsource "Quiet-Engine/src/qtpch.cpp"

    files{
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
        "%{prj.name}/vendor/stb/**.h",
        "%{prj.name}/vendor/stb/**.cpp",
        "%{prj.name}/vendor/glm/glm/**.hpp",
        "%{prj.name}/vendor/glm/glm/**.inl"
    }

    defines{
        "_CRT_SECURE_NO_WARNINGS"
    }

    includedirs{
		"%{prj.name}/src",
        "%{prj.name}/vendor/spdlog/include",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.stb}"
	}

    links{
        "GLFW",
        "Glad",
        "ImGui",
        "opengl32.lib"
    }

    filter "system:windows"
        systemversion "latest"

        defines{
            "QT_BUILD_DLL",
            "GLFW_INCLUDE_NONE"
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

---------------------------------------------
--- QUIET EDITOR ----------------------------
---------------------------------------------
project "Quiet-Editor"
    location "Quiet-Editor"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("_bin/" .. outputdir .. "/%{prj.name}")
    objdir ("_int/" .. outputdir .. "/%{prj.name}")

    files{
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs{
        "Quiet-Engine/src",
        "Quiet-Engine/vendor/",
        "Quiet-Engine/vendor/spdlog/include",
        "%{IncludeDir.glm}"
    }

    links{
        "Quiet-Engine"
    }

    filter "system:windows"
        systemversion "latest"

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

---------------------------------------------
--- SANDBOX APPLICATION ---------------------
---------------------------------------------
project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    staticruntime "on"

    targetdir ("_bin/" .. outputdir .. "/%{prj.name}")
    objdir ("_int/" .. outputdir .. "/%{prj.name}")

    files{
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs{
        "Quiet-Engine/src",
        "Quiet-Engine/vendor/",
        "Quiet-Engine/vendor/spdlog/include",
        "%{IncludeDir.glm}"
    }

    links{
        "Quiet-Engine"
    }

    filter "system:windows"
        systemversion "latest"

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