// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BSDSockets/SocketSubsystemBSD.h"
#include "SocketSubsystemPackage.h"

class Error;
class FInternetAddr;

/**
 * Unix specific socket subsystem implementation
 */
class FSocketSubsystemUnix : public FSocketSubsystemBSD
{
protected:

	/** Single instantiation of this subsystem */
	static FSocketSubsystemUnix* SocketSingleton;

	/** Whether Init() has been called before or not */
	bool bTriedToInit;

PACKAGE_SCOPE:

	/** 
	 * Singleton interface for this subsystem 
	 * @return the only instance of this subsystem
	 */
	static FSocketSubsystemUnix* Create();

	/**
	 * Performs Unix specific socket clean up
	 */
	static void Destroy();

public:

	FSocketSubsystemUnix() 
		: bTriedToInit(false)
	{
	}

	virtual ~FSocketSubsystemUnix()
	{
	}

	// ISocketSubsystem
	virtual bool Init(FString& Error) override;
	virtual void Shutdown() override;
	virtual bool HasNetworkDevice() override;
	virtual class FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, ESocketProtocolFamily ProtocolType, bool bForceUDP = false) override;
	virtual TSharedRef<FInternetAddr> GetLocalHostAddr(FOutputDevice& Out, bool& bCanBindAll) override;
	virtual bool GetLocalAdapterAddresses(TArray<TSharedPtr<FInternetAddr> >& OutAddresses) override;
};
