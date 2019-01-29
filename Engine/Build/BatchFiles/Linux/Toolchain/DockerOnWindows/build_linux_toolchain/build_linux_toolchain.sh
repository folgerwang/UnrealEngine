#! /bin/bash

LLVM_VERSION=7.0.1
TOOLCHAIN_VERSION=v13

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
pushd crosstool-ng
./bootstrap && ./configure --enable-local && make
popd

# Build toolchains
for arch in $TARGETS; do
	mkdir -p build-$arch
	pushd build-$arch
	cp ../$arch.linux.config .config
	../crosstool-ng/ct-ng build.$CORES
	popd
done

# Build clang
LLVM=llvm-$LLVM_VERSION
CLANG=cfe-$LLVM_VERSION
LLD=lld-$LLVM_VERSION
COMPILER_RT=compiler-rt-$LLVM_VERSION

wget http://releases.llvm.org/$LLVM_VERSION/$LLVM.src.tar.xz
wget http://releases.llvm.org/$LLVM_VERSION/$CLANG.src.tar.xz
wget http://releases.llvm.org/$LLVM_VERSION/$LLD.src.tar.xz
wget http://releases.llvm.org/$LLVM_VERSION/$COMPILER_RT.src.tar.xz

mkdir -p llvm
tar -xf $LLVM.src.tar.xz --strip-components 1 -C llvm
mkdir -p llvm/tools/clang
tar -xf $CLANG.src.tar.xz --strip-components 1 -C llvm/tools/clang/
mkdir -p llvm/tools/lld
tar -xf $LLD.src.tar.xz --strip-components 1 -C llvm/tools/lld/
mkdir -p llvm/projects/compiler-rt
tar -xf $COMPILER_RT.src.tar.xz --strip-components 1 -C llvm/projects/compiler-rt/

mkdir build-clang
pushd build-clang
cmake3 -DCMAKE_INSTALL_PREFIX=../install-clang -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_LIBCXX=1 -DCMAKE_CROSSCOMPILING=True -DLLVM_TARGETS_TO_BUILD="AArch64;ARM;X86" -DLLVM_ENABLE_LIBXML2=OFF -DCMAKE_C_COMPILER=/home/buildmaster/OUTPUT/x86_64-unknown-linux-gnu/bin/x86_64-unknown-linux-gnu-gcc -DCMAKE_C_FLAGS="--sysroot=/home/buildmaster/OUTPUT/x86_64-unknown-linux-gnu/x86_64-unknown-linux-gnu/sysroot" -DCMAKE_CXX_COMPILER=/home/buildmaster/OUTPUT/x86_64-unknown-linux-gnu/bin/x86_64-unknown-linux-gnu-g++ -DCMAKE_CXX_FLAGS="--sysroot=/home/buildmaster/OUTPUT/x86_64-unknown-linux-gnu/x86_64-unknown-linux-gnu/sysroot" -G "Unix Makefiles" ../llvm
make -j$CORES && make install
popd

# Copy files
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

	if [ "$arch" == "x86_64-unknown-linux-gnu" ]; then
		cp -r -L $arch/sysroot/lib64 .
	else
		cp -r -L $arch/sysroot/$ArchLibDirectory/* lib/
		ln -s lib lib64
	fi

	mkdir -p usr/lib
	cp -r -L $arch/sysroot/usr/$ArchLibDirectory/* usr/lib/
	ln -s lib usr/lib64
	cp -r -L $arch/sysroot/usr/include usr/
	rm -rf $arch

	rm -f build.log.bz2

	popd

	echo "Copying clang..."
	cp -L install-clang/bin/clang OUTPUT/$arch/bin/
	cp -L install-clang/bin/clang++ OUTPUT/$arch/bin/
	cp -L install-clang/bin/lld OUTPUT/$arch/bin/
	cp -L install-clang/bin/ld.lld OUTPUT/$arch/bin/
	cp -L install-clang/bin/llvm-ar OUTPUT/$arch/bin/

	if [ "$arch" == "x86_64-unknown-linux-gnu" ]; then
		cp -r install-clang/lib/clang OUTPUT/$arch/lib/
	fi
done

# Build compiler-rt
for arch in $TARGETS; do
	if [ "$arch" == "x86_64-unknown-linux-gnu" ]; then
		# We already built it with clang
		continue
	fi

	# TODO! In LLVM 7.0.1 compiler-rt for ARM doesn't compile
	if [ "$arch" == "arm-unknown-linux-gnueabihf" ]; then
		continue
	fi

	mkdir build-rt-$arch
	pushd build-rt-$arch

	cmake3 ../llvm/projects/compiler-rt -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_C_COMPILER=/home/buildmaster/install-clang/bin/clang -DCMAKE_CXX_COMPILER=/home/buildmaster/install-clang/bin/clang++ -DCMAKE_AR=/home/buildmaster/install-clang/bin/llvm-ar -DCMAKE_NM=/home/buildmaster/install-clang/bin/llvm-nm -DCMAKE_RANLIB=/home/buildmaster/install-clang/bin/llvm-ranlib -DCMAKE_EXE_LINKER_FLAGS="--target=$arch -L/home/buildmaster/OUTPUT/$arch/lib64 --sysroot=/home/buildmaster/OUTPUT/$arch -fuse-ld=lld" -DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON -DCMAKE_C_FLAGS="--target=$arch --sysroot=/home/buildmaster/OUTPUT/$arch" -DCMAKE_CXX_FLAGS="--target=$arch --sysroot=/home/buildmaster/OUTPUT/$arch" -DCMAKE_ASM_FLAGS="--target=$arch --sysroot=/home/buildmaster/OUTPUT/$arch" -DCMAKE_INSTALL_PREFIX=../install-rt-$arch -DCMAKE_SYSTEM_NAME="Linux" -DSANITIZER_COMMON_LINK_FLAGS="-fuse-ld=lld" -DCMAKE_C_COMPILER_TARGET="$arch" -DLLVM_CONFIG_PATH=/home/buildmaster/install-clang/bin/llvm-config

	make -j$CORES && make install
	popd

	echo "Copying compiler rt..."
	mkdir -p OUTPUT/$arch/lib/clang/$LLVM_VERSION/lib
	cp -r install-rt-$arch/lib/* OUTPUT/$arch/lib/clang/$LLVM_VERSION/lib/
done

# Create version file
echo "${TOOLCHAIN_VERSION}_clang-${LLVM_VERSION}-centos7" > OUTPUT/ToolchainVersion.txt

# Pack everything
pushd OUTPUT
tar czf ../UnrealToolchain.tar.gz *
popd
