cd _scripts_
premake5.exe --file=gts_perf_tests_sln.lua vs2015 msvc
premake5.exe --file=gts_perf_tests_sln.lua vs2017 msvc
premake5.exe --file=gts_perf_tests_sln.lua vs2015 clang
premake5.exe --file=gts_perf_tests_sln.lua vs2017 clang
premake5.exe --file=gts_perf_tests_sln.lua xcode4
premake5.exe --file=gts_perf_tests_sln.lua gmake
cd ..