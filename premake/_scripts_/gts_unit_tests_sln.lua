-- premake5.lua

workspace "gts_unit_tests"
    configurations { "Debug", "DebugWithGtsMalloc", "DebugWithLogger", "DebugWithTracy", "RelWithLogger", "RelWithTracy", "ReleaseWithGtsMalloc", "Release" }
    platforms { "x86", "x64" }
    location ("../../_build/gts_unit_tests/" .. _ACTION .. (_ARGS[1] and ("/" .. _ARGS[1]) or ("")))
    startproject "gts_unit_tests"

    files {
        "../../source/gts/gts.natvis"
    }

    warnings "Extra"
    exceptionhandling "Off"
    rtti "Off"

    if(_ARGS[1] == "clang") then
        toolset "msc-llvm-vs2014"
    end

    filter { "platforms:x86"}
        architecture "x86"

    filter { "platforms:x64"}
        architecture "x86_64"

    filter { "action:vs*" }        
        defines { "_HAS_EXCEPTIONS=0" , "_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING", "_ITERATOR_DEBUG_LEVEL=0" }
        linkoptions { "-IGNORE:4221" }
        systemversion "latest"

    filter { "action:gmake" }
        buildoptions { "-pedantic", "-Wno-class-memaccess", "-msse2" }

    filter { "action:xcode4" }
        buildoptions { "-pedantic", "-Wno-class-memaccess", "-msse2" }

    filter {"configurations:Debug"}
        defines { "_DEBUG", "GTS_USE_INTERNAL_ASSERTS", "_CRTDBG_MAP_ALLOC"}
        symbols "On"

    filter {"configurations:DebugWithGtsMalloc"}
        defines { "_DEBUG", "GTS_USE_INTERNAL_ASSERTS", "GTS_USE_GTS_MALLOC", "GTS_MALLOC_DEUBG" }
        symbols "On"
        
    filter { "configurations:DebugWithLogger" }
        defines { "_DEBUG", "GTS_USE_INTERNAL_ASSERTS", "_CRTDBG_MAP_ALLOC", "GTS_TRACE_CONCURRENT_USE_LOGGER=1" }
        symbols "On"

    filter { "configurations:DebugWithTracy" }
        defines { "_DEBUG", "GTS_USE_INTERNAL_ASSERTS", "_CRTDBG_MAP_ALLOC", "GTS_TRACE_USE_TRACY=1", "TRACY_ENABLE" }
        symbols "On"

    filter { "configurations:RelWithLogger" }
        defines { "NDEBUG", "GTS_USE_ASSERTS", "GTS_USE_INTERNAL_ASSERTS", "GTS_TRACE_CONCURRENT_USE_LOGGER=1" }
        symbols "On"
        optimize "Full"

    filter { "configurations:RelWithTracy" }
        defines { "NDEBUG", "GTS_USE_ASSERTS", "GTS_USE_INTERNAL_ASSERTS", "GTS_TRACE_USE_TRACY=1", "TRACY_ENABLE" }
        symbols "On"
        optimize "Full"

    filter { "configurations:Release" }
        defines { "NDEBUG", "GTS_USE_ASSERTS", "GTS_USE_INTERNAL_ASSERTS" }
        symbols "On"
        optimize "Full"

    filter { "configurations:ReleaseWithGtsMalloc" }
        defines { "NDEBUG", "GTS_USE_ASSERTS", "GTS_USE_INTERNAL_ASSERTS", "GTS_USE_GTS_MALLOC" }
        symbols "On"
        optimize "Full"

    include "_intermediates_/gtest"
    include "_intermediates_/gmock"
    include "_intermediates_/gts"
    include "_intermediates_/tracy"
    include "_intermediates_/gts_malloc_shared"

if os.target() == "windows" then
    project "gts_test_dll"
        flags "FatalWarnings"
        kind "SharedLib"
        language "C++"
        targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
        defines { "TEST_DLL_EXPORTS" }
        links { "gts" }
        includedirs {
            "../../external_dependencies/tracy",
            "../../source/gts/include",
            "../../source/gts/test_dll/include"
        }
        files {
            "../../source/gts/test_dll/include/**.*",
            "../../source/gts/test_dll/source/**.*"
        }
end


project "gts_unit_tests"
    flags "FatalWarnings"
    kind "ConsoleApp"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "gts", "gts_malloc_shared", "googletest", "googlemock", "tracy" }
    includedirs {
        "../../external_dependencies/googletest/googlemock/include",
        "../../external_dependencies/googletest/googletest/include",
        "../../external_dependencies/tracy",
        "../../source/gts/include",
        "../../source/gts/source",
        "../../source/gts/test_unit/gts/include",
    }
    files {
        "../../source/gts/test_unit/gts/include/**.*",
        "../../source/gts/test_unit/gts/source/**.*"
    }
    
    filter{ "system:windows" }
        links { "gts_test_dll" }
        includedirs {
            "../../source/gts/test_dll/include"
        }
    filter{ "system:linux" }
        links { "pthread" }
