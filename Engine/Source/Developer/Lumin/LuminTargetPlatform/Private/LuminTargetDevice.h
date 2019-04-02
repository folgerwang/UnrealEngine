// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AndroidTargetDevice.h"

/**
* Implements a Lumin target device.
*/
class FLuminTargetDevice : public FAndroidTargetDevice
{
public:

	/**
	* Creates and initializes a new Lumin target device.
	*
	* @param InTargetPlatform - The target platform.
	* @param InSerialNumber - The ADB serial number of the target device.
	* @param InAndroidVariant - The variant of the Android platform, i.e. ATC, DXT or PVRTC.
	*/
	FLuminTargetDevice(const ITargetPlatform& InTargetPlatform, const FString& InSerialNumber, const FString& InAndroidVariant)
		: FAndroidTargetDevice(InTargetPlatform, InSerialNumber, InAndroidVariant)
	{ }

	// Return true if the devices can be grouped in an aggregate (All_<platform>_devices_on_<host>) proxy
	virtual bool IsPlatformAggregated() const override
	{
		return false;
	}

	bool GetMldbFullFilename(FString& OutFilename)
	{
		FString MLSDKPath = FPlatformMisc::GetEnvironmentVariable(TEXT("MLSDK"));
#if PLATFORM_WINDOWS
		OutFilename = FString::Printf(TEXT("%s\\tools\\mldb\\mldb.exe"), *MLSDKPath);
#else
		OutFilename = FString::Printf(TEXT("%s/tools/mldb/mldb"), *MLSDKPath);
#endif
		if (OutFilename[0] != 0)
		{
			return true;
		}
		return false;
	}

	bool ExecuteMldbCommand(const FString Command, FString* OutStdOut, FString* OutStdErr)
	{
		FString MldbPath;
		if (!GetMldbFullFilename(MldbPath))
		{
			return false;
		}

		int32 ReturnCode;
		FString DefaultError;
		if (!OutStdErr)
		{
			OutStdErr = &DefaultError;
		}
		FString MldbCommand = FString::Printf(TEXT("-s %s %s"), *GetSerialNumber(), *Command);

		FPlatformProcess::ExecProcess(*MldbPath, *MldbCommand, &ReturnCode, OutStdOut, OutStdErr);
		if (ReturnCode != 0)
		{
			FPlatformMisc::LowLevelOutputDebugStringf(TEXT("Command %s failed with error code %d"), *MldbCommand, ReturnCode, **OutStdErr);
			return false;
		}
		return true;
	}

	virtual bool SupportsFeature(ETargetDeviceFeatures Feature) const
	{
		switch (Feature)
		{
		case ETargetDeviceFeatures::MultiLaunch:
			return false;
		case ETargetDeviceFeatures::PowerOff:
			return true;
		case ETargetDeviceFeatures::PowerOn:
			return false;
		case ETargetDeviceFeatures::Reboot:
			return true;
		default:
			return false;
		};
	}

	bool AdviseLockStatus()
	{
		FString RetVal;
		if (ExecuteMldbCommand(FString::Printf(TEXT("access-status")), &RetVal, nullptr))
		{
			if (RetVal.Contains(TEXT("Device locked  : True")))
			{
				if (FPlatformMisc::MessageBoxExt(EAppMsgType::OkCancel,
					TEXT("Lumin device is locked. This command will take 60 seconds, during which the editor will be unresponsive."),
					TEXT("Device Locked")) == EAppReturnType::Cancel)
				{
					return false;
				}
			}
		}
		else
		{
			// If the first mldb command doesn't work ...
			return false;
		}
		return true;
	}

	virtual bool PowerOff(bool Force) override
	{
		if (AdviseLockStatus())
		{
			if (!ExecuteMldbCommand(FString::Printf(TEXT("shutdown")), nullptr, nullptr))
			{
				return false;
			}
			return true;
		}
		return false;
	}

	virtual bool Reboot(bool bReconnect) override
	{
		if (AdviseLockStatus())
		{
			if (!ExecuteMldbCommand(FString::Printf(TEXT("reboot")), nullptr, nullptr))
			{
				return false;
			}
			return true;
		}
		return false;
	}

	virtual FString GetOperatingSystemName() override
	{
		if (!AndroidVersionString.IsEmpty())
		{
			return FString::Printf(TEXT("Lumin OS %s"), *AndroidVersionString);
		}
		else
		{
			return TEXT("Lumin OS");
		}
	}

};
