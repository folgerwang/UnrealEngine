#! /bin/bash

# Create non-privileged user and workspace
adduser --disabled-password --gecos "" buildmaster

# Install required packages
apt update
apt install -y gawk make cmake git wget gperf bison flex texinfo help2man file unzip autoconf libtool-bin mingw-w64 libncurses5-dev xz-utils g++ zip python
