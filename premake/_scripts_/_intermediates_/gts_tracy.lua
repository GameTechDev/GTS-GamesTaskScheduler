project "gts"
    flags "FatalWarnings"
    kind "StaticLib"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links "tracy"
    includedirs {
        "../../../external_dependencies/itt",
        "../../../external_dependencies/rad_tm",
        "../../../external_dependencies/tracy",
        "../../../source/gts/include",
    }
    files {
        "../../../source/config.h",
        "../../../source/gts/include/**.*",
        "../../../source/gts/source/**.*"
    }