// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IBackChannelConnection.h"
#include "Modules/ModuleManager.h"

/**
 *	Main module and factory interface for Backchannel connections
 */
class BACKCHANNEL_API IBackChannelTransport : public IModuleInterface
{
public:

	static inline bool IsAvailable(void)
	{
		return Get() != nullptr;
	}

	static inline IBackChannelTransport* Get(void)
	{
		return FModuleManager::LoadModulePtr<IBackChannelTransport>("BackChannel");
	}

	virtual TSharedPtr<IBackChannelConnection> CreateConnection(const int32 Type) = 0;

public:

	static const int TCP;

protected:

	IBackChannelTransport() {}
	virtual ~IBackChannelTransport() {}
};
