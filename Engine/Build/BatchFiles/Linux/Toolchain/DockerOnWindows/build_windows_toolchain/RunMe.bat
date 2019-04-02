echo off

set LLVM_VERSION=7.0.1
set TOOLCHAIN_VERSION=v13

set SVN_BINARY=%CD%\..\..\..\..\..\..\Binaries\ThirdParty\svn\Win64\svn.exe
set CMAKE_BINARY=%CD%\..\..\..\..\..\..\Extras\ThirdPartyNotUE\CMake\bin\cmake.exe
set PYTHON_BINARY=%CD%\..\..\..\..\..\..\Source\ThirdParty\Python\Win64\python.exe
set NSIS_BINARY=C:\Program Files (x86)\NSIS\Bin\makensis.exe

echo Checking if Linux toolchain is already built...
if exist ..\build_linux_toolchain\UnrealToolchain.tar.gz (
    copy ..\build_linux_toolchain\UnrealToolchain.tar.gz .
) else (
    echo You need to build Linux native toolchain first, from build_linux_toolchain directory
    exit 1
)

echo Building dependencies...
pushd ..\ubuntu1804_base
call RunMe.bat
popd

set IMAGE_NAME=build_windows_toolchain

echo Building image...
docker build -t %IMAGE_NAME% .

echo Running...
docker run --name temp_%IMAGE_NAME% %IMAGE_NAME%

echo Copying files over...
docker cp temp_%IMAGE_NAME%:/home/buildmaster/UnrealToolchain.zip .

echo Cleaning up...
docker rm temp_%IMAGE_NAME%

echo Building clang...

echo Using SVN: %SVN_BINARY%
echo Using CMake: %CMAKE_BINARY%
echo Using Python: %PYTHON_BINARY%

rem We need to build in a directory with shorter path, so we avoid hitting path max limit.
set ROOT_DIR=%CD%
rd /s /q %TEMP%\clang-build-%LLVM_VERSION%
mkdir %TEMP%\clang-build-%LLVM_VERSION%
pushd %TEMP%\clang-build-%LLVM_VERSION%

mkdir install

rd /s /q OUTPUT
mkdir OUTPUT
powershell.exe "& Expand-Archive %ROOT_DIR%/UnrealToolchain.zip OUTPUT"

set RELEASE=%LLVM_VERSION:.=%
%SVN_BINARY% co http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_%RELEASE%/final source
pushd source\tools
%SVN_BINARY% co http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_%RELEASE%/final clang
%SVN_BINARY% co http://llvm.org/svn/llvm-project/lld/tags/RELEASE_%RELEASE%/final lld
popd

mkdir build
pushd build

%CMAKE_BINARY% -G "Visual Studio 14 Win64" -DCMAKE_INSTALL_PREFIX="..\install" -DPYTHON_EXECUTABLE="%PYTHON_BINARY%" "..\source"
%CMAKE_BINARY% --build . --target install --config MinSizeRel

popd

for %%G in (aarch64-unknown-linux-gnueabi arm-unknown-linux-gnueabihf x86_64-unknown-linux-gnu) do (
    mkdir OUTPUT\%%G
    mkdir OUTPUT\%%G\bin
    mkdir OUTPUT\%%G\lib
    mkdir OUTPUT\%%G\lib\clang
    copy "install\bin\clang.exe" OUTPUT\%%G\bin
    copy "install\bin\clang++.exe" OUTPUT\%%G\bin
    copy "install\bin\ld.lld.exe" OUTPUT\%%G\bin
    copy "install\bin\lld.exe" OUTPUT\%%G\bin
    copy "install\bin\llvm-ar.exe" OUTPUT\%%G\bin
    copy "install\bin\LTO.dll" OUTPUT\%%G\bin
    xcopy "install\lib\clang" OUTPUT\%%G\lib\clang /s /e /y
)

rem Create version file
echo %TOOLCHAIN_VERSION%_clang-%LLVM_VERSION%-centos7> OUTPUT\ToolchainVersion.txt

echo Packing final toolchain...
powershell.exe "& Compress-Archive -DestinationPath %ROOT_DIR%/%TOOLCHAIN_VERSION%_clang-%LLVM_VERSION%-centos7.zip -Path OUTPUT/* -Force"

if exist "%NSIS_BINARY%" (
    echo Creating installer...
    copy %ROOT_DIR%\InstallerScript.nsi .
    "%NSIS_BINARY%" /V4 InstallerScript.nsi
    move %TOOLCHAIN_VERSION%_clang-%LLVM_VERSION%-centos7.exe %ROOT_DIR%
) else (
    echo Skipping installer creation, because makensis.exe was not found.
    echo Install Nullsoft.
)

popd
