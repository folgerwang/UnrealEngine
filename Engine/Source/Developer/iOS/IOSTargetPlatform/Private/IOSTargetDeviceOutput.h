// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/ITargetDeviceOutput.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/ThreadSafeCounter.h"

class FIOSTargetDevice;

class FIOSDeviceOutputReaderRunnable : public FRunnable
{
public:
	FIOSDeviceOutputReaderRunnable(const FTargetDeviceId InDeviceId, FOutputDevice* Output);
	
	// FRunnable interface.
	virtual bool Init(void) override;
	virtual void Exit(void) override; 
	virtual void Stop(void) override;
	virtual uint32 Run(void) override;

private:
	bool StartDSProcess(void);

private:
	// > 0 if we've been asked to abort work in progress at the next opportunity
	FThreadSafeCounter	StopTaskCounter;
	
	FTargetDeviceId		DeviceId;
	FOutputDevice*		Output;
	
	void*				DSReadPipe;
	void*				DSWritePipe;
	FProcHandle			DSProcHandle;
};

/**
 * Implements a IOS target device output.
 */
class FIOSTargetDeviceOutput : public ITargetDeviceOutput
{
public:
	bool Init(const FIOSTargetDevice& TargetDevice, FOutputDevice* Output);
	
private:
	TUniquePtr<FRunnableThread>						DeviceOutputThread;
	FTargetDeviceId									DeviceId;
	FString											DeviceName;
};

#include "IOSTargetDeviceOutput.inl"
