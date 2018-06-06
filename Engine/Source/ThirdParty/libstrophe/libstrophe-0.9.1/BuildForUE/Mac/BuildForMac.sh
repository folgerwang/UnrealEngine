#!/bin/sh

# Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

SCRIPT_DIR=$(cd $(dirname $0) && pwd)

BUILD_DIR="${SCRIPT_DIR}/../../Mac/Build"

if [ -d "${BUILD_DIR}" ]; then
	rm -rf "${BUILD_DIR}"
fi
mkdir -pv "${BUILD_DIR}"

cd "${BUILD_DIR}"
../../../../../../Extras/ThirdPartyNotUE/CMake/bin/cmake -G "Xcode" -DSOCKET_IMPL=../../src/sock.c -DDISABLE_TLS=0 -DOPENSSL_PATH=../../../OpenSSL/1.0.2g/include/Mac -DCMAKE_OSX_DEPLOYMENT_TARGET="10.9" "${SCRIPT_DIR}/../../BuildForUE"

function build()
{
	CONFIGURATION=$1
	xcodebuild -project libstrophe.xcodeproj -configuration $CONFIGURATION
}

build Release
build Debug
cd "${SCRIPT_DIR}"
rm -rf "${BUILD_DIR}"
