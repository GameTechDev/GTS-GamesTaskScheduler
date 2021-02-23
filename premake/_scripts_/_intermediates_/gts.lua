project "gts"
    flags "FatalWarnings"
    kind "StaticLib"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    includedirs {
        "../../../source/gts/include",
    }
    files {
        "../../../source/config.h",
        "../../../source/gts/include/**.*",
        "../../../source/gts/source/**.*"
    }
    
    filter { "configurations:RelWithTracy or configurations:DebugWithTracy" }
        links { "tracy" }
        includedirs {
            "../../../external_dependencies/tracy"
        }
