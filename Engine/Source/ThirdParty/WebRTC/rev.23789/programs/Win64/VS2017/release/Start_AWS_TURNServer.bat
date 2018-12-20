:: Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
@echo off

pushd %~dp0

title TURN
Powershell.exe -executionpolicy unrestricted -File Start_AWS_TURNServer.ps1

popd