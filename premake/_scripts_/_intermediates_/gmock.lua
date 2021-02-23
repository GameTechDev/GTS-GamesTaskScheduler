project "googlemock"
    kind "StaticLib"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    links { "googletest" }
    includedirs {
        "../../../external_dependencies/googletest/googletest/include",
        "../../../external_dependencies/googletest/googletest",
        "../../../external_dependencies/googletest/googlemock/include",
        "../../../external_dependencies/googletest/googlemock"
    }

    files {
        "../../../external_dependencies/googletest/googlemock/src/gmock-all.cc",
        "../../../external_dependencies/googletest/googlemock/src/gmock_main.cc"
    }

    filter { "action:vs*" }
        defines { "_HAS_EXCEPTIONS=0", "WIN32_LEAN_AND_MEAN", "STRICT", "_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING", "GTEST_CATCH_EXCEPTIONS=0" }
        buildoptions { "/wd4800", "/wd4511", "/wd4512", "/wd4675" }