echo off

set IMAGE_NAME=ubuntu1804_base

echo Building image...
docker build -t %IMAGE_NAME% .
