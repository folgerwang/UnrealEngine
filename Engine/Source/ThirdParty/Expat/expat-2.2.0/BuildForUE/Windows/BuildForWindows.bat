REM ECHO OFF

REM Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

setlocal

set PATH_TO_CMAKE_FILE=%CD%\..\..

REM Temporary build directories (used as working directories when running CMake)
set VS2015_X86_PATH="%PATH_TO_CMAKE_FILE%\Win32\VS2015\Build"
set VS2015_X64_PATH="%PATH_TO_CMAKE_FILE%\Win64\VS2015\Build"

REM MSBuild Directory
for /f "skip=2 tokens=2,*" %%R in ('reg.exe query "HKLM\SOFTWARE\Microsoft\MSBuild\ToolsVersions\14.0" /v MSBuildToolsPath') do SET _msbuild=%%S
if not exist "%_msbuild%msbuild.exe" goto MSBuildMissing


REM Build for VS2015 (32-bit)
echo Generating Expat solution for VS2015 (32-bit)...
if exist "%VS2015_X86_PATH%" (rmdir "%VS2015_X86_PATH%" /s/q)
mkdir "%VS2015_X86_PATH%"
cd "%VS2015_X86_PATH%"
cmake -G "Visual Studio 14 2015" -DCMAKE_SUPPRESS_REGENERATION=1 -DBUILD_tools=0 -DBUILD_examples=0 -DBUILD_tests=0 -DBUILD_shared=0 -DSKIP_PRE_BUILD_COMMAND=1 %PATH_TO_CMAKE_FILE%
echo Building Expat solution for VS2015 (32-bit, Debug)...
"%_msbuild%msbuild.exe" expat.vcxproj /t:build /p:Configuration=Debug
echo Building Expat solution for VS2015 (32-bit, Release)...
"%_msbuild%msbuild.exe" expat.vcxproj /t:build /p:Configuration=Release
cd "%PATH_TO_CMAKE_FILE%"
xcopy /y/s/i "%VS2015_X86_PATH%\Debug" "%VS2015_X86_PATH%\..\Debug"
xcopy /y/s/i "%VS2015_X86_PATH%\Release" "%VS2015_X86_PATH%\..\Release"
rmdir "%VS2015_X86_PATH%" /s/q

REM Build for VS2015 (64-bit)
echo Generating Expat solution for VS2015 (64-bit)...
if exist "%VS2015_X64_PATH%" (rmdir "%VS2015_X64_PATH%" /s/q)
mkdir "%VS2015_X64_PATH%"
cd "%VS2015_X64_PATH%"
cmake -G "Visual Studio 14 2015 Win64" -DCMAKE_SUPPRESS_REGENERATION=1 -DBUILD_tools=0 -DBUILD_examples=0 -DBUILD_tests=0 -DBUILD_shared=0 -DSKIP_PRE_BUILD_COMMAND=1 %PATH_TO_CMAKE_FILE%
echo Building Expat solution for VS2015 (64-bit, Debug)...
"%_msbuild%msbuild.exe" expat.vcxproj /t:build /p:Configuration=Debug
echo Building Expat solution for VS2015 (64-bit, Release)...
"%_msbuild%msbuild.exe" expat.vcxproj /t:build /p:Configuration=Release
cd "%PATH_TO_CMAKE_FILE%"
xcopy /y/s/i "%VS2015_X64_PATH%\Debug" "%VS2015_X64_PATH%\..\Debug"
xcopy /y/s/i "%VS2015_X64_PATH%\Release" "%VS2015_X64_PATH%\..\Release"
rmdir "%VS2015_X64_PATH%" /s/q
goto Exit

:MSBuildMissing
echo MSBuild not found. Please check your Visual Studio install and try again.
goto Exit

:Exit
endlocal
