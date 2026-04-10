project "ImGui"
    kind "StaticLib"
    CppInfo()
    staticruntime "off"

    targetdir ( "../../../build/" .. outputdir .. "/%{prj.name}" )
    objdir ( "../../../build-int/" .. outputdir .. "/%{prj.name}" )

    files {
        "./**.h",
        "./**.hpp",
        "./**.cpp",
        "./**.c",
        "./**.natvis",
        "./**.natstepfilter"
    }

    includedirs {
        "./"
    }

    defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE"
	}

    filter "system:windows"
        systemversion "latest"

    filter "system:macosx"
        xcodebuildsettings
        {
            ["MACOSX_DEPLOYMENT_TARGET"] = "10.15",
            ["USeModernBuildSystem"] = "NO"
        }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter {}
