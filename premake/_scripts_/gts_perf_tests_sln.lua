-- premake5.lua

workspace "gts_perf_tests"
    configurations { "Debug", "DebugAnalyze", "RelWithDebInfo", "Release", "ReleaseITT", "ReleaseAnalyze" }
    platforms { "x86", "x64" }
    location ("../../_build/gts_perf_tests/" .. _ACTION .. (_ARGS[1] and ("/" .. _ARGS[1]) or ("")))
    startproject "gts_perf_tests"
    
    warnings "Extra"
    flags "FatalWarnings"

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
        
    filter {"configurations:DebugAnalyze"}
        defines { "GTS_ANALYZE" }
        symbols "On"
      
    filter { "configurations:RelWithDebInfo" }
        defines { "NDEBUG" }
        symbols "On"
        optimize "Full"

    filter { "configurations:Release" }
        defines { "NDEBUG" }
        optimize "Full"
		
	filter { "configurations:ReleaseITT" }
        defines { "NDEBUG", "GTS_USE_ITT", "GTS_ENABLE_INSTRUMENTER" }
        optimize "Full"
		
	filter { "configurations:ReleaseAnalyze" }
        defines { "NDEBUG", "GTS_ANALYZE" }
        optimize "Full"
        
    include "_intermediates_/gts_itt"
        
project "gts_perf_tests"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts" }
    includedirs {
        "../../source/gts/include",
        "../../source/gts/test_perf/include"
    }
    files {
        "../../source/gts/test_perf/include/**.*",
        "../../source/gts/test_perf/source/**.*"
    }