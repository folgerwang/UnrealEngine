#!/bin/sh

#####################
# configuration

# library versions - expected to match tarball and directory names
VER=Python-2.7.14

# don't forget to match archive options with tarball type (bz/gz)
TARBALL=$VER.tar.xz

DEST_DIR=`pwd`

#####################
# unpack

echo "#######################################"
echo "# Unpacking the tarballs"
tar xf $TARBALL

#####################
# build

pushd $VER
echo "#######################################"
echo "# Configuring $VER"
CPPFLAGS="-fPIC" ./configure --enable-optimizations --enable-shared
echo "# Building $VER"
make -j36
if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# ERROR!"
	echo ""
	exit 1
fi

make install DESTDIR=$DEST_DIR
if [ $? -ne 0 ]; then
	echo ""
	echo "#######################################"
	echo "# ERROR!"
	echo ""
	exit 1
fi

popd

pushd usr/local
tar czvf ../../Python-Linux.tar.gz *
