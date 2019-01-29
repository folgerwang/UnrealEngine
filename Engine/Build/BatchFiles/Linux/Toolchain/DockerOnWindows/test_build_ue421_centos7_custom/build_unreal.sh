#! /bin/bash

# Erase existing toolchain
pushd Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v12_clang-6.0.1-centos7
rm -rf *

# Unpack custom one
tar xf ../UnrealToolchain.tar.gz

popd

# Build
bash ./GenerateProjectFiles.sh
make TestPAL
Engine/Binaries/Linux/TestPAL
