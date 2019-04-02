echo off

echo Building dependencies...
pushd ..\ue421_centos7
call RunMe.bat
popd

set IMAGE_NAME=test_build_ue_centos7_custom

echo Building image...
docker build -t %IMAGE_NAME% .

echo Running...
docker run --name temp_%IMAGE_NAME% %IMAGE_NAME%

echo Cleaning up...
docker rm temp_%IMAGE_NAME%
