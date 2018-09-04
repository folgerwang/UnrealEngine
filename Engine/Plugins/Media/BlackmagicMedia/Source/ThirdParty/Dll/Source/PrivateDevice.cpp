// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "stdafx.h"
#include "VideoIOPrivate.h"

namespace BlackmagicDevice
{

	/*
	 * Video IO device
	 */
	PrivateDeviceCache& PrivateDeviceCache::GetCache()
	{
		// Static state with guaranteed initialization  
		static PrivateDeviceCache DeviceCache;
		return DeviceCache;
	}

	PrivateDevice* PrivateDeviceCache::AquireDevice(FUInt InDeviceId)
	{
		Thread::FAutoLock AutoLock(Lock);
		if (InDeviceId >= MaxDeviceCount)
		{
			LOG_ERROR(TEXT("Aquire device, bad device index %d (%d)\n"), InDeviceId, MaxDeviceCount);
			return nullptr;
		}
		
		if (DeviceList[InDeviceId])
		{
			PrivateDevice* Device = DeviceList[InDeviceId];
			Device->AddRef();
			return Device;
		}

		RefPointer<IDeckLinkIterator> DeckLinkIterator;
		ComCheck(CoCreateInstance(CLSID_CDeckLinkIterator, NULL, CLSCTX_ALL, IID_IDeckLinkIterator, DeckLinkIterator));

		// Should iterate for the device index
		RefPointer<IDeckLink> DeckLink;
		for (FUInt i = 0; DeckLinkIterator->Next(DeckLink) == S_OK && i < InDeviceId; ++i)
		{
			DeckLink.Reset();
		}

		if (!DeckLink)
		{
			LOG_ERROR(TEXT("Failed to create device %d\n"), InDeviceId);
			return nullptr;
		}

		PrivateDevice* Device = new PrivateDevice(InDeviceId, DeckLink);
		DeviceList[InDeviceId] = Device;
		return Device;
	}

	void PrivateDeviceCache::ReleaseDevice(PrivateDevice *InDevice)
	{
		Thread::FAutoLock AutoLock(Lock);
		FUInt DeviceId = InDevice->GetDeviceIndex();
		if (DeviceList[DeviceId] == InDevice)
		{
			DeviceList[DeviceId] = nullptr;
		}
		else
		{
			LOG_ERROR(TEXT("Failed to relase device %d\n"), DeviceId);
		}
	}

	PrivateDevice::PrivateDevice(int InDeviceId, RefPointer<IDeckLink>& InDeckLink)
		: DeviceId(InDeviceId)
		, DeckLink(InDeckLink)
	{
		AddRef();
		InputPorts.resize(MaxPortCount);
		OutputPorts.resize(MaxPortCount);
	}

	PrivateDevice::~PrivateDevice()
	{
		PrivateDeviceCache::GetCache().ReleaseDevice(this);
	}

	HRESULT PrivateDevice::QueryInterface(RefPointer<IDeckLinkInput>& OutDeckLinkInput)
	{
		return DeckLink->QueryInterface(IID_IDeckLinkInput, OutDeckLinkInput);
	}

	HRESULT PrivateDevice::QueryInterface(RefPointer<IDeckLinkOutput>& OutDeckLinkOutput)
	{
		return DeckLink->QueryInterface(IID_IDeckLinkOutput, OutDeckLinkOutput);
	}

	HRESULT PrivateDevice::QueryInterface(RefPointer<IDeckLinkKeyer>& OutDeckLinkKeyer)
	{
		return DeckLink->QueryInterface(IID_IDeckLinkKeyer, OutDeckLinkKeyer);
	}

	PrivatePort* PrivateDevice::AquirePort(FUInt InPortId, bool InInput)
	{
		Thread::FAutoLock AutoLock(Lock);
	
		if (InPortId >= MaxPortCount)
		{
			LOG_ERROR(TEXT("Aquire port, bad port %d (%d)\n"), InPortId, MaxPortCount);
			return 0;
		}

		FPortList& Ports = InInput ? InputPorts : OutputPorts;

		if (Ports[InPortId])
		{
			if (!InInput)
			{
				LOG_ERROR(TEXT("can't share output port %d\n"), InPortId);
				return 0;
			}
			PrivatePort* Port = Ports[InPortId];
			Port->AddRef();
			return Port;
		}

		PrivatePort* Port = new PrivatePort(this, InPortId, InInput);
		Ports[InPortId] = Port;
		return Port;
	}

	void PrivateDevice::ReleasePort(PrivatePort* InPort)
	{
		Thread::FAutoLock AutoLock(Lock);
		assert(InPort);
		FUInt PortId = InPort->GetPortIndex();
		FPortList& Ports = InPort->IsInput() ? InputPorts : OutputPorts;

		if (Ports[PortId] == InPort)
		{
			Ports[PortId] = nullptr;
		}
		else
		{
			LOG_ERROR(TEXT("Failed to release port %d\n"), PortId);
		}
	}

	FUInt PrivateDevice::GetDeviceIndex()
	{
		return DeviceId;
	}

};