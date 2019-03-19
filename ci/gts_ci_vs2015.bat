cd ../premake/visual_studio
call gts_unit_tests_vs.bat
cd ..
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" _build/gts_unit_tests/vs2015/msvc/gts_unit_tests.vcxproj /p:Configuration=Debug /p:Platform=x86
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" _build/gts_unit_tests/vs2015/msvc/gts_unit_tests.vcxproj /p:Configuration=Release /p:Platform=x86
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" _build/gts_unit_tests/vs2015/msvc/gts_unit_tests.vcxproj /p:Configuration=Debug /p:Platform=x64
"C:/Program Files (x86)/MSBuild/14.0/Bin/msbuild.exe" _build/gts_unit_tests/vs2015/msvc/gts_unit_tests.vcxproj /p:Configuration=Release /p:Platform=x64