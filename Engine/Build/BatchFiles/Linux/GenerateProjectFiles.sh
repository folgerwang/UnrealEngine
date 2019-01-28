#!/bin/bash
# Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

echo
echo Setting up Unreal Engine 4 project files...
echo

# If ran from someone other then the script location we'll have the full base path
BASE_PATH="`dirname "$0"`"

# this is located inside an extra 'Linux' path unlike the Windows variant.

if [ ! -d "$BASE_PATH/../../../Binaries/DotNET" ]; then
 echo GenerateProjectFiles ERROR: It looks like you're missing some files that are required in order to generate projects.  Please check that you've downloaded and unpacked the engine source code, binaries, content and third-party dependencies before running this script.
 exit 1
fi

if [ ! -d "$BASE_PATH/../../../Source" ]; then
 echo GenerateProjectFiles ERROR: This script file does not appear to be located inside the Engine/Build/BatchFiles/Mac directory.
 exit 1
fi

source "$BASE_PATH/SetupMono.sh" $BASE_PATH

if [ -f "$BASE_PATH/../../../Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj" ]; then
	xbuild "$BASE_PATH/../../../Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj" /property:Configuration="Development" /verbosity:quiet /nologo
fi

# pass all parameters to UBT
mono "$BASE_PATH/../../../Binaries/DotNET/UnrealBuildTool.exe" -projectfiles "$@"
