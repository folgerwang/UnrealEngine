#!/usr/bin/env bash
set -e  # exit immediately on error
set -x  # print commands

# Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

# --------------------------------------------------------------------------------
# remember to source your emscripten's env settings before using this script:
#   e.g.> source .../emsdk_clone/emsdk_env.sh
# a.k.a.> . .../emsdk_clone/emsdk_set_env.sh


# --------------------------------------------------------------------------------
# experimental

LLVMBACKEND=0 # backend to use =>  0:WASM  1:LLVM


# --------------------------------------------------------------------------------
# this flag is used in all build scripts below

#export UE_EMFLAGS="-msse2 -s SIMD=1 -s USE_PTHREADS=1"
#export UE_EMFLAGS="       -s SIMD=0 -s USE_PTHREADS=1"
#export UE_EMFLAGS="-msse2 -s SIMD=0 -s USE_PTHREADS=1 -s WASM=1 -s BINARYEN=1" # WASM still does not play nice with SIMD
#export UE_EMFLAGS="-msse2           -s USE_PTHREADS=1 -s WASM=1 -s BINARYEN=1" # WASM still does not play nice with SIMD

if [ $LLVMBACKEND == 1 ]; then
	export UE_USE_BITECODE='OFF'
	export UE_LIB_EXT='a'
	export UE_EMFLAGS='-s WASM=1 -s WASM_OBJECT_FILES=1'
else
	export UE_USE_BITECODE='ON'
	export UE_LIB_EXT='bc'
	export UE_EMFLAGS='-s WASM=1'
fi

# --------------------------------------------------------------------------------

# build all ThirdParty libs for HTML5
# from the simplest to build to the most complex
TPS_HTML5=$(pwd)

cd "$TPS_HTML5"/../zlib/zlib-1.2.5/Src/HTML5
	./build_html5.sh

cd "$TPS_HTML5"/../libPNG/libPNG-1.5.2/projects/HTML5
	./build_html5.sh

cd "$TPS_HTML5"/../FreeType2/FreeType2-2.6/Builds/html5
	./build_html5.sh

cd "$TPS_HTML5"/../Ogg/libogg-1.2.2/build/HTML5
	./build_html5.sh

cd "$TPS_HTML5"/../Vorbis/libvorbis-1.3.2/build/HTML5
	./build_html5.sh

# WARNING: this might take a while...
cd "$TPS_HTML5"/../ICU/icu4c-53_1
	./BuildForHTML5.sh

cd "$TPS_HTML5"/../HarfBuzz/harfbuzz-1.2.4/BuildForUE/HTML5
	./BuildForHTML5.sh

# WARNING: this might take a while...
cd "$TPS_HTML5"/../PhysX3/PhysX_3.4/Source/compiler/cmake/html5
	./BuildForHTML5.sh

# WARNING: this might take a while...
cd "$TPS_HTML5"/../SDL2
	./build_html5.sh

cd "$TPS_HTML5"

echo 'Success!'
