-- premake5.lua

workspace "gts_unit_tests"
    configurations { "Debug", "DebugWithInstrument", "RelWithInstrument", "Release" }
    platforms { "x86", "x64" }
    location ("../../_build/gts_unit_tests/" .. _ACTION .. (_ARGS[1] and ("/" .. _ARGS[1]) or ("")))
    startproject "gts_unit_tests"
    
    warnings "Extra"
    
    if(_ARGS[1] == "clang") then
        toolset "msc-llvm-vs2014"
    else
        flags "FatalWarnings"
    end

    filter { "platforms:x86"}
        architecture "x86"

    filter { "platforms:x64"}
        architecture "x86_64"
        
    filter { "action:vs*" }        
        defines { "_HAS_EXCEPTIONS=0" , "_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING", "_ITERATOR_DEBUG_LEVEL=0" }
        buildoptions "/EHsc"
        
    filter { "action:gmake" }
        buildoptions "-pedantic -fno-exceptions"
        
    filter { "action:xcode4" }
        buildoptions "-pedantic -fno-exceptions"
       
    filter {"configurations:Debug"}
        defines { "_DEBUG"}
        symbols "On"
        
    filter { "configurations:DebugWithInstrument" }
        defines { "_DEBUG", "GTS_ENABLE_INSTRUMENTER", "GTS_ENABLE_CONCRT_LOGGER", "GTS_ENABLE_CONCRT_LOGGER" }
        symbols "On"

    filter { "configurations:RelWithInstrument" }
        defines { "NDEBUG", "GTS_USE_ASSERTS", "GTS_ENABLE_INSTRUMENTER", "GTS_ENABLE_CONCRT_LOGGER" }
        symbols "On"
        optimize "Full"

    filter { "configurations:Release" }
        defines { "NDEBUG", "GTS_USE_ASSERTS" }
        optimize "Full"
        
    include "_intermediates_/gtest"
    include "_intermediates_/gmock"
    include "_intermediates_/gts"
      
project "gts_test_dll"
    kind "SharedLib"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
	defines { "TEST_DLL_EXPORTS" }
    links { "gts" }
    includedirs {
        "../../source/gts/include",
        "../../source/gts/test_dll/include"
    }
    files {
        "../../source/gts/test_dll/include/**.*",
        "../../source/gts/test_dll/source/**.*"
    }
	  
project "gts_unit_tests"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts", "gts_test_dll", "googletest", "googlemock" }
    includedirs {
        "../../external_dependencies/googletest/googlemock/include",
        "../../external_dependencies/googletest/googletest/include",
        "../../source/gts/include",
        "../../source/gts/source",
        "../../source/gts/test_unit/include",
		"../../source/gts/test_dll/include"
    }
    files {
        "../../source/gts/test_unit/include/**.*",
        "../../source/gts/test_unit/source/**.*"
    }
    postbuildcommands {
        "\"%{prj.location}%{cfg.buildcfg}_%{cfg.architecture}/gts_unit_tests.exe\""
    }