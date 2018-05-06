#!/bin/bash
# Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

function try_symlink_dir {
	if [ -d $(dirname $2)/$1 ]; then
		if [ ! -d $2 ]; then
			ln -s $1 $2
		fi
	fi
}

function try_copy {
	if [ -f $1 ]; then
		if [ ! -f $2 ]; then
			cp $1 $2
		fi
	fi
}


function try_symlink {
	if [ -f $(dirname $2)/$1 ]; then
		if [ ! -f $2 ]; then
			ln -s $1 $2
		fi
	fi
}

CUR_DIR=`pwd`
bash ../Mac/FixMonoFiles.sh
cd ../../../Binaries/ThirdParty/Mono/Linux

if [ -z $HOST_ARCH ]; then
	HOST_ARCH="x86_64-unknown-linux-gnu"
fi

mkdir -p bin
try_symlink ../$HOST_ARCH/bin/mono-boehm bin/mono-boehm
try_symlink ../$HOST_ARCH/bin/mono-sgen bin/mono-sgen
try_symlink ../$HOST_ARCH/bin/mono-sgen bin/mono
for FILE in ../Mac/bin/*; do
	try_symlink ../../Mac/bin/$(basename $FILE) bin/$(basename $FILE) 
done

mkdir -p lib/mono/4.5
try_copy ../Mac/lib/mono/4.5/mscorlib.dll lib/mono/4.5/mscorlib.dll
try_copy ../Mac/lib/mono/4.5/mcs.exe lib/mono/4.5/mcs.exe
for FILE in ../Mac/lib/mono/4.5/*; do
	try_symlink ../../../../Mac/lib/mono/4.5/$(basename $FILE) lib/mono/4.5/$(basename $FILE) 
	try_symlink_dir ../../../../Mac/lib/mono/4.5/$(basename $FILE) lib/mono/4.5/$(basename $FILE) 
done
for DIRECTORY in ../Mac/lib/mono/*; do
	try_symlink_dir ../../../Mac/lib/mono/$(basename $DIRECTORY) lib/mono/$(basename $DIRECTORY) 
done
for DIRECTORY in ../Mac/lib/*; do
	try_symlink_dir ../../Mac/lib/$(basename $DIRECTORY) lib/$(basename $DIRECTORY) 
done
try_symlink ../../../$HOST_ARCH/lib/mscorlib.dll.so lib/mono/4.5/mscorlib.dll.so
try_symlink ../../../$HOST_ARCH/lib/mcs.exe.so lib/mono/4.5/mcs.exe.so

mkdir -p etc/mono
for DIRECTORY in ../Mac/etc/*; do
	try_symlink_dir ../../Mac/etc/$(basename $DIRECTORY) etc/$(basename $DIRECTORY) 
done
for FILE in ../Mac/etc/mono/*; do
	try_symlink ../../../Mac/etc/mono/$(basename $FILE) etc/mono/$(basename $FILE) 
	try_symlink_dir ../../../Mac/etc/mono/$(basename $FILE) etc/mono/$(basename $FILE) 
done

try_symlink_dir ../Mac/share share
try_symlink_dir ../Mac/include include

function try_chmod {
	if [ -f $1 ]; then
		chmod +x $1
	fi
}

try_chmod $TARGET_ARCH/mono-boehm

cd "$CUR_DIR"
