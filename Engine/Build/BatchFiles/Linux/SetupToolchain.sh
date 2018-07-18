#!/bin/bash
# Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

# put ourselves into Engine directory (two up from location of this script)
pushd "`dirname "$0"`/../../.."

TOOLCHAIN_VERSION=v11_clang-5.0.0-centos7
TOOLCHAIN_ARCHIVE=$TOOLCHAIN_VERSION.tar.gz

TOOLCHAIN_URL=http://cdn.unrealengine.com/Toolchain_Linux/native-linux-$TOOLCHAIN_ARCHIVE
TOOLCHAIN_ROOT=Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/

if [ -d $TOOLCHAIN_ROOT/$TOOLCHAIN_VERSION ]; then
	echo "Toolchain already installed skipping."
	exit
fi

echo "Downloading toolchain."
mkdir -p $TOOLCHAIN_ROOT
pushd $TOOLCHAIN_ROOT > /dev/null

if which curl 1>/dev/null; then
	curl $TOOLCHAIN_URL -o $TOOLCHAIN_ARCHIVE
elif which wget 1>/dev/null; then
	wget $TOOLCHAIN_URL -O $TOOLCHAIN_ARCHIVE
else 
	echo "Please install curl or wget"
	exit
fi

if [ -f $TOOLCHAIN_ARCHIVE ]; then
	echo "Extracting toolchain."
	tar -xvf $TOOLCHAIN_ARCHIVE
	rm -f $TOOLCHAIN_ARCHIVE
else
	echo "Download failed please fetch from $TOOLCHAIN_URL manually".
fi

popd > /dev/null