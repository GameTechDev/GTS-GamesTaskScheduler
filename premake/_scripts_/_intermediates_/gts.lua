project "gts"
    kind "StaticLib"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    includedirs {
        "../../../external_dependencies/itt",
        "../../../external_dependencies/rad_tm",
        "../../../source/gts/include",
    }
    files {
        "../../../source/gts/include/**.*",
        "../../../source/gts/source/**.*"
    }
