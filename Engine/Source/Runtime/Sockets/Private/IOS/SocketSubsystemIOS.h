// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SocketSubsystem.h"
#include "BSDSockets/SocketSubsystemBSD.h"
#include "SocketSubsystemPackage.h"

/**
 * iOS specific socket subsystem implementation
 */
class FSocketSubsystemIOS : public FSocketSubsystemBSD
{
protected:
	virtual TSharedRef<FInternetAddr> CreateInternetAddr(uint32 Address = 0, uint32 Port = 0) override;
	/** Single instantiation of this subsystem */
	static FSocketSubsystemIOS* SocketSingleton;

	/** Whether Init() has been called before or not */
	bool bTriedToInit;

	// @todo ios: This is kind of hacky, since there's no UBT that should set PACKAGE_SCOPE
// PACKAGE_SCOPE:
public:

	/** 
	 * Singleton interface for this subsystem 
	 * @return the only instance of this subsystem
	 */
	static FSocketSubsystemIOS* Create();

	/**
	 * Performs iOS specific socket clean up
	 */
	static void Destroy();


	virtual ESocketProtocolFamily GetDefaultSocketProtocolFamily() const override
	{
		return ESocketProtocolFamily::IPv6;
	}

public:

	FSocketSubsystemIOS() 
		: bTriedToInit(false)
	{
	}

	virtual ~FSocketSubsystemIOS()
	{
	}

	virtual ESocketErrors CreateAddressFromIP(const ANSICHAR* IPAddress, FInternetAddr& OutAddr) override;
	virtual ESocketErrors GetHostByName(const ANSICHAR* HostName, FInternetAddr& OutAddr) override;

	virtual bool Init(FString& Error) override;
	virtual void Shutdown() override;
	virtual bool HasNetworkDevice() override;
	virtual FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, ESocketProtocolFamily ProtocolType) override;

	virtual TSharedRef<FInternetAddr> GetLocalHostAddr(FOutputDevice& Out, bool& bCanBindAll) override;
	virtual class FSocketBSD* InternalBSDSocketFactory(SOCKET Socket, ESocketType SocketType, const FString& SocketDescription, ESocketProtocolFamily SocketProtocol) override;
};