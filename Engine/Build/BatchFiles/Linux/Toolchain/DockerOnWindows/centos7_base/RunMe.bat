echo off

set IMAGE_NAME=centos7_base

echo Building image...
docker build -t %IMAGE_NAME% .
