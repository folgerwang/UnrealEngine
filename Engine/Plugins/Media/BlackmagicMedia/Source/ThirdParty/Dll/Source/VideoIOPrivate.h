// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PrivateDevice.h"
#include "PrivateFrame.h"
#include "PrivatePort.h"
#include "SharedPort.h"


struct IDeckLinkInput;

namespace BlackmagicDevice
{

	enum ESupportedDirection {
		EVIDEOIO_SD_INPUT = 1,
		EVIDEOIO_SD_OUTPUT = 2,
	};

	struct FSupportedDescription {
		FFrameDesc FrameDesc;
		BMDDisplayMode DisplayMode;
		BMDPixelFormat PixelFormat;
		ESupportedDirection SupportedDirection;
	};

	FSupportedDescription* GetSupportedDescription(BMDDisplayMode InDisplayMode);
	FSupportedDescription* GetSupportedDescription(const FFrameDesc& InFrameDesc);

	/*
	 * Video device scanner
	 */
	class PrivateDeviceScanner : public RefCount
	{
	public:
		PrivateDeviceScanner()
		{
			AddRef();

			RefPointer<IDeckLinkIterator> DeckLinkIterator;
			ComCheck(CoCreateInstance(CLSID_CDeckLinkIterator, NULL, CLSCTX_ALL, IID_IDeckLinkIterator, DeckLinkIterator));

			if (DeckLinkIterator)
			{
				RefPointer<IDeckLink> DeckLink;
				while (DeckLinkIterator->Next(DeckLink) == S_OK)
				{
					Devices.push_back(DeckLink);
					DeckLink.Reset();
				}
			}
		}
		~PrivateDeviceScanner()
		{
		}
		FUInt GetDeviceCount(void)
		{
			return static_cast<FUInt>(Devices.size());
		}
		IDeckLink* GetDevice(FUInt InDeviceId)
		{
			if (InDeviceId < Devices.size())
			{
				return Devices[InDeviceId];
			}
			return nullptr;
		}
	protected:
		std::vector<RefPointer<IDeckLink>> Devices;
	};

	/*
	 * Device scanner info
	 */
	class PrivateDeviceScannerInfo : public RefCount
	{
	public:
		PrivateDeviceScannerInfo(FUInt InDeviceId, RefPointer<IDeckLink>& InDeckLink)
			: DeviceId(InDeviceId)
			, DeckLink(InDeckLink)
		{
			AddRef();
		}

		~PrivateDeviceScannerInfo()
		{
		}
		IDeckLink* GetDevice()
		{
			return DeckLink;
		}
	protected:
		FUInt DeviceId;
		RefPointer<IDeckLink> DeckLink;
	};

};