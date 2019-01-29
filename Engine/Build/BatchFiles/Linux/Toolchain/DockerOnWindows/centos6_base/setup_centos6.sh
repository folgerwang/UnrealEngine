#! /bin/bash

# Create non-privileged user and workspace
adduser buildmaster
mkdir -p /build
chown buildmaster:nobody -R /build

# Install required packages
yum install -y epel-release centos-release-scl scl-utils
yum install -y ncurses-devel patch devtoolset-7 cmake3 git wget gcc-c++ gperf bison flex texinfo help2man glibc-static libstdc++-devel libtool xz python27

# Install newer autoconf
wget ftp://ftp.pbone.net/mirror/ftp5.gwdg.de/pub/opensuse/repositories/home:/monkeyiq:/centos6updates/CentOS_CentOS-6/noarch/autoconf-2.69-12.2.noarch.rpm
yum install -y autoconf-2.69-12.2.noarch.rpm
