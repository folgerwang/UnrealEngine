// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaFinder.h"
#include "AjaMediaPrivate.h"

#include "Aja.h"
#include "AJALib.h"

#include "Templates/UniquePtr.h"

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
	if (IsValid())
	{
		return FString::Printf(TEXT("%s [%s]"), *DeviceName, *ToUrl());
	}
	return TEXT("<Invalid>");
}

FString FAjaMediaPort::ToUrl() const
{
	if (IsValid())
	{
		return FString::Printf(TEXT("aja://device%d/port%d"), DeviceIndex, (PortIndex));
	}
	return TEXT("aja://");
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
		bResult = FAja::IsInitialized();
		if (bResult)
		{
			TUniquePtr<AJA::AJADeviceScanner> DeviceScanner = MakeUnique<AJA::AJADeviceScanner>();
			if (DeviceScanner)
			{
				int32 NumDevices = DeviceScanner->GetNumDevices();
				bResult = DeviceIndex < NumDevices;
				if (bResult)
				{
					TCHAR DeviceNameBuffer[AJA::AJADeviceScanner::FormatedTextSize];
					bResult = DeviceScanner->GetDeviceTextId(DeviceIndex, DeviceNameBuffer);
					if (bResult)
					{
						DeviceName = DeviceNameBuffer;
					}
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
	: DeviceIndex(INDEX_NONE)
	, VideoFormatIndex(INDEX_NONE)
{
}

FString FAjaMediaMode::ToString() const
{
	if (IsValid())
	{
		return FString::Printf(TEXT("%s"), *ModeName);
	}
	return TEXT("<Invalid>");
}

bool FAjaMediaMode::IsValid() const
{
	return VideoFormatIndex != INDEX_NONE;
}

/*
 * UAjaMediaFinder interface
 */

bool FAjaMediaFinder::GetSources(TArray<FAjaMediaPort>& OutSources)
{
	OutSources.Reset();
	if (!FAja::IsInitialized() || !FAja::CanUseAJACard())
	{
		return false;
	}

	TUniquePtr<AJA::AJADeviceScanner> DeviceScanner = MakeUnique<AJA::AJADeviceScanner>();
	if (DeviceScanner)
	{
		int32 NumDevices = DeviceScanner->GetNumDevices();
		for (int32 SourceIndex = 0; SourceIndex < NumDevices; ++SourceIndex)
		{
			TCHAR DeviceNameBuffer[AJA::AJADeviceScanner::FormatedTextSize];
			if (DeviceScanner->GetDeviceTextId(SourceIndex, DeviceNameBuffer))
			{
				int32 OutputCount;
				int32 InputCount;
				if (DeviceScanner->GetNumberVideoChannels(SourceIndex, InputCount, OutputCount))
				{
					for (int32 Inputs = 0; Inputs < InputCount; ++Inputs)
					{
						OutSources.Add(FAjaMediaPort(DeviceNameBuffer, SourceIndex, Inputs + 1));
					}
				}
			}
		}
	}

	return true;
}


bool FAjaMediaFinder::GetModes(int32 DeviceIndex, bool bInOutput, TArray<FAjaMediaMode>& OutModes)
{
	OutModes.Reset();
	if (!FAja::IsInitialized() || !FAja::CanUseAJACard())
	{
		return false;
	}

	AJA::AJAVideoFormats FrameFormats(DeviceIndex, bInOutput);

	const int32 NumSupportedFormat = FrameFormats.GetNumSupportedFormat();
	OutModes.Reserve(NumSupportedFormat);
	for(int32 Index = 0; Index < NumSupportedFormat; ++Index)
	{
		AJA::AJAVideoFormats::VideoFormatDescriptor Descriptor = FrameFormats.GetSupportedFormat(Index);
		OutModes.Emplace(AJAHelpers::FromVideoFormatDescriptor(DeviceIndex, Descriptor));
	}

	return true;
}

bool FAjaMediaFinder::IsValid(const FAjaMediaPort& InPort, const FAjaMediaMode& InMode, FString& OutFailureReason)
{
	if (!InPort.IsValid())
	{
		OutFailureReason = TEXT("The MediaPort is invalid.");
		return false;
	}

	if (!InMode.IsValid())
	{
		OutFailureReason = TEXT("The MediaMode is invalid.");
		return false;
	}

	if (InPort.DeviceIndex != InMode.DeviceIndex)
	{
		OutFailureReason = TEXT("The MediaPort & MediaMode are not on the same device.");
		return false;
	}

	return true;
}
