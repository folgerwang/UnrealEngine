#!/bin/bash
# Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

# Certain versions of mono can crash while running GitDependencies
# This script checks if that happened and simply re-runs the tool.

ARGS=$@
# cd to Engine root
cd "$(dirname "$BASH_SOURCE")"/../../../..
RESULT=0

while : ; do
        mono Engine/Binaries/DotNET/GitDependencies.exe $ARGS
        RESULT=$?

        echo "Result: $RESULT"
        # quit if not crashed
        [[ $RESULT -lt 129 ]] && break
        echo "mono GitDependencies.exe $ARGS crashed with return code $RESULT" >> GitDependencies.crash.log
done

exit $RESULT
