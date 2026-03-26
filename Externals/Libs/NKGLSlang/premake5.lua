project "GLSlang"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    staticruntime "off"

    targetdir ( "../../../build/" .. outputdir .. "/%{prj.name}" )
    objdir ( "../../../build-int/" .. outputdir .. "/%{prj.name}" )

    files {
        "./**.h",
        "./**.hpp",
        "./**.c",
        "./**.cpp",
        "./glslan/**.h",
        "./glslan/**.hpp",
        "./glslan/**.c",
        "./glslan/**.cpp",
        "./StandAlone/**.h",
        "./StandAlone/**.hpp",
        "./StandAlone/**.c",
        "./StandAlone/**.cpp",
        "./SPIRV/**.h",
        "./SPIRV/**.hpp",
        "./SPIRV/**.c",
        "./SPIRV/**.cpp",
        "./External/pthreads/**.h",
        "./External/pthreads/**.hpp",
        "./External/pthreads/**.c",
        "./External/pthreads/**.cpp"
    }

    includedirs {
        "./glslan/Include",
        "./",
        "./StandAlone",
        "./External/pthreads/"
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
		defines { "_ALLOW_ITERATOR_DEBUG_LEVEL_MISMATCH " }

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter {}
