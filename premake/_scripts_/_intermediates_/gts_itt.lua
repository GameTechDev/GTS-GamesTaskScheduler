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
    
    filter { "platforms:x86"}
        links { "../../../external_dependencies/itt/lib32/libittnotify.lib" }

    filter { "platforms:x64"}
        links { "../../../external_dependencies/itt/lib64/libittnotify.lib" }