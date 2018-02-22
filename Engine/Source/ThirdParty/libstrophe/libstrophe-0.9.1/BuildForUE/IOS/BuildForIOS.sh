#!/bin/sh

# Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

../../../../../../Extras/ThirdPartyNotUE/CMake/bin/cmake -G Xcode -DSOCKET_IMPL=../../src/sock.c -DDISABLE_TLS=0 -DOPENSSL_PATH=../../../OpenSSL/1_0_1s/include/IOS -DCMAKE_CXX_FLAGS_DEBUG="-O0 -DXML_STATIC" -DCMAKE_CXX_FLAGS_RELEASE="-O3 -DXML_STATIC" ..

echo
echo
echo You will now need to convert the Xcode project to use iOS SDK instead of Mac, then build it!
echo
echo

