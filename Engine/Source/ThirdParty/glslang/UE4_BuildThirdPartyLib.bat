@echo off
REM glslang
pushd glslang\projects

	p4 edit %THIRD_PARTY_CHANGELIST% ..\lib\...

	REM vs2015 x64
	pushd vs2015
	msbuild glslang.sln /target:Clean,glslang_lib /p:Platform=x64;Configuration="Debug"
	msbuild glslang.sln /target:Clean,glslang_lib /p:Platform=x64;Configuration="Release"
	popd
popd
