// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "MIDIDeviceManager.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectHash.h"
#include "MIDIDeviceController.h"
#include "MIDIDeviceLog.h"
#include "portmidi.h"

#define LOCTEXT_NAMESPACE "MIDIDeviceManager"


bool UMIDIDeviceManager::bIsInitialized = false;
TArray<FMIDIDeviceInfo> UMIDIDeviceManager::MIDIInputDevicesInfo;
TArray<FMIDIDeviceInfo> UMIDIDeviceManager::MIDIOutputDevicesInfo;

void UMIDIDeviceManager::StartupMIDIDeviceManager()
{
	if( ensure( !bIsInitialized ) )
	{
		PmError PMError = Pm_Initialize();
		if( PMError == pmNoError )
		{
			bIsInitialized = true;
		}
		else
		{
			UE_LOG( LogMIDIDevice, Error, TEXT( "Unable to open initialize the MIDI device manager (PortMidi error: %s).  You won't be use MIDI features in this session." ), ANSI_TO_TCHAR( Pm_GetErrorText( PMError ) ) );
		}
	}
}


void UMIDIDeviceManager::ShutdownMIDIDeviceManager()
{
	if(bIsInitialized)
	{
		bIsInitialized = false;

		// Kill any open connections
		for(TObjectIterator<UMIDIDeviceController> ControllerIter; ControllerIter; ++ControllerIter)
		{
			UMIDIDeviceController* MIDIDeviceController = *ControllerIter;
			if(MIDIDeviceController != nullptr && !MIDIDeviceController->IsPendingKill())
			{
				MIDIDeviceController->ShutdownDevice();
			}
		}

		Pm_Terminate();
	}
}


void UMIDIDeviceManager::ProcessMIDIEvents()
{
	if(bIsInitialized)
	{
		// @todo midi perf: Should we cache weak pointers instead of using TObjectIterator every frame?
		for(TObjectIterator<UMIDIDeviceController> ControllerIter; ControllerIter; ++ControllerIter)
		{
			UMIDIDeviceController* MIDIDeviceController = *ControllerIter;
			if(MIDIDeviceController != nullptr && !MIDIDeviceController->IsPendingKill())
			{
				MIDIDeviceController->ProcessIncomingMIDIEvents();
			}
		}

		for (TObjectIterator<UMIDIDeviceInputController> ControllerIter; ControllerIter; ++ControllerIter)
		{
			UMIDIDeviceInputController* MIDIDeviceController = *ControllerIter;
			if (MIDIDeviceController != nullptr && !MIDIDeviceController->IsPendingKill())
			{
				MIDIDeviceController->ProcessIncomingMIDIEvents();
			}
		}
	}
}


void UMIDIDeviceManager::FindMIDIDevices(TArray<FFoundMIDIDevice>& OutMIDIDevices)
{
	OutMIDIDevices.Reset();

	if(bIsInitialized)
	{
		// Figure out what the system default input and output devices are, so we can relay that information
		const PmDeviceID DefaultInputPMDeviceID = Pm_GetDefaultInputDeviceID();
		const PmDeviceID DefaultOutputPMDeviceID = Pm_GetDefaultOutputDeviceID();

		const int PMDeviceCount = Pm_CountDevices();
		for(PmDeviceID PMDeviceID = 0; PMDeviceID < PMDeviceCount; ++PMDeviceID)
		{
			const PmDeviceInfo* PMDeviceInfo = Pm_GetDeviceInfo(PMDeviceID);
			if(PMDeviceInfo != nullptr)
			{
				FFoundMIDIDevice& FoundMIDIDevice = *new(OutMIDIDevices) FFoundMIDIDevice();

				FoundMIDIDevice.DeviceID = PMDeviceID;
				FoundMIDIDevice.DeviceName = ANSI_TO_TCHAR(PMDeviceInfo->name);
				FoundMIDIDevice.bCanReceiveFrom = PMDeviceInfo->input != 0;
				FoundMIDIDevice.bCanSendTo = PMDeviceInfo->output != 0;
				FoundMIDIDevice.bIsAlreadyInUse = PMDeviceInfo->opened != 0;
				FoundMIDIDevice.bIsDefaultInputDevice = (PMDeviceID == DefaultInputPMDeviceID);
				FoundMIDIDevice.bIsDefaultOutputDevice = (PMDeviceID == DefaultOutputPMDeviceID);
			}
			else
			{
				UE_LOG(LogMIDIDevice, Error, TEXT("Unable to query information about MIDI device (PortMidi device ID: %i).  This device won't be available for input or output."), PMDeviceID);
			}
		}
	}
	else
	{
		UE_LOG(LogMIDIDevice, Warning, TEXT("Find MIDI Devices cannot be used because the MIDI device manager failed to initialize.  Check earlier in the log to see why."));
	}
}


void UMIDIDeviceManager::FindAllMIDIDeviceInfo(TArray<FMIDIDeviceInfo>& OutMIDIInputDevicesInfo, TArray<FMIDIDeviceInfo>& OutMIDIOutputDevicesInfo)
{
	OutMIDIInputDevicesInfo.Reset();
	OutMIDIOutputDevicesInfo.Reset();

	if (bIsInitialized)
	{
		// Figure out what the system default input and output devices are, so we can relay that information
		const PmDeviceID DefaultInputPMDeviceID = Pm_GetDefaultInputDeviceID();
		const PmDeviceID DefaultOutputPMDeviceID = Pm_GetDefaultOutputDeviceID();

		const int PMDeviceCount = Pm_CountDevices();
		for (PmDeviceID PMDeviceID = 0; PMDeviceID < PMDeviceCount; ++PMDeviceID)
		{
			const PmDeviceInfo* PMDeviceInfo = Pm_GetDeviceInfo(PMDeviceID);
			if (PMDeviceInfo != nullptr)
			{
				if (PMDeviceInfo->input != 0)
				{
					FMIDIDeviceInfo& FoundMIDIInputDeviceInfo = *new(OutMIDIInputDevicesInfo) FMIDIDeviceInfo();
					FoundMIDIInputDeviceInfo.DeviceID = PMDeviceID;
					FoundMIDIInputDeviceInfo.DeviceName = ANSI_TO_TCHAR(PMDeviceInfo->name);
					FoundMIDIInputDeviceInfo.bIsAlreadyInUse = PMDeviceInfo->opened != 0;
					FoundMIDIInputDeviceInfo.bIsDefaultDevice = (PMDeviceID == DefaultInputPMDeviceID);
				}
				else if (PMDeviceInfo->output != 0)
				{
					FMIDIDeviceInfo& FoundMIDIOutputDeviceInfo = *new(OutMIDIOutputDevicesInfo) FMIDIDeviceInfo();
					FoundMIDIOutputDeviceInfo.DeviceID = PMDeviceID;
					FoundMIDIOutputDeviceInfo.DeviceName = ANSI_TO_TCHAR(PMDeviceInfo->name);
					FoundMIDIOutputDeviceInfo.bIsAlreadyInUse = PMDeviceInfo->opened != 0;
					FoundMIDIOutputDeviceInfo.bIsDefaultDevice = (PMDeviceID == DefaultInputPMDeviceID);
				}

				MIDIInputDevicesInfo = OutMIDIInputDevicesInfo;
				MIDIOutputDevicesInfo = OutMIDIOutputDevicesInfo;
			}
			else
			{
				UE_LOG(LogMIDIDevice, Error, TEXT("Unable to query information about MIDI device (PortMidi device ID: %i).  This device won't be available for input or output."), PMDeviceID);
			}
		}
	}
	else
	{
		UE_LOG(LogMIDIDevice, Warning, TEXT("Find MIDI Devices cannot be used because the MIDI device manager failed to initialize.  Check earlier in the log to see why."));
	}
}

void UMIDIDeviceManager::GetMIDIInputDeviceIDByName(const FString DeviceName, int32& DeviceID)
{
	if (bIsInitialized)
	{
		for (int32 i = 0; i < MIDIInputDevicesInfo.Num(); ++i)
		{
			const FMIDIDeviceInfo& MIDIInputDevice = MIDIInputDevicesInfo[i];

			if (MIDIInputDevice.DeviceName == DeviceName)
			{
				DeviceID = MIDIInputDevice.DeviceID;
			}
		}
	}
	else
	{
		UE_LOG(LogMIDIDevice, Warning, TEXT("MIDI device manager not imnitialized"));
	}
}

void UMIDIDeviceManager::GetDefaultIMIDIInputDeviceID(int32& DeviceID)
{
	if (bIsInitialized)
	{
		DeviceID = Pm_GetDefaultInputDeviceID();
	}
	else
	{
		UE_LOG(LogMIDIDevice, Warning, TEXT("MIDI device manager not imnitialized"));
	}
}

void UMIDIDeviceManager::GetMIDIOutputDeviceIDByName(const FString DeviceName, int32& DeviceID)
{
	if (bIsInitialized)
	{
		for (int32 i = 0; i < MIDIOutputDevicesInfo.Num(); ++i)
		{
			const FMIDIDeviceInfo& MIDIOutputDevice = MIDIOutputDevicesInfo[i];

			if (MIDIOutputDevice.DeviceName == DeviceName)
			{
				DeviceID = MIDIOutputDevice.DeviceID;
			}
		}
	}
	else
	{
		UE_LOG(LogMIDIDevice, Warning, TEXT("MIDI device manager not imnitialized"));
	}
}

void UMIDIDeviceManager::GetDefaultIMIDIOutputDeviceID(int32& DeviceID)
{
	if (bIsInitialized)
	{
		DeviceID = Pm_GetDefaultOutputDeviceID();
	}
	else
	{
		UE_LOG(LogMIDIDevice, Warning, TEXT("MIDI device manager not imnitialized"));
	}
}

UMIDIDeviceController* UMIDIDeviceManager::CreateMIDIDeviceController(const int32 DeviceID, const int32 MIDIBufferSize)
{
	UMIDIDeviceController* NewMIDIDeviceController = nullptr;
	
	if(bIsInitialized)
	{
		// Create the MIDI Device Controller object.  It will be transient.
		NewMIDIDeviceController = NewObject<UMIDIDeviceController>();

		bool bStartedSuccessfully = false;
		NewMIDIDeviceController->StartupDevice(DeviceID, MIDIBufferSize, /* Out */ bStartedSuccessfully);

		if(!bStartedSuccessfully)
		{
			// Kill it
			NewMIDIDeviceController->MarkPendingKill();
			NewMIDIDeviceController = nullptr;

			UE_LOG(LogMIDIDevice, Error, TEXT( "Create MIDI Device Controller wasn't able to create the controller successfully.  Returning a null reference."));
		}
	}
	else
	{
		UE_LOG(LogMIDIDevice, Error, TEXT( "Create MIDI Device Controller isn't able to create a controller because the MIDI Device Manager failed to initialize.  Look earlier in the log to see why it failed to startup.  Returning a null reference."));
	}

	return NewMIDIDeviceController;
}

UMIDIDeviceInputController* UMIDIDeviceManager::CreateMIDIDeviceInputController(const int32 DeviceID, const int32 MIDIBufferSize)
{
	UMIDIDeviceInputController* NewMIDIDeviceController = nullptr;

	// Create the MIDI Device Controller object.  It will be transient.
	NewMIDIDeviceController = NewObject<UMIDIDeviceInputController>();

	bool bStartedSuccessfully = false;
	NewMIDIDeviceController->StartupDevice(DeviceID, MIDIBufferSize, /* Out */ bStartedSuccessfully);

	if (!bStartedSuccessfully)
	{
		// Kill it
		NewMIDIDeviceController->MarkPendingKill();
		NewMIDIDeviceController = nullptr;

		UE_LOG(LogMIDIDevice, Error, TEXT("Create MIDI Device Controller wasn't able to create the controller successfully.  Returning a null reference."));
	}

	return NewMIDIDeviceController;
}


UMIDIDeviceOutputController* UMIDIDeviceManager::CreateMIDIDeviceOutputController(const int32 DeviceID)
{
	UMIDIDeviceOutputController* NewMIDIDeviceController = nullptr;

	// Create the MIDI Device Controller object.  It will be transient.
	NewMIDIDeviceController = NewObject<UMIDIDeviceOutputController>();

	bool bStartedSuccessfully = false;
	NewMIDIDeviceController->StartupDevice(DeviceID, /* Out */ bStartedSuccessfully);

	if (!bStartedSuccessfully)
	{
		// Kill it
		NewMIDIDeviceController->MarkPendingKill();
		NewMIDIDeviceController = nullptr;

		UE_LOG(LogMIDIDevice, Error, TEXT("Create MIDI Device Controller wasn't able to create the controller successfully.  Returning a null reference."));
	}

	return NewMIDIDeviceController;
}

#undef LOCTEXT_NAMESPACE
