#!/bin/sh

# Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

SCRIPT_DIR=$(cd $(dirname $0) && pwd)

BUILD_DIR="${SCRIPT_DIR}/../../IOS/Build"

if [ -d "${BUILD_DIR}" ]; then
	rm -rf "${BUILD_DIR}"
fi
mkdir -pv "${BUILD_DIR}"

cd "${BUILD_DIR}"
../../../../../../Extras/ThirdPartyNotUE/CMake/bin/cmake -G "Xcode" -DBUILD_tools=0 -DBUILD_examples=0 -DBUILD_tests=0 -DBUILD_shared=0 -DSKIP_PRE_BUILD_COMMAND=1 -DCMAKE_XCODE_ATTRIBUTE_SDKROOT=iphoneos -DCMAKE_XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET=8.0 -DCMAKE_XCODE_ATTRIBUTE_ONLY_ACTIVE_ARCH=NO -DCMAKE_XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY=1,2 "${SCRIPT_DIR}/../.."

function build()
{
	CONFIGURATION=$1
	xcodebuild BITCODE_GENERATION_MODE=bitcode -project expat.xcodeproj -configuration $CONFIGURATION -destination generic/platform=iOS
	mkdir -p ../${CONFIGURATION}/
	cp -v ${CONFIGURATION}-iphoneos/* ../${CONFIGURATION}/
}

build Release
build Debug
cd "${SCRIPT_DIR}"
rm -rf "${BUILD_DIR}"
