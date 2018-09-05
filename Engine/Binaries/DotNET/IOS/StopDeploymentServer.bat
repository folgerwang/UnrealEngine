@echo off
if not exist DeploymentServer.exe goto end
if not exist DeploymentInterface.dll goto end
if not exist MobileDeviceInterface.dll goto end
DeploymentServer.exe stop
:end
exit /b 0
