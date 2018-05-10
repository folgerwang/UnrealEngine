// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaFinder.h"

#include "Aja.h"
#include "AJALib.h"

/*
 * FAjaMediaSourceId interface
 */

const TCHAR* FAjaMediaPort::Protocol = TEXT("aja");

namespace AjaMediaSourceId
{
	const TCHAR* ProtocolPath = TEXT("aja://");
	const TCHAR* Device = TEXT("device");
	const TCHAR* Port = TEXT("port");

	const int32 ProtocolLength = FPlatformString::Strlen(AjaMediaSourceId::ProtocolPath);
	const int32 DeviceLength = FPlatformString::Strlen(AjaMediaSourceId::Device);
	const int32 InputLength = FPlatformString::Strlen(AjaMediaSourceId::Port);

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

FAjaMediaPort::FAjaMediaPort()
	: DeviceIndex(INDEX_NONE)
	, PortIndex(INDEX_NONE)
{ }

FAjaMediaPort::FAjaMediaPort(const FString& InDeviceName, int32 InDeviceIndex, int32 InPortIndex)
	: DeviceName(InDeviceName)
	, DeviceIndex(InDeviceIndex)
	, PortIndex(InPortIndex)
{ }

FString FAjaMediaPort::ToString() const
{
	return FString::Printf(TEXT("%s [%s]"), *DeviceName, *ToUrl());
}

FString FAjaMediaPort::ToUrl() const
{
	return FString::Printf(TEXT("aja://device%d/port%d"), DeviceIndex, (PortIndex));
}

bool FAjaMediaPort::IsValid() const
{
	return DeviceIndex != INDEX_NONE && PortIndex != INDEX_NONE;
}

bool FAjaMediaPort::FromUrl(const FString& Url, bool bDiscoverDeviceName)
{
	const int32 NumberLength = 1;
	const int32 SlashLength = 1;
	const int32 ValidUrlLength = AjaMediaSourceId::ProtocolLength + AjaMediaSourceId::DeviceLength + NumberLength + SlashLength + AjaMediaSourceId::InputLength + NumberLength;

	DeviceName.Reset();
	DeviceIndex = INDEX_NONE;
	PortIndex = INDEX_NONE;

	if (Url.Len() != ValidUrlLength || !Url.StartsWith(AjaMediaSourceId::ProtocolPath))
	{
		return false;
	}

	int32 TempDeviceIndex;
	int32 TempPortIndex;
	if(!AjaMediaSourceId::GetNumber(Url[AjaMediaSourceId::ProtocolLength + AjaMediaSourceId::DeviceLength], TempDeviceIndex)
		||	!AjaMediaSourceId::GetNumber(Url[AjaMediaSourceId::ProtocolLength + AjaMediaSourceId::DeviceLength + NumberLength + SlashLength + AjaMediaSourceId::InputLength], TempPortIndex))
	{
		return false;
	}

	DeviceIndex = TempDeviceIndex;
	PortIndex = TempPortIndex;

	bool bResult = true;
	if (bDiscoverDeviceName)
	{
		DeviceName.Reset();

		bResult = FAja::IsInitialized();
		if (bResult)
		{
			AJA::FDeviceScanner DeviceScanner = AJA::CreateDeviceScanner();
			if (DeviceScanner)
			{
				AJA::DeviceScannerScanHardware(DeviceScanner);
				bResult = DeviceScanner != nullptr;
				if (bResult)
				{
					uint32 NumDevices = AJA::DeviceScannerGetNumDevices(DeviceScanner);
					bResult = (uint32)DeviceIndex < NumDevices;
					if (bResult)
					{
						AJA::FDeviceInfo DeviceInfo = AJA::DeviceScannerGetDeviceInfo(DeviceScanner, PortIndex);
						TCHAR DeviceNameBuffer[AjaMediaSourceId::DeviceNameBufferSize];
						AJA::DeviceInfoGetDeviceId(DeviceInfo, DeviceNameBuffer, AjaMediaSourceId::DeviceNameBufferSize);
						AJA::ReleaseDeviceInfo(DeviceInfo);

						DeviceName = DeviceNameBuffer;
					}

					AJA::ReleaseDeviceScanner(DeviceScanner);
				}
			}
		}
	}

	return bResult;
}

/*
 * FAjaMediaMode interface
 */

FAjaMediaMode::FAjaMediaMode()
	: Mode(INDEX_NONE)
{
}

FAjaMediaMode::FAjaMediaMode(const FString& InModeName, int32 InMode)
	: ModeName(InModeName)
	, Mode(InMode)
{
}

FString FAjaMediaMode::ToString() const
{
	return FString::Printf(TEXT("%s [%d]"), *ModeName, Mode);
}

FString FAjaMediaMode::ToUrl() const
{
	return ToString();
}

bool FAjaMediaMode::IsValid() const
{
	return Mode != INDEX_NONE;
}

/*
 * UAjaMediaFinder interface
 */

bool UAjaMediaFinder::GetSources(TArray<FAjaMediaPort>& OutSources)
{
	OutSources.Reset();
	if (!FAja::IsInitialized())
	{
		return false;
	}

	AJA::FDeviceScanner DeviceScanner = AJA::CreateDeviceScanner();
	if (DeviceScanner)
	{
		AJA::DeviceScannerScanHardware(DeviceScanner);

		uint32 NumDevices = AJA::DeviceScannerGetNumDevices(DeviceScanner);
		for (uint32 SourceIndex = 0; SourceIndex < NumDevices; ++SourceIndex)
		{
			AJA::FDeviceInfo DeviceInfo = AJA::DeviceScannerGetDeviceInfo(DeviceScanner, SourceIndex);
			TCHAR DeviceName[AjaMediaSourceId::DeviceNameBufferSize];
			AJA::DeviceInfoGetDeviceId(DeviceInfo, DeviceName, AjaMediaSourceId::DeviceNameBufferSize);

			uint32 InputCount = AJA::DeviceInfoGetVidInputs(DeviceInfo);
			for (uint32 Inputs = 0; Inputs < InputCount; ++Inputs)
			{
				OutSources.Add(FAjaMediaPort(DeviceName, SourceIndex, Inputs+1));
			}
			AJA::ReleaseDeviceInfo(DeviceInfo);
		}

		AJA::ReleaseDeviceScanner(DeviceScanner);
	}

	return true;
}


bool UAjaMediaFinder::GetModes(TArray<FAjaMediaMode>& OutModes, bool bInOutput)
{
	OutModes.Reset();
	if (!FAja::IsInitialized())
	{
		return false;
	}

	uint32_t NumModes = AJA::ModeCount();
	TCHAR ModeName[AjaMediaSourceId::ModeNameBufferSize];
	for (uint32_t Mode = 0; Mode < NumModes; ++Mode)
	{
		if (AJA::ModeNames(Mode, bInOutput ? AJA::EDirectionFilter::DF_OUTPUT : AJA::EDirectionFilter::DF_INPUT, ModeName, AjaMediaSourceId::ModeNameBufferSize))
		{
			OutModes.Add(FAjaMediaMode(ModeName, Mode));
		}
	}

	return true;
}
