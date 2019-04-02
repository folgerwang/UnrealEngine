echo off

REM echo Building dependencies...
REM pushd ..\centos7_base
REM call RunMe.bat
REM popd

set IMAGE_NAME=build_linux_python

echo Building image...
docker build -t %IMAGE_NAME% .

echo Running...
docker run --name temp_%IMAGE_NAME% %IMAGE_NAME%

echo Copying files over...
docker cp temp_%IMAGE_NAME%:/home/buildmaster/Python-Linux.tar.gz .

echo Cleaning up...
docker rm temp_%IMAGE_NAME%
