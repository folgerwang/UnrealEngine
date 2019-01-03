:: Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
@echo off

pushd %~dp0

title STUN
stunserver.exe 0.0.0.0:19302

popd