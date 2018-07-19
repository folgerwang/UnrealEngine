// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDeviceRedirector.h"
#include "SocketSubsystemPackage.h"
#include "SocketTypes.h"

#if PLATFORM_HAS_BSD_SOCKETS || PLATFORM_HAS_BSD_IPV6_SOCKETS

class FInternetAddr;
class FSocketBSD;

#include "SocketSubsystemBSDPrivate.h"

/**
 * Standard BSD specific socket subsystem implementation
 */
class FSocketSubsystemBSD : public ISocketSubsystem
{
public:

	virtual class FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, ESocketProtocolFamily ProtocolType, bool bForceUDP = false);
	virtual ESocketErrors CreateAddressFromIP(const ANSICHAR* IPAddress, FInternetAddr& OutAddr);

	/**
	 * Gets the address information of the given hostname and outputs it into an array of resolvable addresses.
	 * It is up to the caller to determine which one is valid for their environment.
	 *
	 * @param HostName string version of the queryable hostname or ip address
	 * @param bResolveAddress if a blocking query to the DNS provider should be make to determine the ip address of the given hostname.
	 *						  if this flag is false, the call is asynchronous and will only work with ip addresses
	 * @param ProtocolType this is used to limit results from the call. Specifying None will search all valid protocols.
	 *					   Callers will find they rarely have to specify this flag.
	 *
	 * @return the array of results from GetAddrInfo
	 */
	virtual TArray<TSharedRef<FInternetAddr> > GetAddrInfo(const ANSICHAR* HostName, bool bResolveAddress, ESocketProtocolFamily ProtocolType = ESocketProtocolFamily::None);

	/**
	 * Specifies the default socket protocol family to use when creating a socket
	 * without explicitly passing in the protocol type on creation.
	 *
	 * All sockets created using the base class's CreateSocket will use this function
	 * to determine domain.
	 */
	virtual ESocketProtocolFamily GetDefaultSocketProtocolFamily() const
	{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
		return ESocketProtocolFamily::IPv6;
#else
		return ESocketProtocolFamily::IPv4;
#endif
	}

	// ISocketSubsystem interface
	virtual TSharedRef<FInternetAddr> CreateInternetAddr(uint32 Address = 0, uint32 Port = 0) override;
	virtual class FSocket* CreateSocket( const FName& SocketType, const FString& SocketDescription, bool bForceUDP = false ) override;
	virtual void DestroySocket( class FSocket* Socket ) override;
	virtual ESocketErrors GetHostByName( const ANSICHAR* HostName, FInternetAddr& OutAddr ) override;
	virtual bool GetHostName( FString& HostName ) override;
	virtual ESocketErrors GetLastErrorCode() override;

	virtual bool GetLocalAdapterAddresses( TArray<TSharedPtr<FInternetAddr> >& OutAdresses ) override
	{
		bool bCanBindAll;

		OutAdresses.Add(GetLocalHostAddr(*GLog, bCanBindAll));

		return true;
	}

	virtual const TCHAR* GetSocketAPIName() const override;

	virtual bool RequiresChatDataBeSeparate() override
	{
		return false;
	}

	virtual bool RequiresEncryptedPackets() override
	{
		return false;
	}

	virtual ESocketErrors TranslateErrorCode( int32 Code ) override;

protected:
	/**
	 * Translates return values of getaddrinfo() to socket error enum
	 */
	ESocketErrors TranslateGAIErrorCode(int32 Code) const;

	/**
	 * Translates an ESocketProtocolFamily code into a value usable by raw socket apis.
	 */
	virtual int32 GetProtocolFamilyValue(ESocketProtocolFamily InProtocol) const;

	/**
	 * Allows a subsystem subclass to create a FSocketBSD sub class.
	 */
	virtual class FSocketBSD* InternalBSDSocketFactory( SOCKET Socket, ESocketType SocketType, const FString& SocketDescription );

	// allow BSD sockets to use this when creating new sockets from accept() etc
	friend FSocketBSD;
};

#endif
