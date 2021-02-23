project "gts_malloc_shared"
    flags "FatalWarnings"
    kind "SharedLib"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    defines { "GTS_MALLOC_SHARED_LIB", "GTS_MALLOC_SHARED_LIB_EXPORT" }
    includedirs {
        "../../../source/gts/include"
    }
    files {
        "../../../source/config.h",
        "../../../source/gts/include/gts/platform/**.*",
        "../../../source/gts/include/gts/analysis/**.*",
        "../../../source/gts/include/gts/containers/**.*",
        "../../../source/gts/include/gts/malloc/**.*",
        "../../../source/gts/source/platform/**.*",
        "../../../source/gts/source/analysis/**.*",
        "../../../source/gts/source/containers/**.*",
        "../../../source/gts/source/malloc/**.*"
    }
    
    filter { "configurations:RelWithTracy" }
        links { "tracy" }
        includedirs {
            "../../../external_dependencies/tracy"
        }
