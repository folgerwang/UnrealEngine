echo off

echo Building dependencies...
pushd ..\centos7_base
call RunMe.bat
popd

set IMAGE_NAME=build_linux_toolchain

echo Building image...
docker build -t %IMAGE_NAME% .

echo Running...
docker run --name temp_%IMAGE_NAME% %IMAGE_NAME%

echo Copying files over...
docker cp temp_%IMAGE_NAME%:/home/buildmaster/UnrealToolchain.tar.gz .

echo Cleaning up...
docker rm temp_%IMAGE_NAME%
