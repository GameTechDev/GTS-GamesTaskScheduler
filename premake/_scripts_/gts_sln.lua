-- premake5.lua

workspace "gts"
    configurations { "Debug", "RelWithAssert", "Release" }
    platforms { "x86", "x64" }
    location ("../../_build/gts/" .. _ACTION .. (_ARGS[1] and ("/" .. _ARGS[1]) or ("")))
    startproject "gts"
    
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
        defines { "_HAS_EXCEPTIONS=0" }
        linkoptions { "-IGNORE:4221" }
        systemversion "latest"
        
    filter { "action:gmake" }
        buildoptions { "-pedantic", "-Wno-error=class-memaccess", "-msse2" }
        
    filter { "action:xcode4" }
        buildoptions { "-pedantic", "-Wno-error=class-memaccess", "-msse2" }
       
    filter {"configurations:Debug"}
        defines { "_DEBUG" }
        symbols "On"

    filter { "configurations:RelWithDebInfo" }
        defines { "NDEBUG", "GTS_USE_FATAL_ASSERT" }
        symbols "On"
        optimize "Full"

    filter { "configurations:Release" }
        defines { "NDEBUG" }
        symbols "On"
        optimize "Full"
 
    include "_intermediates_/gts"
