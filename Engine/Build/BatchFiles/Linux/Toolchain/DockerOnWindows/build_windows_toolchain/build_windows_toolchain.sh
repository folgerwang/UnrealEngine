#! /bin/bash

TARGETS="aarch64-unknown-linux-gnueabi arm-unknown-linux-gnueabihf x86_64-unknown-linux-gnu"

# Helper function
GetLibDirForArch()
{
	local ARCHELIBDIRECTORIES=("x86_64-unknown-linux-gnu lib64" "arm-unknown-linux-gnueabihf lib" "aarch64-unknown-linux-gnueabi lib64")
	local Arch=$1

	for dir in "${ARCHELIBDIRECTORIES[@]}";
	do
		dir_split=($dir)
		if [ "${dir_split[0]}" == "$Arch" ]; then
			echo ${dir_split[1]}
		fi
	done
}

# Default permissions
umask 0022

# Get num of cores
CORES=`cat /proc/cpuinfo | awk '/^processor/{print $3}' | wc -l`
echo Using $CORES cores for building

# Get crosstool-ng
git clone http://github.com/RCL/crosstool-ng -b 1.22

# Build crosstool-ng
tar xf crosstool-ng.tar.xz
pushd crosstool-ng
./bootstrap && ./configure --enable-local && make
popd

# Build toolchains
for arch in $TARGETS; do
	mkdir -p build-$arch
	pushd build-$arch
	cp ../$arch.windows.config .config
	../crosstool-ng/ct-ng build.$CORES
	popd
done

# Unpack compiler-rt libs from Linux native toolchain
mkdir -p compiler-rt
pushd compiler-rt
tar xf ../UnrealToolchain.tar.gz
popd

for arch in $TARGETS; do
	ArchLibDirectory=`GetLibDirForArch $arch`

	echo "Copying toolchain..."
	mkdir -p OUTPUT/$arch/bin
	mkdir -p OUTPUT/$arch/lib
	chmod 777 OUTPUT/$arch/bin
	chmod 777 OUTPUT/$arch/lib

	pushd OUTPUT/$arch/
	find . -type d -exec chmod +w {} \;

	cp -r -L $arch/include .

	if [ "$arch" == "aarch64-unknown-linux-gnueabi" ]; then
		cp -r -L $arch/sysroot/lib64/* lib/
		mkdir -p lib64
		cp lib/libc.so.6 lib64/
		cp lib/libpthread.so.0 lib64/
	else
		cp -r -L $arch/sysroot/$ArchLibDirectory .
	fi

	mkdir -p usr
	cp -r -L $arch/sysroot/usr/$ArchLibDirectory usr/
	cp -r -L $arch/sysroot/usr/include usr/
	rm -rf $arch

	rm -f build.log.bz2

	# Copy compiler-rt
	cp -r ../../compiler-rt/$arch/lib/clang lib/

	popd
done

# Pack everything
pushd OUTPUT
zip -r ../UnrealToolchain.zip *
popd
