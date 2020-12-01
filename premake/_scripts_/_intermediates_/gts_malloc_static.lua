project "gts_malloc_static"
    flags "FatalWarnings"
    kind "StaticLib"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    includedirs {
        "../../../source/gts/include",
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
