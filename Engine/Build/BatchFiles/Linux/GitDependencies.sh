#!/bin/bash
# Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

# Certain versions of mono can crash while running GitDependencies
# This script checks if that happened and simply re-runs the tool.

ARGS=$@

SCRIPT_PATH=$0
if [ -L "$SCRIPT_PATH" ]; then
    SCRIPT_PATH=$(dirname "$SCRIPT_PATH")/$(readlink "$SCRIPT_PATH")
fi

cd "$(dirname "$SCRIPT_PATH")" && SCRIPT_PATH="`pwd`/$(basename "$SCRIPT_PATH")"

BASE_PATH="`dirname "$SCRIPT_PATH"`"

# cd to Engine root
cd ../../../..
RESULT=0

source "$BASE_PATH/SetupMono.sh" "$BASE_PATH"

while : ; do
        mono Engine/Binaries/DotNET/GitDependencies.exe $ARGS
        RESULT=$?

        echo "Result: $RESULT"
        # quit if not crashed
        [[ $RESULT -lt 129 ]] && break
        echo "mono GitDependencies.exe $ARGS crashed with return code $RESULT" >> GitDependencies.crash.log
done

exit $RESULT
