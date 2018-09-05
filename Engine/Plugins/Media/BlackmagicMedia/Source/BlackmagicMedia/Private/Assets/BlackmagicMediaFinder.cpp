// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BlackmagicMediaFinder.h"

#include "Blackmagic.h"
#include "BlackmagicLib.h"

/*
 * FBlackmagicMediaSourceId interface
 */

const TCHAR* FBlackmagicMediaPort::Protocol = TEXT("blackmagic");

namespace BlackmagicMediaSourceId
{
	const TCHAR* ProtocolPath = TEXT("blackmagic://");
	const TCHAR* Device = TEXT("device");
	const TCHAR* Port = TEXT("port");

	const int32 ProtocolLength = FPlatformString::Strlen(BlackmagicMediaSourceId::ProtocolPath);
	const int32 DeviceLength = FPlatformString::Strlen(BlackmagicMediaSourceId::Device);
	const int32 InputLength = FPlatformString::Strlen(BlackmagicMediaSourceId::Port);

	const int32 DeviceNameBufferSize = 64;
	const int32 ModeNameBufferSize = 64;

	bool GetNumber(TCHAR InInput, int32& OutId)
	{
		OutId = 0;
		if (InInput < '0' && InInput > '9')
		{
			return false;
		}
		OutId = InInput - '0';
		return true;
	}
}

FBlackmagicMediaPort::FBlackmagicMediaPort()
	: DeviceIndex(INDEX_NONE)
	, PortIndex(INDEX_NONE)
{ }

FBlackmagicMediaPort::FBlackmagicMediaPort(const FString& InDeviceName, int32 InDeviceIndex, int32 InPortIndex)
	: DeviceName(InDeviceName)
	, DeviceIndex(InDeviceIndex)
	, PortIndex(InPortIndex)
{ }

FString FBlackmagicMediaPort::ToString() const
{
	return FString::Printf(TEXT("%s [%s]"), *DeviceName, *ToUrl());
}

FString FBlackmagicMediaPort::ToUrl() const
{
	return FString::Printf(TEXT("blackmagic://device%d/port%d"), DeviceIndex, (PortIndex));
}

bool FBlackmagicMediaPort::IsValid() const
{
	return DeviceIndex != INDEX_NONE && PortIndex != INDEX_NONE;
}

bool FBlackmagicMediaPort::FromUrl(const FString& Url, bool bDiscoverDeviceName)
{
	const int32 NumberLength = 1;
	const int32 SlashLength = 1;
	const int32 ValidUrlLength = BlackmagicMediaSourceId::ProtocolLength + BlackmagicMediaSourceId::DeviceLength + NumberLength + SlashLength + BlackmagicMediaSourceId::InputLength + NumberLength;

	DeviceName.Reset();
	DeviceIndex = INDEX_NONE;
	PortIndex = INDEX_NONE;

	if (Url.Len() != ValidUrlLength || !Url.StartsWith(BlackmagicMediaSourceId::ProtocolPath))
	{
		return false;
	}

	int32 TempDeviceIndex;
	int32 TempPortIndex;
	if(!BlackmagicMediaSourceId::GetNumber(Url[BlackmagicMediaSourceId::ProtocolLength + BlackmagicMediaSourceId::DeviceLength], TempDeviceIndex)
		||	!BlackmagicMediaSourceId::GetNumber(Url[BlackmagicMediaSourceId::ProtocolLength + BlackmagicMediaSourceId::DeviceLength + NumberLength + SlashLength + BlackmagicMediaSourceId::InputLength], TempPortIndex))
	{
		return false;
	}

	DeviceIndex = TempDeviceIndex;
	PortIndex = TempPortIndex;

	bool bResult = true;
	if (bDiscoverDeviceName)
	{
		DeviceName.Reset();

		bResult = FBlackmagic::IsInitialized();
		if (bResult)
		{
			BlackmagicDevice::FDeviceScanner DeviceScanner = BlackmagicDevice::VideoIOCreateDeviceScanner();
			if (DeviceScanner)
			{
				BlackmagicDevice::VideoIODeviceScannerScanHardware(DeviceScanner);
				bResult = DeviceScanner != nullptr;
				if (bResult)
				{
					uint32 NumDevices = BlackmagicDevice::VideoIODeviceScannerGetNumDevices(DeviceScanner);
					bResult = (uint32)DeviceIndex < NumDevices;
					if (bResult)
					{
						BlackmagicDevice::FDeviceInfo DeviceInfo = BlackmagicDevice::VideoIODeviceScannerGetDeviceInfo(DeviceScanner, PortIndex);
						TCHAR DeviceNameBuffer[BlackmagicMediaSourceId::DeviceNameBufferSize];
						BlackmagicDevice::VideoIODeviceInfoGetDeviceId(DeviceInfo, DeviceNameBuffer, BlackmagicMediaSourceId::DeviceNameBufferSize);
						BlackmagicDevice::VideoIOReleaseDeviceInfo(DeviceInfo);
						DeviceName = DeviceNameBuffer;
					}

					BlackmagicDevice::VideoIOReleaseDeviceScanner(DeviceScanner);
				}
			}
		}
	}

	return bResult;
}

/*
 * UBlackmagicMediaMode interface
 */

FBlackmagicMediaMode::FBlackmagicMediaMode()
	: Mode(INDEX_NONE)
{
}

FBlackmagicMediaMode::FBlackmagicMediaMode(const FString& InModeName, int32 InMode)
	: ModeName(InModeName)
	, Mode(InMode)
{
}

FString FBlackmagicMediaMode::ToString() const
{
	return FString::Printf(TEXT("%s [%d]"), *ModeName, Mode);
}

FString FBlackmagicMediaMode::ToUrl() const
{
	return ToString();
}

bool FBlackmagicMediaMode::IsValid() const
{
	return Mode != INDEX_NONE; // && PortIndex != INDEX_NONE;
}

/*
 * UBlackmagicMediaFinder interface
 */

bool UBlackmagicMediaFinder::GetSources(TArray<FBlackmagicMediaPort>& OutSources)
{
	OutSources.Reset();
	if (!FBlackmagic::IsInitialized())
	{
		return false;
	}

	BlackmagicDevice::FDeviceScanner DeviceScanner = BlackmagicDevice::VideoIOCreateDeviceScanner();
	if (DeviceScanner)
	{
		BlackmagicDevice::VideoIODeviceScannerScanHardware(DeviceScanner);

		uint32 NumDevices = BlackmagicDevice::VideoIODeviceScannerGetNumDevices(DeviceScanner);
		for (uint32 SourceIndex = 0; SourceIndex < NumDevices; ++SourceIndex)
		{
			BlackmagicDevice::FDeviceInfo DeviceInfo = BlackmagicDevice::VideoIODeviceScannerGetDeviceInfo(DeviceScanner, SourceIndex);
			TCHAR DeviceName[BlackmagicMediaSourceId::DeviceNameBufferSize];
			BlackmagicDevice::VideoIODeviceInfoGetDeviceId(DeviceInfo, DeviceName, BlackmagicMediaSourceId::DeviceNameBufferSize);

			uint32 InputCount = BlackmagicDevice::VideoIODeviceInfoGetVidInputs(DeviceInfo);
			for (uint32 Inputs = 0; Inputs < InputCount; ++Inputs)
			{
				OutSources.Add(FBlackmagicMediaPort(DeviceName, SourceIndex, Inputs+1));
			}
			BlackmagicDevice::VideoIOReleaseDeviceInfo(DeviceInfo);
		}
		BlackmagicDevice::VideoIOReleaseDeviceScanner(DeviceScanner);
	}

	return true;
}

bool UBlackmagicMediaFinder::GetModes(TArray<FBlackmagicMediaMode>& OutModes, bool bInOutput)
{
	OutModes.Reset();
	if (!FBlackmagic::IsInitialized())
	{
		return false;
	}

	uint32 NumModes = BlackmagicDevice::VideoIOModeCount();
	TCHAR ModeName[BlackmagicMediaSourceId::ModeNameBufferSize];
	for (uint32 Mode = 0; Mode < NumModes; ++Mode)
	{
		if (BlackmagicDevice::VideoIOModeNames(Mode, 
			bInOutput? BlackmagicDevice::EModeFilter::MF_OUTPUT : BlackmagicDevice::EModeFilter::MF_INPUT,
			ModeName,
			BlackmagicMediaSourceId::ModeNameBufferSize))
		{
			OutModes.Add(FBlackmagicMediaMode(ModeName, Mode));
		}
	}

	return true;
}
