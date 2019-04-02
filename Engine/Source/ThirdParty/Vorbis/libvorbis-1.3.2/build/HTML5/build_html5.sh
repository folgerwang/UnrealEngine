#!/bin/bash
set -x -e

# Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

# NOTE: this script needs to be built from Engine/Source/ThirdParty/HTML5/Build_All_HTML5_libs.sh


VORBIS_HTML5=$(pwd)

cd ../../../..
	OGG_DIR="$(pwd)/Ogg/libogg-1.2.2"
	VORB_FLAGS="-I\"$OGG_DIR/include\" -Wno-comment -Wno-shift-op-parentheses"
cd "$VORBIS_HTML5"


build_via_cmake()
{
	SUFFIX=_O$OLEVEL
	OPTIMIZATION=-O$OLEVEL
	# ----------------------------------------
	rm -rf BUILD$SUFFIX
	mkdir BUILD$SUFFIX
	cd BUILD$SUFFIX
	# ----------------------------------------
#	TYPE=${type^^} # OSX-bash doesn't like this
	TYPE=`echo $type | tr "[:lower:]" "[:upper:]"`
	if [ $TYPE == "DEBUG" ]; then
		DBGFLAG=_DEBUG
	else
		DBGFLAG=NDEBUG
	fi
	EMFLAGS="$UE_EMFLAGS"
	# ----------------------------------------
	emcmake cmake -G "Unix Makefiles" \
		-DBUILD_SHARED_LIBS=OFF \
		-DOGG_INCLUDE_DIRS=$OGG_DIR/include \
		-DOGG_LIBRARIES=$OGG_DIR/lib/HTML5 \
		-DEMSCRIPTEN_GENERATE_BITCODE_STATIC_LIBRARIES=$UE_USE_BITECODE \
		-DCMAKE_BUILD_TYPE=$type \
		-DCMAKE_C_FLAGS_$TYPE="$OPTIMIZATION -D$DBGFLAG $EMFLAGS $VORB_FLAGS" \
		-DCMAKE_CXX_FLAGS_$TYPE="$OPTIMIZATION -D$DBGFLAG $EMFLAGS $VORB_FLAGS" \
		../../..
	cmake --build . -- -j VERBOSE=1 2>&1 | tee zzz_build.log
	# ----------------------------------------
	if [ $OLEVEL == 0 ]; then
		SUFFIX=
	fi
	cp lib/libvorbis.$UE_LIB_EXT ../../../lib/HTML5/libvorbis${SUFFIX}.$UE_LIB_EXT
#	cp lib/libvorbisenc.$UE_LIB_EXT ../../../lib/HTML5/libvorbisenc${SUFFIX}.$UE_LIB_EXT
	cp lib/libvorbisfile.$UE_LIB_EXT ../../../lib/HTML5/libvorbisfile${SUFFIX}.$UE_LIB_EXT
	cd ..
}
type=Debug;       OLEVEL=0;  build_via_cmake
type=Release;     OLEVEL=2;  build_via_cmake
type=Release;     OLEVEL=3;  build_via_cmake
type=MinSizeRel;  OLEVEL=z;  build_via_cmake
ls -l ../../lib/HTML5


exit
exit
exit


# NOT USED: LEFT HERE FOR REFERENCE
build_via_makefile()
{
	cd ../../lib
	
	proj1=libvorbis
	proj2=libvorbisfile
	makefile=../build/HTML5/Makefile.HTML5
	EMFLAGS="-msse -msse2 -s FULL_ES2=1 -s USE_PTHREADS=1"
	
	make clean   OPTIMIZATION=-O3 CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}_O3.$UE_LIB_EXT LIB2=${proj2}_O3.$UE_LIB_EXT -f ${makefile}
	make install OPTIMIZATION=-O3 CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}_O3.$UE_LIB_EXT LIB2=${proj2}_O3.$UE_LIB_EXT -f ${makefile}
	
	make clean   OPTIMIZATION=-O2 CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}_O2.$UE_LIB_EXT LIB2=${proj2}_O2.$UE_LIB_EXT -f ${makefile}
	make install OPTIMIZATION=-O2 CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}_O2.$UE_LIB_EXT LIB2=${proj2}_O2.$UE_LIB_EXT -f ${makefile}
	
	make clean   OPTIMIZATION=-Oz CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}_Oz.$UE_LIB_EXT LIB2=${proj2}_Oz.$UE_LIB_EXT -f ${makefile}
	make install OPTIMIZATION=-Oz CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}_Oz.$UE_LIB_EXT LIB2=${proj2}_Oz.$UE_LIB_EXT -f ${makefile}
	
	make clean   OPTIMIZATION=-O0 CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}.$UE_LIB_EXT LIB2=${proj2}.$UE_LIB_EXT -f ${makefile}
	make install OPTIMIZATION=-O0 CFLAGS_EXTRA="$EMFLAGS" LIB1=${proj1}.$UE_LIB_EXT LIB2=${proj2}.$UE_LIB_EXT -f ${makefile}
	
	ls -l ../lib/HTML5
}

# no longer needed with latest emscripten:
#		-DCMAKE_TOOLCHAIN_FILE=$EMSCRIPTEN/cmake/Modules/Platform/Emscripten.cmake \

