// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "CoreFwd.h"
#include "Logging/LogMacros.h"
#include "Common/TcpSocketBuilder.h"
#include "string.h"

class FIOSDeviceOutputReaderRunnable;
class FIOSTargetDevice;
class FIOSTargetDeviceOutput;




inline FIOSDeviceOutputReaderRunnable::FIOSDeviceOutputReaderRunnable(const FTargetDeviceId InDeviceId, FOutputDevice* InOutput)
	: StopTaskCounter(0)
	, DeviceId(InDeviceId)
	, Output(InOutput)
	, DSReadPipe(nullptr)
	, DSWritePipe(nullptr)
	, DSCommander(nullptr)
{
}

inline bool FIOSDeviceOutputReaderRunnable::StartDSCommander()
{
	if (DSCommander)
	{
		DSCommander->Stop();
		delete DSCommander;
	}
	Output->Serialize(TEXT("Starting listening ....."), ELogVerbosity::Log, NAME_None);
	Output->Serialize(*DeviceId.GetDeviceName(), ELogVerbosity::Log, NAME_None);
	FString Command = FString::Printf(TEXT("listentodevice -device %s"), *DeviceId.GetDeviceName());
	uint8* DSCommand = (uint8*)TCHAR_TO_UTF8(*Command);
	DSCommander = new FTcpDSCommander(DSCommand, strlen((const char*)DSCommand), DSWritePipe);
	return DSCommander->IsValid();
}

inline bool FIOSDeviceOutputReaderRunnable::Init(void) 
{ 
	FPlatformProcess::CreatePipe(DSReadPipe, DSWritePipe);
	return StartDSCommander();
}

inline void FIOSDeviceOutputReaderRunnable::Exit(void) 
{
	StopTaskCounter.Increment();
	if (DSCommander)
	{
		DSCommander->Stop();
		delete DSCommander;
		DSCommander = nullptr;
	}
	if (DSReadPipe && DSWritePipe)
	{
		FPlatformProcess::ClosePipe(DSReadPipe, DSWritePipe);
		DSReadPipe = nullptr;
		DSWritePipe = nullptr;
	}
}

inline void FIOSDeviceOutputReaderRunnable::Stop(void)
{
	StopTaskCounter.Increment();
}

inline uint32 FIOSDeviceOutputReaderRunnable::Run(void)
{
	FString DSOutput;
	Output->Serialize(TEXT("Starting Output"), ELogVerbosity::Log, NAME_None);
	while (StopTaskCounter.GetValue() == 0 && DSCommander->IsValid())
	{
		if (DSCommander->IsStopped() || !DSCommander->IsValid())
		{
			// When user plugs out USB cable DS process stops
			// Keep trying to restore DS connection until code that uses this object will not kill us
			Output->Serialize(TEXT("Trying to restore connection to device..."), ELogVerbosity::Log, NAME_None);
			if (StartDSCommander())
			{
				FPlatformProcess::Sleep(5.0f);
			}
			else
			{
				Output->Serialize(TEXT("Failed to start DS commander"), ELogVerbosity::Log, NAME_None);
			}
		}
		else
		{
			DSOutput.Append(FPlatformProcess::ReadPipe(DSReadPipe));

			if (DSOutput.Len() > 0)
			{
				TArray<FString> OutputLines;
				DSOutput.ParseIntoArray(OutputLines, TEXT("\n"), false);

				if (!DSOutput.EndsWith(TEXT("\n")))
				{
					// partial line at the end, do not serialize it until we receive remainder
					DSOutput = OutputLines.Last();
					OutputLines.RemoveAt(OutputLines.Num() - 1);
				}
				else
				{
					DSOutput.Reset();
				}

				for (int32 i = 0; i < OutputLines.Num(); ++i)
				{
					if (OutputLines[i].Contains(TEXT("[UE4]"), ESearchCase::CaseSensitive))
					{
						Output->Serialize(*OutputLines[i], ELogVerbosity::Log, NAME_None);
					}
				}
			}
			
			FPlatformProcess::Sleep(0.1f);
		}
	}

	return 0;
}

inline bool FIOSTargetDeviceOutput::Init(const FIOSTargetDevice& TargetDevice, FOutputDevice* Output)
{
	check(Output);
	// Output will be produced by background thread
	check(Output->CanBeUsedOnAnyThread());
	
	DeviceId = TargetDevice.GetId();
	DeviceName = TargetDevice.GetName();

	Output->Serialize(TEXT("Creating FIOSTargetDeviceOutput ....."), ELogVerbosity::Log, NAME_None);
		
	auto* Runnable = new FIOSDeviceOutputReaderRunnable(DeviceId, Output);
	DeviceOutputThread = TUniquePtr<FRunnableThread>(FRunnableThread::Create(Runnable, TEXT("FIOSDeviceOutputReaderRunnable")));
	return true;
}
