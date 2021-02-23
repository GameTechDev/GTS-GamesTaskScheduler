project "1_dynamic_micro_scheduler"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts" }
    includedirs {
        "../../../source/gts/include",
        "../../../source/gts/examples/3_macro_scheduler/1_dynamic_micro_scheduler/include"
    }
    files {
        "../../../source/gts/examples/3_macro_scheduler/1_dynamic_micro_scheduler/**.*"
    }
    filter{ "system:linux" }
        links { "pthread" }    
    
project "2_critically_aware_task_scheduling"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts" }
    includedirs {
        "../../../source/gts/include",
        "../../../source/gts/examples/3_macro_scheduler/2_critically_aware_task_scheduling/include"
    }
    files {
        "../../../source/gts/examples/3_macro_scheduler/2_critically_aware_task_scheduling/**.*"
    }
    filter{ "system:linux" }
        links { "pthread" }
