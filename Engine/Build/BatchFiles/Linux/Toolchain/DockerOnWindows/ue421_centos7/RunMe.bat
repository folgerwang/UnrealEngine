echo off

echo Building dependencies...
pushd ..\centos7_base
call RunMe.bat
popd

set IMAGE_NAME=ue421_centos7

echo Building image...
docker build -t %IMAGE_NAME% .
