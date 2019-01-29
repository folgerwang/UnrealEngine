echo off

set IMAGE_NAME=centos6_base

echo Building image...
docker build -t %IMAGE_NAME% .
