@echo off

echo Building clang...

set LLVM_VERSION=700

set SVN_BINARY=%CD%\..\..\..\..\Binaries\ThirdParty\svn\Win64\svn.exe
set CMAKE_BINARY=%CD%\..\..\..\..\Extras\ThirdPartyNotUE\CMake\bin\cmake.exe
set PYTHON_BINARY=%CD%\..\..\..\..\Source\ThirdParty\Python\Win64\python.exe

echo Using SVN: %SVN_BINARY%
echo Using CMake: %CMAKE_BINARY%
echo Using Python: %PYTHON_BINARY%

mkdir clang-build-%LLVM_VERSION%
pushd clang-build-%LLVM_VERSION%

%SVN_BINARY% co http://llvm.org/svn/llvm-project/llvm/tags/RELEASE_%LLVM_VERSION%/final source
pushd source\tools
%SVN_BINARY% co http://llvm.org/svn/llvm-project/cfe/tags/RELEASE_%LLVM_VERSION%/final clang
%SVN_BINARY% co http://llvm.org/svn/llvm-project/lld/tags/RELEASE_%LLVM_VERSION%/final lld
popd

mkdir build
pushd build

%CMAKE_BINARY% -G "Visual Studio 14 Win64" -DCMAKE_INSTALL_PREFIX=..\install -DPYTHON_EXECUTABLE=%PYTHON_BINARY% -DCOMPILER_RT_TEST_TARGET_TRIPLE=x86_64-unknown-linux-gnu ..\source
%CMAKE_BINARY% --build . --target install --config MinSizeRel

rd /q /s copy_over
mkdir copy_over
for %%G in (aarch64-unknown-linux-gnueabi arm-unknown-linux-gnueabihf i686-unknown-linux-gnu x86_64-unknown-linux-gnu) do (
    mkdir copy_over\%%G
    mkdir copy_over\%%G\bin
    mkdir copy_over\%%G\lib
    mkdir copy_over\%%G\lib\clang
    copy "install\bin\clang.exe" copy_over\%%G\bin
    copy "install\bin\clang++.exe" copy_over\%%G\bin
    copy "install\bin\ld.lld.exe" copy_over\%%G\bin
    copy "install\bin\lld.exe" copy_over\%%G\bin
    copy "install\bin\llvm-ar.exe" copy_over\%%G\bin
    copy "install\bin\LTO.dll" copy_over\%%G\bin
    xcopy "install\lib\clang" copy_over\%%G\lib\clang /s /e /y
)

popd

echo If suceeded, files were put in: clang-build-%LLVM_VERSION%\copy_over directory

exit 0

rem "In LLVM 7.0 compiler-rt won't cross compile on Windows, so the code below is not used. I kept it here, because maybe at some point it will be fixed by developers"

if not EXIST %LINUX_MULTIARCH_ROOT%\x86_64-unknown-linux-gnu (
    echo You need to have working Linux cross toolchain in LINUX_MULTIARCH_ROOT
    exit 1
)

echo Building compiler-rt...

pushd source\projects
%SVN_BINARY% co http://llvm.org/svn/llvm-project/compiler-rt/tags/RELEASE_%LLVM_VERSION%/final compiler-rt
popd

echo Using existing Linux cross toolchain from LINUX_MULTIARCH_ROOT

set CLANG_PATH=%CD%/install/bin
set CLANG_PATH=%CLANG_PATH:\=/%
set TARGET=x86_64-unknown-linux-gnu
set TOOLCHAIN_PATH=%LINUX_MULTIARCH_ROOT:\=/%

mkdir build-rt
pushd build-rt

%CMAKE_BINARY% ../source/projects/compiler-rt -G Ninja -DCMAKE_C_COMPILER_ID=Clang -DCMAKE_SYSTEM_NAME=Generic -DCMAKE_C_COMPILER=%CLANG_PATH%/clang.exe -DCMAKE_AR=%CLANG_PATH%/llvm-ar.exe -DCMAKE_NM=%CLANG_PATH%/llvm-nm.exe -DCMAKE_RANLIB=%CLANG_PATH%/llvm-ranlib.exe -DCMAKE_EXE_LINKER_FLAGS="--target=%TARGET% -fuse-ld=lld -L%TOOLCHAIN_PATH%/%TARGET%/lib64 --sysroot=%TOOLCHAIN_PATH%/%TARGET%" -DCMAKE_C_COMPILER_TARGET="%TARGET%"-DCOMPILER_RT_DEFAULT_TARGET_ONLY=ON -DLLVM_CONFIG_PATH=%CLANG_PATH%/llvm-config.exe -DCMAKE_C_FLAGS="--target=%TARGET%  --sysroot=%TOOLCHAIN_PATH%/%TARGET%" -DCMAKE_CXX_FLAGS="--target=%TARGET%  --sysroot=%TOOLCHAIN_PATH%/%TARGET%" -DCMAKE_ASM_FLAGS="--target=%TARGET% --sysroot=%TOOLCHAIN_PATH%/%TARGET%" -DPYTHON_EXECUTABLE=%PYTHON_BINARY% -DCMAKE_INSTALL_PREFIX=..\install-rt -DCMAKE_SYSTEM_NAME="Linux" -DSANITIZER_COMMON_LINK_FLAGS="-fuse-ld=lld"

rem llvm-ar.exe doesn't support "\" in response files, so we need to replace "\" with "/"
powershell -Command "(Get-Content build.ninja).replace('\', '/') | Set-Content build.ninja"

rem Run the build
%CMAKE_BINARY% --build . --target install --config MinSizeRel

echo If suceeded, files were put in: clang-build-%LLVM_VERSION%\install-rt directory

popd
