-- premake5.lua

workspace "gts_examples"
    configurations {  "Debug", "DebugWithInstrument", "RelWithInstrument", "Release" }
    platforms { "x86", "x64" }
    location ("../../_build/gts_examples/" .. _ACTION .. (_ARGS[1] and ("/" .. _ARGS[1]) or ("")))
    startproject "fib_example"

    filter { "platforms:x86"}
        architecture "x86"

    filter { "platforms:x64"}
        architecture "x86_64"
        
    filter { "action:vs*" }        
        defines { "_HAS_EXCEPTIONS=0" }
        buildoptions "/EHsc"
        
    filter { "action:vs2017" }        
        buildoptions "/permissive-"
        
    filter { "action:gmake" }
        buildoptions "-pedantic -fno-exceptions"
        
    filter { "action:xcode4" }
        buildoptions "-pedantic -fno-exceptions"
       
    filter {"configurations:Debug"}
        symbols "On"
        defines { "_DEBUG" }
        
    filter { "configurations:DebugWithInstrument" }
        defines { "_DEBUG", "GTS_ENABLE_INSTRUMENTER", "GTS_ENABLE_CONCRT_LOGGER", "GTS_ENABLE_CONCRT_LOGGER" }
        symbols "On"

    filter { "configurations:RelWithInstrument" }
        defines { "NDEBUG", "GTS_USE_FATAL_ASSERT", "GTS_ENABLE_INSTRUMENTER", "GTS_ENABLE_CONCRT_LOGGER" }
        symbols "On"
        optimize "Full"

    filter { "configurations:Release" }
        defines { "NDEBUG" }
        optimize "Full"
        
    include "_intermediates_/gts"
        
project "1_initialization"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts" }
    includedirs {
        "../../source/gts/include",
        "../../source/gts/examples/micro_scheduler/1_initialization/include"
    }
    files {
        "../../source/gts/examples/micro_scheduler/1_initialization/**.*"
    }
    
project "2_fork_join"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts" }
    includedirs {
        "../../source/gts/include",
        "../../source/gts/examples/micro_scheduler/2_fork_join/include"
    }
    files {
        "../../source/gts/examples/micro_scheduler/2_fork_join/**.*"
    }
    
project "3_parallel_patterns"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts" }
    includedirs {
        "../../source/gts/include",
        "../../source/gts/examples/micro_scheduler/3_parallel_patterns/include"
    }
    files {
        "../../source/gts/examples/micro_scheduler/3_parallel_patterns/**.*"
    }
    
project "4_debugging_and_intrumentation"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts" }
    includedirs {
        "../../source/gts/include",
        "../../source/gts/examples/micro_scheduler/4_debugging_and_intrumentation/include"
    }
    files {
        "../../source/gts/examples/micro_scheduler/4_debugging_and_intrumentation/**.*"
    }
    
project "5_miscellaneous"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts" }
    includedirs {
        "../../source/gts/include",
        "../../source/gts/examples/micro_scheduler/5_miscellaneous/include"
    }
    files {
        "../../source/gts/examples/micro_scheduler/5_miscellaneous/**.*"
    } 
    