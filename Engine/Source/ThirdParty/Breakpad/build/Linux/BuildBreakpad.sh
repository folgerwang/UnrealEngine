#!/bin/bash

set -e

export CXX=clang++
TEMP_DIR="/tmp/local-breakpad-$BASHPID"
UE_THIRD_PARTY_DIR=`cd "../../../"; pwd`
BASE_DIR=`cd "../../"; pwd`
BINARY_INSTALL="$BASE_DIR/../../../Binaries/Linux/"

echo "UE_THIRD_PARTY_DIR=$UE_THIRD_PARTY_DIR"
echo "BINARY_INSTALL=$BINARY_INSTALL"

cd ../../
export CXXFLAGS="-DDUMP_SYMS_WITH_EPIC_EXTENSIONS -O3 -fPIC -std=c++11 -stdlib=libc++ -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include -I$UE_THIRD_PARTY_DIR/Linux/LibCxx/include/c++/v1"
export LDFLAGS="-nodefaultlibs -L$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/"
export LIBS="$UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++.a $UE_THIRD_PARTY_DIR/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu/libc++abi.a -lm -lc -lgcc_s -lgcc -lpthread"
./configure --prefix=$TEMP_DIR --disable-processor
make install

echo "=========== Copy dump_syms Binary ============"
echo "cp $TEMP_DIR/bin/dump_syms $BINARY_INSTALL"
cp $TEMP_DIR/bin/dump_syms $BINARY_INSTALL
