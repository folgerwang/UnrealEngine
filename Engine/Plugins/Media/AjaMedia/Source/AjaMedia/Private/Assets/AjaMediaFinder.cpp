// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AjaMediaFinder.h"
#include "AjaMediaPrivate.h"

#include "Aja.h"
#include "AJALib.h"

#include "Misc/FeedbackContext.h"
#include "UObject/PropertyPortFlags.h"

#define LOCTEXT_NAMESPACE "AjaMediaFinder"


/************************************************************************/
// FAjaMediaSourceId interface
/************************************************************************/

namespace AjaMediaSourceId
{
	const TCHAR* ProtocolPath = TEXT("aja://");
	const TCHAR* Device = TEXT("device");
	const TCHAR* Single = TEXT("single");
	const TCHAR* Dual = TEXT("dual");
	const TCHAR* QuadSquare = TEXT("quadSq");
	const TCHAR* QuadTsi = TEXT("quadTSI");

	const TCHAR* GetLinkText(EAjaLinkType InLinkType, EAjaQuadLinkType InQuadLinkType)
	{
		const TCHAR* Result = AjaMediaSourceId::Single;
		switch (InLinkType)
		{
		case EAjaLinkType::DualLink: Result = AjaMediaSourceId::Dual; break;
		case EAjaLinkType::QuadLink: Result = (InQuadLinkType == EAjaQuadLinkType::Square ? AjaMediaSourceId::QuadSquare : AjaMediaSourceId::QuadTsi); break;
		}
		return Result;
	}

	FAjaMediaMode ToMediaMode(const AJA::AJAVideoFormats::VideoFormatDescriptor& InDescriptor)
	{
		FAjaMediaMode MediaMode;
		MediaMode.TargetSize = FIntPoint(InDescriptor.ResolutionWidth, InDescriptor.ResolutionHeight);
		MediaMode.bIsProgressiveStandard = InDescriptor.bIsProgressiveStandard;
		MediaMode.bIsInterlacedStandard = InDescriptor.bIsInterlacedStandard;
		MediaMode.bIsPsfStandard = InDescriptor.bIsPsfStandard;
		MediaMode.FrameRate = FFrameRate(InDescriptor.FrameRateNumerator, InDescriptor.FrameRateDenominator);
		MediaMode.VideoFormatIndex = InDescriptor.VideoFormatIndex;

		if (InDescriptor.bIsInterlacedStandard)
		{
			MediaMode.FrameRate.Numerator *= 2;
		}

		return MediaMode;
	}

	bool IsVideoFormatValid(const AJA::AJAVideoFormats::VideoFormatDescriptor& InDescriptor)
	{
		if (!InDescriptor.bIsValid)
		{
			return false;
		}
		if (InDescriptor.bIsPsfStandard || InDescriptor.bIsVideoFormatB)
		{
			return false;
		}
		if (InDescriptor.bIsSD || InDescriptor.bIs2K || InDescriptor.bIs4K)
		{
			return false;
		}
		return true;
	}
}

/************************************************************************/
// FAjaMediaDevice implementation
/************************************************************************/

FAjaMediaDevice::FAjaMediaDevice()
	: DeviceIndex(INDEX_NONE)
{ }

/************************************************************************/
// FAjaMediaPort implementation
/************************************************************************/

FAjaMediaPort::FAjaMediaPort()
	: DeviceIndex(INDEX_NONE)
	, PortIndex(INDEX_NONE)
	, LinkType(EAjaLinkType::SingleLink)
	, QuadLinkType(EAjaQuadLinkType::Square)
{ }

FAjaMediaPort::FAjaMediaPort(FName InDeviceName, int32 InDeviceIndex, int32 InPortIndex)
	: DeviceName(InDeviceName)
	, DeviceIndex(InDeviceIndex)
	, PortIndex(InPortIndex)
	, LinkType(EAjaLinkType::SingleLink)
	, QuadLinkType(EAjaQuadLinkType::Square)
{ }

FString FAjaMediaPort::ToString() const
{
	if (IsValid())
	{
		return FString::Printf(TEXT("%s [%s%d/%s%d]")
			, *DeviceName.ToString()
			, AjaMediaSourceId::Device
			, DeviceIndex
			, AjaMediaSourceId::GetLinkText(LinkType, QuadLinkType)
			, PortIndex);
	}
	return TEXT("<Invalid>");
}

FString FAjaMediaPort::ToUrl() const
{
	if (IsValid())
	{
		return FString::Printf(TEXT("%s%s%d/%s%d")
			, AjaMediaSourceId::ProtocolPath
			, AjaMediaSourceId::Device
			, DeviceIndex
			, AjaMediaSourceId::GetLinkText(LinkType, QuadLinkType)
			, PortIndex);
	}
	return TEXT("aja://");
}

bool FAjaMediaPort::IsValid() const
{
	return DeviceIndex != INDEX_NONE && PortIndex != INDEX_NONE;
}

/************************************************************************/
// FAjaMediaMode implementation
/************************************************************************/

FAjaMediaMode::FAjaMediaMode()
	: VideoFormatIndex(INDEX_NONE)
{
}

bool FAjaMediaMode::IsValid() const
{
	return VideoFormatIndex != INDEX_NONE;
}

FString FAjaMediaMode::ToString() const
{
	const TCHAR* Standard = TEXT("");
	if (TargetSize.Y == 1080 || TargetSize.Y == 720)
	{
		if (bIsInterlacedStandard)
		{
			Standard = TEXT("i");
		}
		else if (bIsProgressiveStandard)
		{
			Standard = TEXT("p");
		}
		else if (bIsPsfStandard)
		{
			Standard = TEXT("psf");
		}
	}
	return FString::Printf(TEXT("%s%s%s"), *FAjaMediaFinder::ResolutionToPrettyText(TargetSize).ToString(), Standard, *FrameRate.ToPrettyText().ToString());
}

/************************************************************************/
// FAjaMediaConfiguration implementation
/************************************************************************/

FAjaMediaConfiguration::FAjaMediaConfiguration()
	: bInput(true)
{
}

bool FAjaMediaConfiguration::IsValid() const
{
	return MediaPort.IsValid() && MediaMode.IsValid();
}

FText FAjaMediaConfiguration::ToText() const
{
	if (IsValid())
	{
		FNumberFormattingOptions Options;
		Options.UseGrouping = false;
		return FText::Format(LOCTEXT("MediaConfigurationInputText", "{0}[{1}]/{2}/{3}{4}/{5}")
			, FText::FromName(MediaPort.DeviceName)
			, FText::AsNumber(MediaPort.DeviceIndex)
			, FAjaMediaFinder::LinkTypeToPrettyText(MediaPort.LinkType, MediaPort.PortIndex, true)
			, FText::AsNumber(MediaMode.TargetSize.Y, &Options)
			, (MediaMode.bIsInterlacedStandard ? FText::FromString(TEXT("i")) : (MediaMode.bIsPsfStandard ? FText::FromString(TEXT("psf")) : FText::FromString(TEXT("p"))))
			, MediaMode.FrameRate.ToPrettyText());
	}

	return LOCTEXT("Invalid", "<Invalid>");
}

/************************************************************************/
// FAjaMediaFinder interface
/************************************************************************/

FText FAjaMediaFinder::LinkTypeToPrettyText(EAjaLinkType InLinkType, int32 InChannel, bool bShortVersion)
{
	if (bShortVersion)
	{
		switch (InLinkType)
		{
		case EAjaLinkType::SingleLink: return FText::Format(LOCTEXT("ChannelShortLabel", "Single[{0}]"), FText::AsNumber(InChannel));
		case EAjaLinkType::DualLink: return FText::Format(LOCTEXT("DualLinkShortLabel", "Dual[{0}-{1}]"), FText::AsNumber(InChannel), FText::AsNumber(InChannel + 1));
		case EAjaLinkType::QuadLink: return FText::Format(LOCTEXT("QuadLinkShortLabel", "Quad[{0}-{1}]"), FText::AsNumber(InChannel), FText::AsNumber(InChannel + 3));
		}
	}
	else
	{
		switch (InLinkType)
		{
		case EAjaLinkType::SingleLink: return FText::Format(LOCTEXT("ChannelLabel", "Single Link {0}"), FText::AsNumber(InChannel));
		case EAjaLinkType::DualLink: return FText::Format(LOCTEXT("DualLinkLabel", "Dual Link {0}-{1}"), FText::AsNumber(InChannel), FText::AsNumber(InChannel + 1));
		case EAjaLinkType::QuadLink: return FText::Format(LOCTEXT("QuadLinkLabel", "Quad Link {0}-{1}"), FText::AsNumber(InChannel), FText::AsNumber(InChannel + 3));
		}
	}
	return FText::GetEmpty();
}


FText FAjaMediaFinder::QuadLinkTypeToPrettyText(EAjaQuadLinkType InLinkType)
{
	switch (InLinkType)
	{
	case EAjaQuadLinkType::Square: return LOCTEXT("QuadLinkSquareLabel", "Square");
	case EAjaQuadLinkType::TSI: return LOCTEXT("QuadLink_TsiLabel", "TSI");
	}
	return FText::GetEmpty();
}


FText FAjaMediaFinder::ResolutionToPrettyText(FIntPoint InResolution)
{
	if (InResolution.X == 720 && InResolution.Y == 486) { return LOCTEXT("NTSC_Resolution", "NTSC SD"); }
	if (InResolution.X == 720 && InResolution.Y == 576) { return LOCTEXT("PAL_Resolution", "PAL SD"); }
	if (InResolution.X == 1280 && InResolution.Y == 720) { return LOCTEXT("HD720_Resolution", "HD 720"); }
	if (InResolution.X == 1920 && InResolution.Y == 1080) { return LOCTEXT("HD1080_Resolution", "HD 1080"); }
	if (InResolution.X == 2048 && InResolution.Y == 1080) { return LOCTEXT("2K_Resolution", "2K"); }
	if (InResolution.X == 2048 && InResolution.Y == 1556) { return LOCTEXT("2KFull_Resolution", "2K Full"); }
	if (InResolution.X == 3840 && InResolution.Y == 2160) { return LOCTEXT("UHD_Resolution", "UHD"); }
	if (InResolution.X == 4096 && InResolution.Y == 2160) { return LOCTEXT("4K_Resolution", "4K"); }
	return FText::Format(LOCTEXT("Undefined_Resolution", "{0}x{1}"), FText::AsNumber(InResolution.X), FText::AsNumber(InResolution.Y));
}


namespace AjaMediaFinder
{
	bool GetConfigurations(TArray<FAjaMediaConfiguration>& OutConfigurations, bool bAllowInput, bool bAllowOutput)
	{
		const int32 MaxNumberOfChannel = 8;

		OutConfigurations.Reset();
		if (!FAja::IsInitialized())
		{
			return false;
		}

		AJA::AJADeviceScanner DeviceScanner;
		int32 NumDevices = DeviceScanner.GetNumDevices();
		for (int32 DeviceIndex = 0; DeviceIndex < NumDevices; ++DeviceIndex)
		{
			TCHAR DeviceNameBuffer[AJA::AJADeviceScanner::FormatedTextSize];
			if (DeviceScanner.GetDeviceTextId(DeviceIndex, DeviceNameBuffer))
			{
				AJA::AJADeviceScanner::DeviceInfo DeviceInfo;
				if (DeviceScanner.GetDeviceInfo(DeviceIndex, DeviceInfo))
				{
					int32 OutputCount;
					int32 InputCount;
					if (DeviceInfo.bIsSupported && DeviceScanner.GetNumberVideoChannels(DeviceIndex, InputCount, OutputCount))
					{
						if (bAllowInput && !DeviceInfo.bCanDoCapture)
						{
							continue;
						}

						if (bAllowOutput && !DeviceInfo.bCanDoPlayback)
						{
							continue;
						}

						InputCount = FMath::Min(InputCount, MaxNumberOfChannel);
						OutputCount = FMath::Min(OutputCount, MaxNumberOfChannel);
						const int32 PortCount = (bAllowInput && bAllowOutput) ? FMath::Min(InputCount, OutputCount) : (bAllowOutput ? OutputCount : InputCount);

						AJA::AJAVideoFormats FrameFormats(DeviceIndex, false);
						const int32 NumSupportedFormat = FrameFormats.GetNumSupportedFormat();

						FAjaMediaConfiguration MediaConfiguration;
						MediaConfiguration.MediaPort.DeviceIndex = DeviceIndex;
						MediaConfiguration.MediaPort.DeviceName = FName(DeviceNameBuffer);
						MediaConfiguration.bInput = true;

						for (int32 FormatIndex = 0; FormatIndex < NumSupportedFormat; ++FormatIndex)
						{
							AJA::AJAVideoFormats::VideoFormatDescriptor Descriptor = FrameFormats.GetSupportedFormat(FormatIndex);
							if (!AjaMediaSourceId::IsVideoFormatValid(Descriptor))
							{
								continue;
							}

							MediaConfiguration.MediaMode = AjaMediaSourceId::ToMediaMode(Descriptor);
							MediaConfiguration.MediaPort.QuadLinkType = EAjaQuadLinkType::Square;

							if (Descriptor.bIs2K)
							{
								if (DeviceInfo.bCanDoDualLink)
								{
									MediaConfiguration.MediaPort.LinkType = EAjaLinkType::DualLink;
									for (int32 SourceIndex = 0; SourceIndex < PortCount; SourceIndex += 2)
									{
										MediaConfiguration.MediaPort.PortIndex = SourceIndex + 1;
										OutConfigurations.Add(MediaConfiguration);
									}
								}
							}
							else if (Descriptor.bIs4K)
							{
								if (DeviceInfo.bCanDoDualLink)
								{
									MediaConfiguration.MediaPort.LinkType = EAjaLinkType::QuadLink;
									for (int32 SourceIndex = 0; SourceIndex < PortCount; SourceIndex += 4)
									{
										MediaConfiguration.MediaPort.QuadLinkType = EAjaQuadLinkType::Square;
										MediaConfiguration.MediaPort.PortIndex = SourceIndex + 1;
										OutConfigurations.Add(MediaConfiguration);

										MediaConfiguration.MediaPort.QuadLinkType = EAjaQuadLinkType::TSI;
										OutConfigurations.Add(MediaConfiguration);
									}
								}
							}
							else
							{
								MediaConfiguration.MediaPort.LinkType = EAjaLinkType::SingleLink;
								for (int32 SourceIndex = 0; SourceIndex < PortCount; ++SourceIndex)
								{
									MediaConfiguration.MediaPort.PortIndex = SourceIndex + 1;
									OutConfigurations.Add(MediaConfiguration);
								}
							}
						}
					}
				}
			}
		}

		return OutConfigurations.Num() > 0;
	}
}


bool FAjaMediaFinder::GetInputConfigurations(TArray<FAjaMediaConfiguration>& OutConfigurations)
{
	return AjaMediaFinder::GetConfigurations(OutConfigurations, true, false);
}


bool FAjaMediaFinder::GetOutputConfigurations(TArray<FAjaMediaConfiguration>& OutConfigurations)
{
	return AjaMediaFinder::GetConfigurations(OutConfigurations, false, true);
}


bool FAjaMediaFinder::GetDevices(TArray<FAjaMediaDevice>& OutDevices)
{
	OutDevices.Reset();
	if (!FAja::IsInitialized())
	{
		return false;
	}

	AJA::AJADeviceScanner DeviceScanner;
	int32 NumDevices = DeviceScanner.GetNumDevices();
	for (int32 DeviceIndex = 0; DeviceIndex < NumDevices; ++DeviceIndex)
	{
		AJA::AJADeviceScanner::DeviceInfo DeviceInfo;
		if (!DeviceScanner.GetDeviceInfo(DeviceIndex, DeviceInfo))
		{
			continue;
		}

		if (!DeviceInfo.bIsSupported)
		{
			continue;
		}

		TCHAR DeviceNameBuffer[AJA::AJADeviceScanner::FormatedTextSize];
		if (!DeviceScanner.GetDeviceTextId(DeviceIndex, DeviceNameBuffer))
		{
			continue;
		}

		FAjaMediaDevice Device;
		Device.DeviceIndex = DeviceIndex;
		Device.DeviceName = DeviceNameBuffer;
		OutDevices.Add(MoveTemp(Device));
	}

	return true;
}

bool FAjaMediaFinder::GetSources(TArray<FAjaMediaPort>& OutSources)
{
	OutSources.Reset();
	if (!FAja::IsInitialized())
	{
		return false;
	}

	AJA::AJADeviceScanner DeviceScanner;
	const int32 NumDevices = DeviceScanner.GetNumDevices();
	for (int32 DeviceIndex = 0; DeviceIndex < NumDevices; ++DeviceIndex)
	{
		AJA::AJADeviceScanner::DeviceInfo DeviceInfo;
		if (!DeviceScanner.GetDeviceInfo(DeviceIndex, DeviceInfo))
		{
			continue;
		}

		if (!DeviceInfo.bIsSupported)
		{
			continue;
		}

		TCHAR DeviceNameBuffer[AJA::AJADeviceScanner::FormatedTextSize];
		if (DeviceScanner.GetDeviceTextId(DeviceIndex, DeviceNameBuffer))
		{
			int32 OutputCount;
			int32 InputCount;
			if (DeviceScanner.GetNumberVideoChannels(DeviceIndex, InputCount, OutputCount))
			{
				for (int32 Inputs = 0; Inputs < InputCount; ++Inputs)
				{
					OutSources.Add(FAjaMediaPort(DeviceNameBuffer, DeviceIndex, Inputs + 1));
				}
			}
		}
	}

	return true;
}


bool FAjaMediaFinder::GetModes(int32 DeviceIndex, bool bInOutput, TArray<FAjaMediaMode>& OutModes)
{
	OutModes.Reset();
	if (!FAja::IsInitialized())
	{
		return false;
	}

	AJA::AJADeviceScanner DeviceScanner;
	AJA::AJADeviceScanner::DeviceInfo DeviceInfo;
	if (!DeviceScanner.GetDeviceInfo(DeviceIndex, DeviceInfo))
	{
		return false;
	}

	if (!DeviceInfo.bIsSupported)
	{
		return false;
	}

	if (bInOutput && !DeviceInfo.bCanDoPlayback || !bInOutput && !DeviceInfo.bCanDoCapture)
	{
		return false;
	}

	AJA::AJAVideoFormats FrameFormats(DeviceIndex, bInOutput);
	const int32 NumSupportedFormat = FrameFormats.GetNumSupportedFormat();
	OutModes.Reserve(NumSupportedFormat);
	for(int32 Index = 0; Index < NumSupportedFormat; ++Index)
	{
		AJA::AJAVideoFormats::VideoFormatDescriptor Descriptor = FrameFormats.GetSupportedFormat(Index);
		if (!AjaMediaSourceId::IsVideoFormatValid(Descriptor))
		{
			continue;
		}
		OutModes.Add(AjaMediaSourceId::ToMediaMode(Descriptor));
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

	return true;
}

#undef LOCTEXT_NAMESPACE
