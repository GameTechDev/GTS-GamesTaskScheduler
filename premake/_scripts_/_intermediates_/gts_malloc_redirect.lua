project "gts_malloc_redirect"
    flags "FatalWarnings"
    kind "SharedLib"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    libdirs { "%{prj.location}%{cfg.targetdir}" }
    links { "detours", "gts_malloc_shared" }
    defines { "GTS_MALLOC_SHARED_LIB", "GTS_MALLOC_REDIRECT_LIB_EXPORT", "GTS_MALLOC_REDIRECT" }
    includedirs {
        "../../../external_dependencies/Detours/include",
        "../../../source/gts/include"
    }
    files {
        "../../../source/gts/include/gts/malloc/**.*",
        "../../../source/gts/source/malloc/GtsMallocRedirect.cpp"
    }
    filter { "platforms:x64", "system:Windows" }
        prebuildcommands {
            "cd ../../../../external_dependencies/Detours/src",
            "set DETOURS_TARGET_PROCESSOR=X64",
            "set __VCVARSALL_TARGET_ARCH=x64",
            "set __VCVARSALL_HOST_ARCH=x64",
            "nmake",
            "cd ..",
            "copy lib.X64/detours.lib %{prj.location}%{cfg.targetdir}"
        }
    filter { "platforms:x86", "system:Windows" }
        prebuildcommands {
            "cd ../../../../external_dependencies/Detours/src",
            "set DETOURS_TARGET_PROCESSOR=X86",
            "set __VCVARSALL_TARGET_ARCH=X86",
            "set __VCVARSALL_HOST_ARCH=X86",
            "nmake",
            "cd ..",
            "copy lib.X86/detours.lib %{prj.location}%{cfg.targetdir}"
        }
