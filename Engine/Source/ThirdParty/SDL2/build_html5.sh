#!/bin/bash

SDL2_HTML5=$(pwd)

cd ../HTML5/
	. ./Build_All_HTML5_libs.rc
cd "$SDL2_HTML5/SDL-gui-backend"


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
#	EMFLAGS="-msse2 -s SIMD=1 -s USE_PTHREADS=1"
#	EMFLAGS="-msse2 -s SIMD=0 -s USE_PTHREADS=1 -s WASM=1 -s BINARYEN=1" # WASM still does not play nice with SIMD
	EMFLAGS="-s SIMD=0 -s USE_PTHREADS=1"
	# ----------------------------------------
	emcmake cmake -G "Unix Makefiles" \
		-DBUILD_SHARED_LIBS=FALSE \
		-DLIBTYPE=STATIC \
		-DEMSCRIPTEN_GENERATE_BITCODE_STATIC_LIBRARIES=ON \
		-DCMAKE_BUILD_TYPE=$type \
		-DCMAKE_C_FLAGS_$TYPE="$OPTIMIZATION -D$DBGFLAG $EMFLAGS" \
		..
	cmake --build . -- -j VERBOSE=1 2>&1 | tee zzz_build.log
	# ----------------------------------------
	if [ $OLEVEL == 0 ]; then
		SUFFIX=
	fi
	cp libSDL2.bc ../lib/HTML5/libSDL2${SUFFIX}.bc
	cd ..
}
chmod +w lib/HTML5/*
type=Debug;       OLEVEL=0;  build_via_cmake
type=Release;     OLEVEL=2;  build_via_cmake
type=Release;     OLEVEL=3;  build_via_cmake
type=MinSizeRel;  OLEVEL=z;  build_via_cmake
ls -l lib/HTML5


exit

# NOT USED: LEFT HERE FOR REFERENCE
# NOT USED: LEFT HERE FOR REFERENCE
# NOT USED: LEFT HERE FOR REFERENCE


build_via_makefile()
{
	cd ../../Src
	
	proj=libSDL2
	makefile=../Builds/html5/Makefile.HTML5
	EMFLAGS="-msse -msse2 -s FULL_ES2=1 -s USE_PTHREADS=1"
	
	make clean   OPTIMIZATION=-O3 CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}_O3.bc -f ${makefile}
	make install OPTIMIZATION=-O3 CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}_O3.bc -f ${makefile}
	
	make clean   OPTIMIZATION=-O2 CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}_O2.bc -f ${makefile}
	make install OPTIMIZATION=-O2 CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}_O2.bc -f ${makefile}
	
	make clean   OPTIMIZATION=-Oz CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}_Oz.bc -f ${makefile}
	make install OPTIMIZATION=-Oz CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}_Oz.bc -f ${makefile}
	
	make clean   OPTIMIZATION=-O0 CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}.bc -f ${makefile}
	make install OPTIMIZATION=-O0 CFLAGS_EXTRA="$EMFLAGS" LIB=${proj}.bc -f ${makefile}
	
	ls -l ../Lib/HTML5
}

# no longer needed with latest emscripten:
#		-DCMAKE_TOOLCHAIN_FILE=$EMSCRIPTEN/cmake/Modules/Platform/Emscripten.cmake \

