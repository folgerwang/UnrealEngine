#!/bin/sh

cd "`dirname "$0"`/../../../.."

# Setup Mono
source Engine/Build/BatchFiles/Mac/SetupMono.sh Engine/Build/BatchFiles/Mac

if [ "$4" == "-buildscw" ] || [ "$5" == "-buildscw" ]; then
	echo Building ShaderCompileWorker...
	mono Engine/Binaries/DotNET/UnrealBuildTool.exe ShaderCompileWorker Mac Development
fi

echo Running command : Engine/Binaries/DotNET/UnrealBuildTool.exe "$@"
mono Engine/Binaries/DotNET/UnrealBuildTool.exe "$@"

ExitCode=$?
if [ $ExitCode -eq 254 ] || [ $ExitCode -eq 255 ] || [ $ExitCode -eq 2 ]; then
	exit 0
else
	exit $ExitCode
fi
