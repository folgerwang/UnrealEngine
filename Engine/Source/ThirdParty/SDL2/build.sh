#!/bin/bash

## Unreal Engine 4 Build script for SDL2
## Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

pushd "`dirname "$0"`/"

if [ -z "$TARGET_ARCH" ]; then
	TARGET_ARCH=x86_64-unknown-linux-gnu
fi

export VULKAN_SDK=`pwd`/../Vulkan/Linux
export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:/usr/local/share/pkgconfig:$PKG_CONFIG_PATH

BuildWithOptions()
{
	local BuildDir=$1
	local SdlDir=$2
	local SdlLibName=$3
	shift
	shift
	shift
	local Options=$@

	rm -rf $BuildDir
	mkdir -p $BuildDir
	pushd $BuildDir

	cmake $Options $SdlDir
	make -j 4
	mkdir -p $SdlDir/lib/Linux/$TARGET_ARCH/
	cp --remove-destination libSDL2.a $SdlDir/lib/Linux/$TARGET_ARCH/$SdlLibName
	popd
}

set -e
SDL_DIR=SDL-gui-backend
BUILD_DIR=build-$SDL_DIR

# build Debug with -fPIC so it's usable in any type of build
BuildWithOptions $BUILD_DIR-Debug ../$SDL_DIR libSDL2_fPIC_Debug.a -DCMAKE_BUILD_TYPE=Debug -DSDL_STATIC_PIC=ON -DVIDEO_MIR=OFF -DVIDEO_KMSDRM=OFF
#exit 0
BuildWithOptions $BUILD_DIR-Release ../$SDL_DIR libSDL2.a -DCMAKE_BUILD_TYPE=Release -DVIDEO_MIR=OFF -DVIDEO_KMSDRM=OFF
BuildWithOptions $BUILD_DIR-ReleasePIC ../$SDL_DIR libSDL2_fPIC.a -DCMAKE_BUILD_TYPE=Release -DSDL_STATIC_PIC=ON -DVIDEO_MIR=OFF -DVIDEO_KMSDRM=OFF
set +e

