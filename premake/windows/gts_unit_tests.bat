cd ..\_scripts_
..\windows\premake5.exe --file=gts_unit_tests_sln.lua vs2017 msvc
..\windows\premake5.exe --file=gts_unit_tests_sln.lua vs2019 msvc
..\windows\premake5.exe --file=gts_unit_tests_sln.lua vs2017 clang
..\windows\premake5.exe --file=gts_unit_tests_sln.lua vs2019 clang
..\windows\premake5.exe --file=gts_unit_tests_sln.lua gmake
cd ..