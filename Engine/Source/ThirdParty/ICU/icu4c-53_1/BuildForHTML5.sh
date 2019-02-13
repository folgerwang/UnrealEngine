#!/bin/bash
set -x
# set -x -e # ICU has a number of warnings that will fail when -e is set


# Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

# NOTE: this script needs to be built from Engine/Source/ThirdParty/HTML5/Build_All_HTML5_libs.sh


# TODO: CONVERT to CMAKE!


SYSTEM=$(uname)
if [[ $SYSTEM == *'_NT-'* ]]; then
	echo "ERROR: unable to run configure from windows"
	echo "ERROR: see Build_All_HTML5_libs.rc for details"
	exit
fi


# ----------------------------------------
ICU_HTML5=$(pwd)

# github version needs some loving
cd source
	chmod +x configure

	# Convert all configure files to unix line endings
	if [ $(command -v dos2unix && true) ]; then
		d2u="dos2unix -f"
	else #jic
		REGEX='s/\r\n/\n/g'
		d2u="perl -pi -e $REGEX"
	fi

	$d2u configure
	$d2u config.sub
	$d2u config.guess
	$d2u mkinstalldirs
	cd config
		$d2u make2sh.sed
		$d2u Makefile.inc.in
		$d2u mh-darwin
		$d2u icu-config-bottom
	cd ..
cd ..

# ----------------------------------------
# using save files so i can run this script over and over again


# strip out "data" Makefile -- this is not necessary for HTML5 builds...
if [ ! -e source/Makefile.in.save ]; then
	mv source/Makefile.in source/Makefile.in.save
fi
sed -e 's/) data /) /' source/Makefile.in.save > source/Makefile.in

if [ $UE_LIB_EXT == 'bc' ]; then

	# change to "A = bc" -- archive to bytecode
	if [ ! -e source/config/Makefile.inc.in.save ]; then
		mv source/config/Makefile.inc.in source/config/Makefile.inc.in.save
	fi
	sed -e 's/A = a/A = bc/' source/config/Makefile.inc.in.save > source/config/Makefile.inc.in
	
	if [ ! -e source/icudefs.mk.in.save ]; then
		mv source/icudefs.mk.in source/icudefs.mk.in.save
	fi
	sed -e 's/A = a/A = bc/' -e 's/\(@ARFLAGS@\).*/\1/' source/icudefs.mk.in.save > source/icudefs.mk.in
	
	
	# change STATIC_O to bc (bytecode) for both OSX and Linux
	if [ ! -e source/config/mh-darwin.save ]; then
		mv source/config/mh-darwin source/config/mh-darwin.save
	fi
	sed -e 's/\(STATIC_O =\).*/\1 bc/' -e '/ARFLAGS/d' source/config/mh-darwin.save > source/config/mh-darwin
	
	if [ ! -e source/config/mh-linux.save ]; then
		mv source/config/mh-linux source/config/mh-linux.save
	fi
	sed -e 's/\(STATIC_O =\).*/\1 bc/' -e '/ARFLAGS/d' source/config/mh-linux.save > source/config/mh-linux

fi

# ----------------------------------------
# emcc & em++ will fail these checks and cause a missing default constructor linker issue so we force the tests to pass
export ac_cv_override_cxx_allocation_ok="yes"
export ac_cv_override_placement_new_ok="yes"


# ----------------------------------------
# MAKE

build_all()
{
	echo
	echo BUILDING $OPTIMIZATION
	echo

	rm -rf html5_build$OPTIMIZATION
	mkdir html5_build$OPTIMIZATION
	cd html5_build$OPTIMIZATION

	COMMONCOMPILERFLAGS="$OPTIMIZATION -DUCONFIG_NO_TRANSLITERATION=1 -DU_USING_ICU_NAMESPACE=0 -DU_NO_DEFAULT_INCLUDE_UTF_HEADERS=1 -DUNISTR_FROM_CHAR_EXPLICIT=explicit -DUNISTR_FROM_STRING_EXPLICIT=explicit -DU_STATIC_IMPLEMENTATION -DU_OVERRIDE_CXX_ALLOCATION=1"
	EMFLAGS="$UE_EMFLAGS"

	if [ $UE_LIB_EXT == 'bc' ]; then
		emconfigure ../source/configure CFLAGS="$COMMONCOMPILERFLAGS $EMFLAGS" CXXFLAGS="$COMMONCOMPILERFLAGS $EMFLAGS -std=c++14" CPPFLAGS="$COMMONCOMPILERFLAGS $EMFLAGS" LDFLAGS="$OPTIMIZATION $EMFLAGS" AR="emcc" ARFLAGS="$OPTIMIZATION $EMFLAGS -o" RANLIB="echo" --disable-debug --enable-release --enable-static --disable-shared --disable-extras --disable-samples --disable-tools --disable-tests
	else
		emconfigure ../source/configure CFLAGS="$COMMONCOMPILERFLAGS $EMFLAGS" CXXFLAGS="$COMMONCOMPILERFLAGS $EMFLAGS -std=c++14" CPPFLAGS="$COMMONCOMPILERFLAGS $EMFLAGS" LDFLAGS="$OPTIMIZATION $EMFLAGS" AR="emar"                                                   --disable-debug --enable-release --enable-static --disable-shared --disable-extras --disable-samples --disable-tools --disable-tests
	fi

	# stuff in ICULIBSUFFIX
	mv icudefs.mk icudefs.mk.save
	sed -e "s/\(ICULIBSUFFIX=\)/\1$LIB_SUFFIX/" icudefs.mk.save > icudefs.mk

	if [ $UE_LIB_EXT == 'bc' ]; then
		# and add build rules for .bc
		echo '
%.bc : %.o
	$(CC) $(CFLAGS) $< -o $@
' >> icudefs.mk
	fi

	# finally...
#	emmake make clean VERBOSE=1
	emmake make -j VERBOSE=1 | tee zzz_build.log

	cd ..
}

OPTIMIZATION=-O3; LIB_SUFFIX=_O3; build_all

OPTIMIZATION=-O2; LIB_SUFFIX=_O2; build_all

OPTIMIZATION=-Oz; LIB_SUFFIX=_Oz; build_all

OPTIMIZATION=-O0; LIB_SUFFIX=
build_all


# ----------------------------------------
# INSTALL

if [ ! -d HTML5 ]; then
	mkdir HTML5
fi

# TODO change this to p4 checkout
if [ ! -z "$(ls -A HTML5)" ]; then
	chmod 744 HTML5/*
fi

cp -vp html5_build*/lib/* html5_build*/stubdata/lib* HTML5/.


# ----------------------------------------
# restore

if [ -e source/Makefile.in.save ]; then
	mv source/Makefile.in.save source/Makefile.in
fi
if [ -e source/config/Makefile.inc.in.save ]; then
	mv source/config/Makefile.inc.in.save source/config/Makefile.inc.in
fi
if [ -e source/icudefs.mk.in.save ]; then
	mv source/icudefs.mk.in.save source/icudefs.mk.in
fi
if [ -e source/config/mh-darwin.save ]; then
	mv source/config/mh-darwin.save source/config/mh-darwin
fi
if [ -e source/config/mh-linux.save ]; then
	mv source/config/mh-linux.save source/config/mh-linux
fi



# ----------------------------------------
# clean up
#rm -rf html5_build-O3 html5_build-Oz html5_build-O0


# ----------------------------------------
exit
exit
exit
# ----------------------------------------
# NO LONGER USED -- LEFT HERE FOR REFERENCE

build_lib ()
{
	libbasename="${1##*/}"
	finallibname="${libbasename%.a}.$UE_LIB_EXT"
	echo Building $1 to $finallibname
	mkdir tmp
	cd tmp
	llvm-ar x ../$1
	for f in *.ao; do
		mv "$f" "${f%.ao}.$UE_LIB_EXT";
	done
	LINK_FILES=$(ls *.$UE_LIB_EXT)
	emcc $OPTIMIZATION $LINK_FILES -o "../$finallibname"
	cd ..
	rm -rf tmp
}

for f in ../html5_build/lib/*.a; do
	build_lib $f
done
for f in ../html5_build/stubdata/*.a; do
	build_lib $f
done
