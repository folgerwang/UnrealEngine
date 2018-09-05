@echo off
pushd %~dp0

Powershell.exe -executionpolicy unrestricted -File Start_AWS_WebRTCProxy.ps1

popd