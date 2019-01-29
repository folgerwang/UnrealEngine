#! /bin/bash

LLVM_VERSION=700

if [ "`ls /var/.uecontainerinit*`" == "" ]; then
  echo "You must run this script via ContainerBuildThirdParty.sh"
  echo "e.g. sudo ./ContainerBuildThirdParty.sh -b LLVMCompilerRt"
  exit 1
fi

#rm -rf llvm-build
mkdir -p llvm-build
cd llvm-build

svn co http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_$LLVM_VERSION/final source
cd source/projects
svn co http://llvm.org/svn/llvm-project/compiler-rt/tags/RELEASE_$LLVM_VERSION/final compiler-rt
cd ../..

mkdir build
cd build

scl enable llvm-toolset-7 'cmake3 ../source/ -DCMAKE_INSTALL_PREFIX=../install-rt'
scl enable llvm-toolset-7 'cmake3 --build . --target install --config MinSizeRel -- -j8'
