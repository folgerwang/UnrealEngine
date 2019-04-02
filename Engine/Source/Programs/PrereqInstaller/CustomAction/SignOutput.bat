@echo off

rem ## Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

echo Starting signing of custom action dll

set ErrorCode=0

set SignOutput=%1
set SignTool=%2
set SigningIdentity=%3
set TimeStampServer=%4
set File=%5

if %SignOutput%==false goto :SKIPSIGN

echo SignOutput=%SignOutput%
echo SignTool=%SignTool%
echo SigningIdentity=%SigningIdentity%
echo TimeStampServer=%TimeStampServer%
echo File=%File%

echo %SignTool% sign /a /n %SigningIdentity% /t %TimeStampServer% /v %File%
%SignTool% sign /a /n %SigningIdentity% /t %TimeStampServer% /v %File%
set ErrorCode=%ERRORLEVEL%

:END
echo Sign custom action dll completed with ErrorCode: %ErrorCode%
exit /b %ErrorCode%

:SKIPSIGN
echo Skipped signing of file: %File%
exit /b 0