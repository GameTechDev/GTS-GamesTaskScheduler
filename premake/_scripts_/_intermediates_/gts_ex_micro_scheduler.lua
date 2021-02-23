project "1_initialization"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts" }
    includedirs {
        "../../../source/gts/include",
        "../../../source/gts/examples/1_micro_scheduler/1_initialization/include"
    }
    files {
        "../../../source/gts/examples/1_micro_scheduler/1_initialization/**.*"
    }
    filter{ "system:linux" }
        links { "pthread" }    
    
project "2_fork_join"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts" }
    includedirs {
        "../../../source/gts/include",
        "../../../source/gts/examples/1_micro_scheduler/2_fork_join/include"
    }
    files {
        "../../../source/gts/examples/1_micro_scheduler/2_fork_join/**.*"
    }
    filter{ "system:linux" }
        links { "pthread" }
    
project "3_parallel_patterns"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts" }
    includedirs {
        "../../../source/gts/include",
        "../../../source/gts/examples/1_micro_scheduler/3_parallel_patterns/include"
    }
    files {
        "../../../source/gts/examples/1_micro_scheduler/3_parallel_patterns/**.*"
    }
    filter{ "system:linux" }
        links { "pthread" }
    
project "4_antipatterns"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts" }
    includedirs {
        "../../../source/gts/include",
        "../../../source/gts/examples/1_micro_scheduler/4_antipatterns/include"
    }
    files {
        "../../../source/gts/examples/1_micro_scheduler/4_antipatterns/**.*"
    }
    filter{ "system:linux" }
        links { "pthread" }
