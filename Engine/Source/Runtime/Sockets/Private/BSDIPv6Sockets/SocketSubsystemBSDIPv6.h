// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDeviceRedirector.h"
#include "BSDSockets/SocketSubsystemBSDPrivate.h"
#include "BSDIPv6Sockets/SocketsBSDIPv6.h"
#include "IPAddress.h"
#include "SocketSubsystemPackage.h"

#if PLATFORM_HAS_BSD_IPV6_SOCKETS

/**
 * Standard BSD specific IPv6 socket subsystem implementation
 */
class UE_DEPRECATED(4.21, "Move to FSocketSubsystemBSD") FSocketSubsystemBSDIPv6
	: public ISocketSubsystem
{
public:

	//~ Begin ISocketSubsystem Interface

	virtual TSharedRef<FInternetAddr> CreateInternetAddr( uint32 Address = 0, uint32 Port = 0 ) override;

	virtual class FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, ESocketProtocolFamily ProtocolType) override;

	virtual FResolveInfoCached* CreateResolveInfoCached(TSharedPtr<FInternetAddr> Addr) const override;

	virtual void DestroySocket( class FSocket* Socket ) override;

	virtual FAddressInfoResult GetAddressInfo(const TCHAR* HostName, const TCHAR* ServiceName = nullptr,
		EAddressInfoFlags QueryFlags = EAddressInfoFlags::Default,
		ESocketProtocolFamily ProtocolType = ESocketProtocolFamily::None,
		ESocketType SocketType = ESocketType::SOCKTYPE_Unknown) override;

	virtual ESocketErrors GetHostByName( const ANSICHAR* HostName, FInternetAddr& OutAddr ) override;

	virtual bool GetHostName( FString& HostName ) override;

	virtual ESocketErrors GetLastErrorCode( ) override;

	virtual bool GetLocalAdapterAddresses( TArray<TSharedPtr<FInternetAddr> >& OutAdresses ) override
	{
		bool bCanBindAll;

		OutAdresses.Add(GetLocalHostAddr(*GLog, bCanBindAll));

		return true;
	}

	virtual const TCHAR* GetSocketAPIName( ) const override;

	virtual bool RequiresChatDataBeSeparate( ) override
	{
		return false;
	}

	virtual bool RequiresEncryptedPackets( ) override
	{
		return false;
	}

	virtual ESocketErrors TranslateErrorCode( int32 Code ) override;

	virtual bool IsSocketWaitSupported() const override;

	//~ End ISocketSubsystem Interface


protected:

	/**
	 * Allows a subsystem subclass to create a FSocketBSD sub class.
	 */
	virtual class FSocketBSDIPv6* InternalBSDSocketFactory( SOCKET Socket, ESocketType SocketType, const FString& SocketDescription, ESocketProtocolFamily SocketProtocol );


	/**
	 * Allows a subsystem subclass to create a FSocketBSD sub class.
	 */
	UE_DEPRECATED(4.22, "To support multiple stack types, move to the constructor that allows for specifying the protocol stack to initialize the socket on.")
	virtual class FSocketBSDIPv6* InternalBSDSocketFactory(SOCKET Socket, ESocketType SocketType, const FString& SocketDescription)
	{
		return InternalBSDSocketFactory(Socket, SocketType, SocketDescription, ESocketProtocolFamily::IPv6);
	}

	/**
	 * Translates an ESocketAddressInfoFlags into a value usable by getaddrinfo
	 */
	virtual int32 GetAddressInfoHintFlag(EAddressInfoFlags InFlags) const;

	// allow BSD sockets to use this when creating new sockets from accept() etc
	friend class FSocketBSDIPv6;

	// Used to prevent multiple threads accessing the shared data.
	FCriticalSection HostByNameSynch;

	ESocketErrors TranslateGAIErrorCode(int32 Code) const;
};
#endif
