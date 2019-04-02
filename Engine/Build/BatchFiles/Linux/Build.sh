#!/bin/bash
# Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
# This script gets can be used to build clean individual projects using UnrealBuildTool

set -e

cd "`dirname "$0"`/../../../.." 

# Setup Mono
source Engine/Build/BatchFiles/Linux/SetupMono.sh Engine/Build/BatchFiles/Linux

# First make sure that the UnrealBuildTool is up-to-date
if ! xbuild /property:Configuration=Development /verbosity:quiet /nologo Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj; then
  echo "Failed to build to build tool (UnrealBuildTool)"
  exit 1
fi

echo "Building $1..."
mono Engine/Binaries/DotNET/UnrealBuildTool.exe "$@"
exit $?
