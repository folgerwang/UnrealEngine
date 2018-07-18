@echo off
REM glslang
REM set LINUX_ROOT=D:\DevC\CarefullyRedist\HostWin64\Linux_x64\v8_clang-3.9.0-centos7\x86_64-unknown-linux-gnu\
set LINUX_ROOT=D:\DevC\CarefullyRedist\HostWin64\Linux_x64\v7_clang-3.7.0_ld-2.24_glibc-2.12.2\toolchain
pushd glslang\projects

	p4 edit %THIRD_PARTY_CHANGELIST% ..\lib\...

	REM vs2015 x64
	pushd vs2015
	msbuild glslang.sln /target:Clean,glslang_lib /p:Platform=x64;Configuration="Debug"
	msbuild glslang.sln /target:Clean,glslang_lib /p:Platform=x64;Configuration="Release"
	popd

	REM Linux (only if LINUX_ROOT is defined)
	set CheckLINUX_ROOT=%LINUX_ROOT%
	if "%CheckLINUX_ROOT%"=="" goto SkipLinux

	pushd Linux
ECHO ***********************************
ECHO LINUX
ECHO ***********************************
	call CrossCompile.bat
	popd

:SkipLinux

popd
