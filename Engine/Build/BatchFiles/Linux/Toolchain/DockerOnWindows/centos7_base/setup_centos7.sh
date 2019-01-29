#! /bin/bash

# Create non-privileged user and workspace
adduser buildmaster
mkdir -p /build
chown buildmaster:nobody -R /build

# Install required packages
yum install -y epel-release centos-release-scl
yum install -y ncurses-devel patch llvm-toolset-7 llvm-toolset-7-llvm-devel make cmake3 git wget which gcc-c++ gperf bison flex texinfo bzip2 help2man glibc-static libstdc++-devel libstdc++-static file unzip autoconf libtool mingw64-gcc mingw64-gcc-c++
