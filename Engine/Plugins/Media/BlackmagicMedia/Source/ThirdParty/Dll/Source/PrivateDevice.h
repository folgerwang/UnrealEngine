// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

struct IDeckLink;

namespace BlackmagicDevice
{
	class PrivatePort;
	/*
	 * Video IO device
	 */
	class PrivateDevice : public RefCount
	{
		static const FUInt MaxPortCount = 8;
	public:
		PrivateDevice(int InDeviceId, RefPointer<IDeckLink>& InDeckLink);
		~PrivateDevice();

		HRESULT QueryInterface(RefPointer<IDeckLinkInput>& OutDeckLinkInput);
		HRESULT QueryInterface(RefPointer<IDeckLinkOutput>& OutDeckLinkInput);
		HRESULT QueryInterface(RefPointer<IDeckLinkKeyer>& OutDeckLinkKeyer);

		PrivatePort* AquirePort(FUInt InPort, bool InDirection);
		void ReleasePort(PrivatePort* InPort);

		FUInt GetDeviceIndex();

	protected:
		Thread::FLock Lock;

		int DeviceId;
		RefPointer<IDeckLink> DeckLink;

		typedef std::vector<PrivatePort*> FPortList;
		FPortList InputPorts;
		FPortList OutputPorts;
	};

	class PrivateDeviceCache
	{
	public:
		static PrivateDeviceCache& GetCache();

		PrivateDevice* AquireDevice(FUInt InDeviceId);
		void ReleaseDevice(PrivateDevice* InDevice);

	protected:
		Thread::FLock Lock;
		static const int MaxDeviceCount = 8;
		std::array<PrivateDevice*, MaxDeviceCount> DeviceList;
	};

};
