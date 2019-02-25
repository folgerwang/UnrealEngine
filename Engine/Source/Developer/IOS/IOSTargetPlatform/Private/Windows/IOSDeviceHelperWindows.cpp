// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved

#include "IOSTargetPlatform.h"
#include "IOSTargetDeviceOutput.h"
#include "HAL/PlatformProcess.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Interfaces/ITargetPlatform.h"

struct FDeviceNotificationCallbackInformation
{
	FString UDID;
	FString	DeviceName;
	FString ProductType;
	uint32 msgType;
};

class FIOSDevice
{
public:
    FIOSDevice(FString InID, FString InName)
		: UDID(InID)
		, Name(InName)
    {
    }
    
	~FIOSDevice()
	{
	}

	FString SerialNumber() const
	{
		return UDID;
	}

private:
    FString UDID;
	FString Name;
};

/**
 * Delegate type for devices being connected or disconnected from the machine
 *
 * The first parameter is newly added or removed device
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FDeviceNotification, void*)

class FDeviceQueryTask
	: public FRunnable
{
public:
	FDeviceQueryTask()
		: Stopping(false)
		, bCheckDevices(true)
		, NeedSDKCheck(true)
		, RetryQuery(5)
	{}

	virtual bool Init() override
	{
		return true;
	}

	virtual uint32 Run() override
	{
		while (!Stopping)
		{
			if (bCheckDevices)
			{
#if WITH_EDITOR
				if (!IsRunningCommandlet())
				{
					if (NeedSDKCheck)
					{
						if (GetTargetPlatformManager())
						{
							bool CanQuery = false;
							FString OutTutorialPath;
							const ITargetPlatform* Platform = GetTargetPlatformManager()->FindTargetPlatform(TEXT("IOS"));
							if (Platform)
							{
								if (Platform->IsSdkInstalled(false, OutTutorialPath))
								{
									CanQuery = true;
								}
							}
							NeedSDKCheck = false;
							if (!CanQuery)
							{
								Enable(false);
							}
						}
					}
					else
					{
						// BHP - Turning off device check to prevent it from interfering with packaging
						QueryDevices();
					}
				}
#endif
			}

			FPlatformProcess::Sleep(5.0f);
		}

		return 0;
	}

	virtual void Stop() override
	{
		Stopping = true;
	}

	virtual void Exit() override
	{}

	FDeviceNotification& OnDeviceNotification()
	{
		return DeviceNotification;
	}

	void Enable(bool OnOff)
	{
		bCheckDevices = OnOff;
	}

private:

	void QueryDevices()
	{
		FString StdOut;
		// get the list of devices
		int Response = FIOSTargetDeviceOutput::ExecuteDSCommand("listdevices", &StdOut);
		if (Response <= 0)
		{
			RetryQuery--;
			if (RetryQuery < 0 || Response < 0)
			{
				UE_LOG(LogTemp, Log, TEXT("IOS device listing is disabled (to many failed attempts)!"));
				Enable(false);
			}
			return;
		}
		RetryQuery = 5;
		// separate out each line
		TArray<FString> DeviceStrings;
		StdOut = StdOut.Replace(TEXT("\r"), TEXT("\n"));
		StdOut.ParseIntoArray(DeviceStrings, TEXT("\n"), true);

		TArray<FString> CurrentDeviceIds;
		for (int32 StringIndex = 0; StringIndex < DeviceStrings.Num(); ++StringIndex)
		{
			const FString& DeviceString = DeviceStrings[StringIndex];

			if(!DeviceString.StartsWith("[DD] FOUND: "))
			{
				continue;
			}

			// grab the device serial number
			int32 typeIndex = DeviceString.Find(TEXT("TYPE: "));
			int32 idIndex = DeviceString.Find(TEXT("ID: "));
			int32 nameIndex = DeviceString.Find(TEXT("NAME: "));
			if (typeIndex < 0 || idIndex < 0 || nameIndex < 0)
			{
				continue;
			}

			FString SerialNumber = DeviceString.Mid(idIndex + 4, nameIndex - 1 - (idIndex + 4));
			CurrentDeviceIds.Add(SerialNumber);

			// move on to next device if this one is already a known device
			if (ConnectedDeviceIds.Find(SerialNumber) != INDEX_NONE)
			{
				ConnectedDeviceIds.Remove(SerialNumber);
				continue;
			}

			// parse product type and device name
			FString ProductType = DeviceString.Mid(typeIndex + 6, idIndex - 1 - (typeIndex + 6));
			FString DeviceName = DeviceString.Mid(nameIndex + 6, DeviceString.Len() - (nameIndex + 6));

			// create an FIOSDevice
			FDeviceNotificationCallbackInformation CallbackInfo;
			CallbackInfo.DeviceName = DeviceName;
			CallbackInfo.UDID = SerialNumber;
			CallbackInfo.ProductType = ProductType;
			CallbackInfo.msgType = 1;
			DeviceNotification.Broadcast(&CallbackInfo);
		}

		// remove all devices no longer found
		for (int32 DeviceIndex = 0; DeviceIndex < ConnectedDeviceIds.Num(); ++DeviceIndex)
		{
			FDeviceNotificationCallbackInformation CallbackInfo;
			CallbackInfo.UDID = ConnectedDeviceIds[DeviceIndex];
			CallbackInfo.msgType = 2;
			DeviceNotification.Broadcast(&CallbackInfo);
		}
		ConnectedDeviceIds = CurrentDeviceIds;
	}

	bool Stopping;
	bool bCheckDevices;
	bool NeedSDKCheck;
	int RetryQuery;
	TArray<FString> ConnectedDeviceIds;
	FDeviceNotification DeviceNotification;
};

/* FIOSDeviceHelper structors
 *****************************************************************************/
static TMap<FIOSDevice*, FIOSLaunchDaemonPong> ConnectedDevices;
static FDeviceQueryTask* QueryTask = NULL;
static FRunnableThread* QueryThread = NULL;
static TArray<FDeviceNotificationCallbackInformation> NotificationMessages;
static FTickerDelegate TickDelegate;

bool FIOSDeviceHelper::MessageTickDelegate(float DeltaTime)
{
    QUICK_SCOPE_CYCLE_COUNTER(STAT_FIOSDeviceHelper_MessageTickDelegate);

	for (int Index = 0; Index < NotificationMessages.Num(); ++Index)
	{
		FDeviceNotificationCallbackInformation cbi = NotificationMessages[Index];
		FIOSDeviceHelper::DeviceCallback(&cbi);
	}
	NotificationMessages.Empty();

	return true;
}

void FIOSDeviceHelper::Initialize(bool bIsTVOS)
{
	// Create a dummy device to hand over
	const FString DummyDeviceName = FString::Printf(TEXT("All_%s_On_%s"), bIsTVOS ? TEXT("tvOS") : TEXT("iOS"), FPlatformProcess::ComputerName());
	
	FIOSLaunchDaemonPong Event;
	Event.DeviceID = FString::Printf(TEXT("%s@%s"), bIsTVOS ? TEXT("TVOS") : TEXT("IOS"), *DummyDeviceName);
	Event.bCanReboot = false;
	Event.bCanPowerOn = false;
	Event.bCanPowerOff = false;
	Event.DeviceName = DummyDeviceName;
	Event.DeviceType = bIsTVOS ? TEXT("AppleTV") : TEXT("");
	FIOSDeviceHelper::OnDeviceConnected().Broadcast(Event);

	if(!bIsTVOS)
	{
		// add the message pump
		TickDelegate = FTickerDelegate::CreateStatic(MessageTickDelegate);
		FTicker::GetCoreTicker().AddTicker(TickDelegate, 5.0f);

		// kick off a thread to query for connected devices
		QueryTask = new FDeviceQueryTask();
		QueryTask->OnDeviceNotification().AddStatic(FIOSDeviceHelper::DeviceCallback);

		static int32 QueryTaskCount = 1;
		if (QueryTaskCount == 1)
		{
			// create the socket subsystem (loadmodule in game thread)
			ISocketSubsystem* SSS = ISocketSubsystem::Get();
			QueryThread = FRunnableThread::Create(QueryTask, *FString::Printf(TEXT("FIOSDeviceHelper.QueryTask_%d"), QueryTaskCount++), 128 * 1024, TPri_Normal);
		}
	}
}

void FIOSDeviceHelper::DeviceCallback(void* CallbackInfo)
{
	struct FDeviceNotificationCallbackInformation* cbi = (FDeviceNotificationCallbackInformation*)CallbackInfo;

	if (!IsInGameThread())
	{
		NotificationMessages.Add(*cbi);
	}
	else
	{
		switch(cbi->msgType)
		{
		case 1:
			FIOSDeviceHelper::DoDeviceConnect(CallbackInfo);
			break;

		case 2:
			FIOSDeviceHelper::DoDeviceDisconnect(CallbackInfo);
			break;
		}
	}
}

void FIOSDeviceHelper::DoDeviceConnect(void* CallbackInfo)
{
	// connect to the device
	struct FDeviceNotificationCallbackInformation* cbi = (FDeviceNotificationCallbackInformation*)CallbackInfo;
	FIOSDevice* Device = new FIOSDevice(cbi->UDID, cbi->DeviceName);

	// fire the event
	FIOSLaunchDaemonPong Event;
	Event.DeviceID = FString::Printf(TEXT("%s@%s"), cbi->ProductType.Contains(TEXT("AppleTV")) ? TEXT("TVOS") : TEXT("IOS"), *(cbi->UDID));
	Event.DeviceName = cbi->DeviceName;
	Event.DeviceType = cbi->ProductType;
	Event.bCanReboot = false;
	Event.bCanPowerOn = false;
	Event.bCanPowerOff = false;
	FIOSDeviceHelper::OnDeviceConnected().Broadcast(Event);

	// add to the device list
	ConnectedDevices.Add(Device, Event);
}

void FIOSDeviceHelper::DoDeviceDisconnect(void* CallbackInfo)
{
	struct FDeviceNotificationCallbackInformation* cbi = (FDeviceNotificationCallbackInformation*)CallbackInfo;
	FIOSDevice* device = NULL;
	for (auto DeviceIterator = ConnectedDevices.CreateIterator(); DeviceIterator; ++DeviceIterator)
	{
		if (DeviceIterator.Key()->SerialNumber() == cbi->UDID)
		{
			device = DeviceIterator.Key();
			break;
		}
	}
	if (device != NULL)
	{
		// extract the device id from the connected list
		FIOSLaunchDaemonPong Event = ConnectedDevices.FindAndRemoveChecked(device);

		// fire the event
		FIOSDeviceHelper::OnDeviceDisconnected().Broadcast(Event);

		// delete the device
		delete device;
	}
}

bool FIOSDeviceHelper::InstallIPAOnDevice(const FTargetDeviceId& DeviceId, const FString& IPAPath)
{
    return false;
}

void FIOSDeviceHelper::EnableDeviceCheck(bool OnOff)
{
	QueryTask->Enable(OnOff);
}