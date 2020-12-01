project "tracy"
    kind "StaticLib"
    language "C++"
    targetdir "%{prj.location}/%{cfg.buildcfg}_%{cfg.architecture}"
    includedirs {
        "../../../external_dependencies/tracy"
    }
    files {
        "../../../external_dependencies/tracy/Tracy.hpp",
        "../../../external_dependencies/tracy/TracyClient.cpp",
    }
