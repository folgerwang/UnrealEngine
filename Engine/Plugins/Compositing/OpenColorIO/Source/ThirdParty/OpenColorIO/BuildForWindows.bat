@echo off

rem Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

rem Setup part
setlocal

set OCIO_LIB_NAME=OpenColorIO-v1.1.0


IF NOT EXIST .\build GOTO NO_BUILD_DIR
rd /S /Q .\build

:NO_BUILD_DIR
mkdir build

IF NOT EXIST %OCIO_LIB_NAME% (
        echo Extracting %OCIO_LIB_NAME%.zip...
    powershell -Command "Add-Type -AssemblyName System.IO.Compression.FileSystem; [System.IO.Compression.ZipFile]::ExtractToDirectory('%OCIO_LIB_NAME%.zip', '.\build')"
)


cd /d .\build

set OCIO_ROOT_FOLDER=.\%OCIO_LIB_NAME%
set DISTRIBUTION_ROOT_FOLDER=..\distribution


rem Configure OCIO cmake and launch a release build
echo Configuring x64 build...
cmake  -G "Visual Studio 15 2017 Win64" -DOCIO_BUILD_SHARED=ON -DOCIO_BUILD_STATIC=OFF -DOCIO_BUILD_TRUELIGHT=OFF -DOCIO_BUILD_APPS=OFF -DOCIO_BUILD_NUKE=OFF -DOCIO_BUILD_DOCS=OFF -DOCIO_BUILD_TESTS=OFF -DOCIO_BUILD_PYGLUE=OFF -DOCIO_BUILD_JNIGLUE=OFF -DOCIO_STATIC_JNIGLUE=OFF -DOCIO_USE_BOOST_PTR=OFF -DOCIO_PYGLUE_LINK=OFF -DCMAKE_INSTALL_PREFIX:PATH=.\install .\%OCIO_ROOT_FOLDER%

rem Remove previous distribution file
IF NOT EXIST %DISTRIBUTION_ROOT_FOLDER% GOTO NO_DISTRIBUTION_DIR
rd /S /Q %DISTRIBUTION_ROOT_FOLDER%

:NO_DISTRIBUTION_DIR

echo Building x64 Release build...
cmake --build .\ --config Release --target INSTALL

echo Copying distribution files...
xcopy .\install\install\bin\OpenColorIO.dll %DISTRIBUTION_ROOT_FOLDER%\..\..\..\..\Binaries\ThirdParty\Win64\* /Y
xcopy .\install\install\include\OpenColorIO\* %DISTRIBUTION_ROOT_FOLDER%\include\OpenColorIO\* /Y
xcopy .\install\install\lib\OpenColorIO.lib %DISTRIBUTION_ROOT_FOLDER%\lib\Win64\* /Y


pause

