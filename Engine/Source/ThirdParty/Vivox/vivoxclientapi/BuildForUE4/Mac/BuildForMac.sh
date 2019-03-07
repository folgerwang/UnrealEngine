#!/bin/sh

# Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

SCRIPT_DIR=$(cd $(dirname $0) && pwd)

BUILD_DIR="${SCRIPT_DIR}/../../Mac/Build"

if [ -d "${BUILD_DIR}" ]; then
	rm -rf "${BUILD_DIR}"
fi
mkdir -pv "${BUILD_DIR}"

cd "${BUILD_DIR}"
../../../../../../Extras/ThirdPartyNotUE/CMake/bin/cmake -G "Xcode" -DVIVOXSDK_PATH=../../vivox-sdk/Include -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9" "${SCRIPT_DIR}/.."

function build()
{
	CONFIGURATION=$1
	xcodebuild -project vivoxclientapi.xcodeproj -configuration $CONFIGURATION
}

build Release
build Debug
cd "${SCRIPT_DIR}"
rm -rf "${BUILD_DIR}"
