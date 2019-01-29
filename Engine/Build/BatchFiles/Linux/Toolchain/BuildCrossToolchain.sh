#!/bin/bash
# by rcl^epic

SelfName=`basename $0`
PlatformBuildPrefix=host-build
OutputFolder=CrossToolchainMultiarch
ToolchainVersion=v12_clang-6.0.1-centos7

mkdir -p ./src

# TARGET_ARCH set in BuildThirdParty.sh conflicts with definition of LINK.o when making toolchains
# TODO - Save this and pass to BuildToolchainTargetPlatform 
unset TARGET_ARCH

# prereq:
# mercurial autoconf gperf bison flex libtool ncurses-dev

HOSTPLATFORMS=("linux x86_64-pc-linux-gnu" "win64 x86_64-w64-mingw32" "darwin x86_64-apple-darwin")
ARCHES=("x86_64 x86_64-unknown-linux-gnu" "i686 i686-unknown-linux-gnu" "ARM32 arm-unknown-linux-gnueabihf" "ARM64 aarch64-unknown-linux-gnueabi")
ARCHELIBDIRECTORIES=("x86_64 lib64" "i686 lib" "ARM32 lib" "ARM64 lib64")

BuildPlatform=`uname | awk '{print tolower($0)}'`

if [ $BuildPlatform == "linux" ]; then
        if [ "`ls /var/.uecontainerinit*`" == "" ]; then
		echo "You must run this script via ContainerBuildThirdParty.sh"
		echo "e.g. sudo ./ContainerBuildThirdParty.sh -b Toolchain"
		exit
	fi
fi

ConvertHostPlatformFormat()
{
	local Platform=$1
	if [ -z $Platform ]; then
		Platform="linux"
	fi

	for platform in "${HOSTPLATFORMS[@]}";
	do
		platform_split=($platform)
		if [ "${platform_split[0]}" == "$Platform" ]; then
			echo ${platform_split[1]}
		fi
	done
}


ConvertArchFormat()
{
	local Arch=$1
	if [ -z $Arch ]; then
		Arch="x86_64"
	fi

	for arch in "${ARCHES[@]}";
	do
		arch_split=($arch)
		if [ "${arch_split[0]}" == "$Arch" ]; then
			echo ${arch_split[1]}
		fi
	done
}

GetLibDirForArch()
{
	local Arch=$1
	if [ -z $Arch ]; then
		Arch="x86_64"
	fi

	for dir in "${ARCHELIBDIRECTORIES[@]}";
	do
		dir_split=($dir)
		if [ "${dir_split[0]}" == "$Arch" ]; then
			echo ${dir_split[1]}
		fi
	done
}

#############
# build recent ct-ng
Buildctng()
{
	rm -rf crosstool-ng
	git clone http://github.com/RCL/crosstool-ng -b 1.22

	pushd crosstool-ng

        if [ ! `ls /var/.uecontainerinit*` == "" ]; then
  		# CentOS 6 does not have autoconf 2.67
        	sed -i -e 's/2\.67/2\.63/g' configure.ac
		local DESCRIBE=`git describe --always --dirty`
		sed -i -e "s/m4_esyscmd_s(\[git describe --always --dirty\])/$DESCRIBE/g" configure.ac
        fi
	
	./bootstrap
	[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1
	
	./configure --enable-local
	[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

	sed -i -e 's/multiprecision.org\/mpc\/download/multiprecision.org\/downloads/g' scripts/build/companion_libs/140-mpc.sh
	grep -lr "releases.linaro.org" scripts/build/ | xargs sed -i -e 's/releases.linaro.org/releases.linaro.org\/archive/g'

	make -j8
	[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

	popd
}

#############
# rearrange files the way we need them
OutputToolchain()
{
	local Output=$1
	local ToolchainDir=$(pwd)
	local Platform=$2
	local Arch=$3
	local ArchDirectory=`ConvertArchFormat $Arch`
	local ArchLibDirectory=`GetLibDirForArch $Arch`

	if [ -d $Output/$Platform/$ArchDirectory ]; then
		chmod -R 777 $Output/$Platform/$ArchDirectory
		rm -rf $Output/$Platform/$ArchDirectory
	fi

	mkdir -p $Output/$Platform/$ArchDirectory
	cp -r $ToolchainDir/x-tools/$Platform/$ArchDirectory $Output/$Platform
	[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

	mkdir -p $Output/$Platform/$ArchDirectory
	
	pushd $Output/$Platform/$ArchDirectory
	find . -type d -exec chmod +w {} \;

	# Copy in clang if it was compiled for Platform
	if [ -d $ToolchainDir/x-tools/$Platform/bin ]; then
		cp -r $ToolchainDir/x-tools/$Platform/bin .
		cp -r $ToolchainDir/x-tools/$Platform/lib .
	fi

	cp -r -L $ArchDirectory/include .
	cp -r -L $ArchDirectory/sysroot/$ArchLibDirectory .
	mkdir -p usr
	cp -r -L $ArchDirectory/sysroot/usr/$ArchLibDirectory usr/
	cp -r -L $ArchDirectory/sysroot/usr/include usr/
	rm -rf $ArchDirectory

	rm -f build.log.bz2

	echo "$ToolchainVersion" > ../ToolchainVersion.txt
	popd
}

#############
# rearrange files the way we need them
OutputHostToolchain()
{
	# TODO Remove duplication of OutputToolchain and OutputHostToolchain
	local Output=$1
	local ToolchainDir=$2
	local Platform=$3
	local ArchDirectory=`ConvertHostPlatformFormat $Platform`

	if [ -d $Output ]; then
		chmod -R 777 $Output
		rm -rf $Output
	fi

	mkdir -p $Output
	cp -r $ToolchainDir/x-tools/$Platform/$ArchDirectory/* $Output/
	[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1
	
	pushd $Output
	find . -type d -exec chmod +w {} \;
	cp -r -L $ArchDirectory/include .
	cp -r -L $ArchDirectory/sysroot/lib .
	mkdir -p usr
	cp -r -L $ArchDirectory/sysroot/usr/lib usr
	cp -r -L $ArchDirectory/sysroot/usr/include usr
	rm -rf $ArchDirectory
	popd
}


#############
# build BuildPlatform-hosted toolchain that targets Platform
BuildToolchainTargetPlatform()
{
	local Platform=$1
	local BuildPlatformTemp="$BuildPlatform-$PlatformBuildPrefix-$Platform"

	rm -rf $BuildPlatformTemp
	mkdir $BuildPlatformTemp

	cp $BuildPlatform-host-$Platform.config $BuildPlatformTemp/.config 
	[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

	pushd $BuildPlatformTemp
	../crosstool-ng/ct-ng build
	[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

	popd
}

#############
# build Platform-hosted clang 
BuildPlatformClang()
{
	local ToolchainDir=$(pwd)
	local Platform=$1
	local BuildPlatformBuildTemp="$BuildPlatform-$PlatformBuildPrefix-$Platform-LLVM"
	local HostPlatformTriple=`ConvertHostPlatformFormat $Platform`

	if [ $Platform == "win64" ]; then
		echo "Currently unable to build clang for windows using mingw32. Please build manually as per README.md"
		return 0
	fi

	local LLVM_VERSION=6.0.1
	local LLVM=llvm-$LLVM_VERSION
	local CLANG=cfe-$LLVM_VERSION
	local LLD=lld-$LLVM_VERSION
	if [ ! -f $ToolchainDir/src/$LLVM.src.tar.xz ]; then
		wget http://releases.llvm.org/$LLVM_VERSION/$LLVM.src.tar.xz -P $ToolchainDir/src/
	fi
	if [ ! -f $ToolchainDir/src/$CLANG.src.tar.xz ]; then
		wget http://releases.llvm.org/$LLVM_VERSION/$CLANG.src.tar.xz -P $ToolchainDir/src/
	fi
	if [ ! -f $ToolchainDir/src/$LLD.src.tar.xz ]; then
		wget http://releases.llvm.org/$LLVM_VERSION/$LLD.src.tar.xz -P $ToolchainDir/src/
	fi
	if [ ! -f $ToolchainDir/src/Python-2.7.tgz ]; then
		wget https://www.python.org/ftp/python/2.7/Python-2.7.tgz -P $ToolchainDir/src/
	fi
	if [ ! -f $ToolchainDir/src/cmake-3.10.1.tar.gz ];then
		wget https://cmake.org/files/v3.10/cmake-3.10.1.tar.gz -P $ToolchainDir/src/
	fi

	rm -rf $BuildPlatformBuildTemp
	mkdir $BuildPlatformBuildTemp

	pushd $BuildPlatformBuildTemp
	
	if [ ! -d $ToolchainDir/python ]; then
		tar -xvf $ToolchainDir/src/Python-2.7.tgz 
		pushd Python-2.7
		./configure --prefix=$ToolchainDir/python --enable-shared
		make -j8
		make install	
		popd
	fi

	local LinuxSysRoot=$ToolchainDir/x-tools/linux/x86_64-pc-linux-gnu/x86_64-pc-linux-gnu/sysroot
	local LibCxxStaticDir=$ToolchainDir/../../../../Source/ThirdParty/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu 
 	local LibCxxIncludeDir=$ToolchainDir/../../../../Source/ThirdParty/Linux/LibCxx/include/c++/v1
	export LD_LIBRARY_PATH=$ToolchainDir/x-tools/linux/x86_64-pc-linux-gnu/x86_64-pc-linux-gnu/lib
	local LinuxToolsPrefix=$ToolchainDir/x-tools/linux/x86_64-pc-linux-gnu/bin/x86_64-pc-linux-gnu
	local LinuxBuildToolsIncDir=$ToolchainDir/linux-host-build-linux/.build/x86_64-pc-linux-gnu/buildtools/include
	local LinuxBuildToolsLibDir=$ToolchainDir/linux-host-build-linux/.build/x86_64-pc-linux-gnu/buildtools/lib

	# CentOS cmake version is too old for clang, the new version of cmake also requires c++11 which CentOS's GCC does not support
	# Rebuild cmake with temp linux toolchain's newer GCC.
	if [ ! -d $ToolchainDir/cmake ]; then
		tar -xvf $ToolchainDir/src/cmake-3.10.1.tar.gz
		pushd cmake-3.10.1
		if [ $BuildPlatform == "linux" ]; then			
			if [ ! -d $ToolchainDir/x-tools/linux ]; then
				echo "Please build Linux Platform toolchain first"
				exit
			fi
	        	CFLAGS="--sysroot=$LinuxSysRoot -I$LinuxBuildToolsIncDir/ncurses -I$LinuxBuildToolsIncDir -L$LinuxBuildToolsLibDir" CC=$LinuxToolsPrefix-gcc CXXFLAGS="--sysroot=$LinuxSysRoot -I$LinuxBuildToolsIncDir/ncurses -I$LinuxBuildToolsIncDir -L$LinuxBuildToolsLibDir" CXX=$LinuxToolsPrefix-g++ ./bootstrap --prefix=$ToolchainDir/cmake
		else
			./bootstrap --prefix=$ToolchainDir/cmake
		fi

		make -j8
		make install
		popd
	fi
	
	export PATH=$ToolchainDir/python/bin:$ToolchainDir/cmake/bin:$PATH
	export PKG_CONFIG_PATH=$ToolchainDir/python/lib/pkgconfig:$PKG_CONFIG_PATH
	export LD_LIBRARY_PATH=$ToolchainDir/python/lib:$LD_LIBRARY_PATH

	mkdir -p llvm
	tar -xvf $ToolchainDir/src/$LLVM.src.tar.xz --strip-components 1 -C llvm
	mkdir -p llvm/tools/clang
	tar -xvf $ToolchainDir/src/$CLANG.src.tar.xz --strip-components 1 -C llvm/tools/clang/
	mkdir -p llvm/tools/lld
	tar -xvf $ToolchainDir/src/$LLD.src.tar.xz --strip-components 1 -C llvm/tools/lld/
	
	pushd llvm

	mkdir -p build
	pushd build

	# CentOS GCC does not have the support for libc++ and we need this to build a shippable clang with support for a modern C++ api.
	# Also the default ld does not support sysroot therefore we cannot use this either. 
	# We first need to build clang against libstdc++ (for Linux) to then use it to build a clang against libc++ for platform.
	local ClangDir=/usr/bin
	if [ $BuildPlatform == "linux" ]; then
		if [ ! -d $ToolchainDir/clang ]; then
			if [ ! -d $ToolchainDir/x-tools/linux ]; then
				echo "Please build Linux Platform toolchain first"
				exit
			fi
			cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=$ToolchainDir/clang -DCMAKE_SYSROOT=$LinuxSysRoot -DCMAKE_CXX_COMPILER=$LinuxToolsPrefix-g++ -DCMAKE_C_COMPILER=$LinuxToolsPrefix-gcc -DCMAKE_CROSSCOMPILING=True -DLLVM_TARGETS_TO_BUILD="X86" -DCMAKE_C_FLAGS="--sysroot=$LinuxSysRoot" -DLLVM_ENABLE_LIBXML2=OFF -DCMAKE_CXX_FLAGS="--sysroot=$LinuxSysRoot" -G "Unix Makefiles" ..
			make -j8
			make install
		fi
		ClangDir=$ToolchainDir/clang/bin
	fi
	
	local SysRoot=$ToolchainDir/x-tools/$Platform-host
	OutputHostToolchain $SysRoot $ToolchainDir $Platform 
	local Linker="-fuse-ld=$ClangDir/ld.lld"

	# Finally build clang against a static libc++ for our target platform this should be shippable without any C++ dependency issues
	cmake -DCMAKE_BUILD_TYPE=Release -DLLVM_ENABLE_LIBCXX=1 -DCMAKE_SYSROOT=$SysRoot -DCMAKE_CXX_COMPILER=$ClangDir/clang++ -DCMAKE_C_COMPILER=$ClangDir/clang -DCMAKE_CROSSCOMPILING=True -DLLVM_TARGETS_TO_BUILD="AArch64;ARM;X86" -DCMAKE_C_FLAGS="$Linker -target $HostPlatformTriple -Wno-unused-command-line-argument -pthread" -DLLVM_ENABLE_LIBXML2=OFF -DCMAKE_CXX_FLAGS="$Linker -target $HostPlatformTriple -stdlib=libc++ -lc++abi -I$LibCxxIncludeDir -L$LibCxxStaticDir -Wno-unused-command-line-argument -pthread" -G "Unix Makefiles" ..
	make -j8

	# Copy clang into toolchain platform root
	mkdir -p $ToolchainDir/x-tools/$Platform/bin
	mkdir -p $ToolchainDir/x-tools/$Platform/lib

	local BinSuffix=""
	if [ -f bin/clang.exe ]; then
		BinSuffix=".exe"
	fi

	cp bin/clang$BinSuffix $ToolchainDir/x-tools/$Platform/bin/
	cp bin/clang++$BinSuffix $ToolchainDir/x-tools/$Platform/bin/
	cp bin/lld$BinSuffix $ToolchainDir/x-tools/$Platform/bin/
	cp bin/ld.lld$BinSuffix $ToolchainDir/x-tools/$Platform/bin/
	cp bin/llvm-ar$BinSuffix $ToolchainDir/x-tools/$Platform/bin/
	cp -r lib/clang $ToolchainDir/x-tools/$Platform/lib/

	unset LD_LIBRARY_PATH

	popd
	popd
	popd
}

#############
# build Platform-hosted toolchain that targets Linux (ARCH)
BuildPlatformToolchainTargetLinux()
{
	local Platform=$1
	local Arch=$2
	local PlatformBuildTemp="$Platform-$PlatformBuildPrefix-$Arch"

	rm -rf $PlatformBuildTemp
	mkdir $PlatformBuildTemp

	cp $Platform-host_$Arch.config $PlatformBuildTemp/.config
	[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

	pushd $PlatformBuildTemp

	../crosstool-ng/ct-ng build
	[ $? -ne 0 ] && echo "$SelfName: Error encountered, exiting at line $LINENO" && exit 1

	popd

	OutputToolchain $OutputFolder $Platform $Arch
}

#############
# Build and output linux toolchains for a specific platform
BuildToolchainsForPlatform()
{
	local Platform=$1

	BuildToolchainTargetPlatform $Platform 
	BuildPlatformClang $Platform
	BuildPlatformToolchainTargetLinux $Platform x86_64
	BuildPlatformToolchainTargetLinux $Platform i686
	BuildPlatformToolchainTargetLinux $Platform ARM32 
	BuildPlatformToolchainTargetLinux $Platform ARM64
}

Buildctng
BuildToolchainsForPlatform $BuildPlatform
if [ $BuildPlatform != "linux" ]; then
	BuildToolchainsForPlatform linux
fi
BuildToolchainsForPlatform win64
