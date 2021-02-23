-- premake5.lua

workspace "gts_malloc_unit_tests"
    configurations { "Debug", "DebugWithAssert", "Release", "RelWithAssert" }
    platforms { "x86", "x64" }
    location ("../../_build/gts_malloc_unit_tests/" .. _ACTION .. (_ARGS[1] and ("/" .. _ARGS[1]) or ("")))
    startproject "gts_malloc_unit_tests"
    
    warnings "Extra"
    exceptionhandling "Off"
    rtti "Off"

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
        defines { "_HAS_EXCEPTIONS=0", "_ITERATOR_DEBUG_LEVEL=0" }
        linkoptions { "-IGNORE:4099", "-IGNORE:4221" }
        systemversion "latest"
        
    filter { "action:gmake" }
        buildoptions { "-pedantic", "-Wno-error=class-memaccess", "-msse2" }
        
    filter { "action:xcode4" }
        buildoptions { "-pedantic", "-Wno-error=class-memaccess", "-msse2" }
       
    filter {"configurations:Debug"}
        defines { "_DEBUG" }
        symbols "On"
        
    filter {"configurations:DebugWithAssert"}
        defines { "_DEBUG", "GTS_USE_INTERNAL_ASSERTS" }
        symbols "On"

    filter { "configurations:Release" }
        defines { "NDEBUG", "GTS_USE_ASSERTS" }
        symbols "On"
        optimize "Full"

    filter { "configurations:RelWithAssert" }
        defines { "NDEBUG", "GTS_USE_ASSERTS", "GTS_USE_INTERNAL_ASSERTS" }
        symbols "On"
        optimize "Full"
        
    include "_intermediates_/gtest"
    include "_intermediates_/gmock"
    include "_intermediates_/gts_malloc_static"
    include "_intermediates_/gts_malloc_shared"
    include "_intermediates_/gts_malloc_redirect"

project "test_malloc_proc"
    flags "FatalWarnings"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts_malloc_redirect", "gts_malloc_shared" }
    includedirs {
        "../../source/gts/include",
        "../../source/gts/test_malloc_proc/include"
    }
    files {
        "../../source/gts/test_malloc_proc/include/**.*",
        "../../source/gts/test_malloc_proc/source/**.*"
    }
    filter { "action:vs*" }
        linkoptions { "/IGNORE:4099" }

project "gts_malloc_shared_unit_tests"
    flags "FatalWarnings"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts_malloc_shared", "googletest", "googlemock" }
    includedirs {
        "../../external_dependencies/googletest/googlemock/include",
        "../../external_dependencies/googletest/googletest/include",
        "../../source/gts/include",
        "../../source/gts/source",
        "../../source/gts/test_unit/gts_malloc/include",
    }
    files {
        "../../source/gts/test_unit/gts_malloc/include/**.*",
        "../../source/gts/test_unit/gts_malloc/source/**.*"
    }
    postbuildcommands {
        "\"%{prj.location}%{cfg.buildcfg}_%{cfg.architecture}/gts_malloc_shared_unit_tests.exe\""
    }

project "gts_malloc_static_unit_tests"
    flags "FatalWarnings"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts_malloc_static", "googletest", "googlemock" }
    includedirs {
        "../../external_dependencies/googletest/googlemock/include",
        "../../external_dependencies/googletest/googletest/include",
        "../../source/gts/include",
        "../../source/gts/source",
        "../../source/gts/test_unit/gts_malloc/include",
    }
    files {
        "../../source/gts/test_unit/gts_malloc/include/**.*",
        "../../source/gts/test_unit/gts_malloc/source/**.*"
    }
    postbuildcommands {
        "\"%{prj.location}%{cfg.buildcfg}_%{cfg.architecture}/gts_malloc_static_unit_tests.exe\""
    }
