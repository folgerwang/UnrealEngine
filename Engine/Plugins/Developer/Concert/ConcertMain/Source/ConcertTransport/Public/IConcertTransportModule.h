// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "IConcertTransportLoggerPtr.h"

class IConcertLocalEndpoint;
struct FConcertEndpointSettings;

/**
* Interface for an Endpoint Provider
*/
class IConcertEndpointProvider
{
public:
	virtual ~IConcertEndpointProvider() {}

	/** */
	virtual TSharedPtr<IConcertLocalEndpoint> CreateLocalEndpoint(const FString& InEndpointFriendlyName, const FConcertEndpointSettings& InEndpointSettings, const FConcertTransportLoggerFactory& InLogFactory) const = 0;
};

/**
 * Interface for the Concert Transport module.
 */
class IConcertTransportModule : public IModuleInterface
{
public:
	/** Get the Concert module */
	static IConcertTransportModule& Get()
	{
		static const FName ModuleName = TEXT("ConcertTransport");
		return FModuleManager::Get().GetModuleChecked<IConcertTransportModule>(ModuleName);
	}

	/** Create a local transport endpoint */
	virtual TSharedPtr<IConcertEndpointProvider> CreateEndpointProvider() const = 0;
};
