project "quick_start"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts" }
    includedirs {
        "../../../source/gts/include",
        "../../../source/gts/examples/0_quick_start/quick_start/include"
    }
    files {
        "../../../source/gts/examples/0_quick_start/quick_start/**.*"
    }
    filter{ "system:linux" }
        links { "pthread" }
