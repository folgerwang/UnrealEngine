// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDeviceRedirector.h"
#include "SocketSubsystemPackage.h"
#include "SocketSubsystem.h"

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

	virtual ESocketErrors CreateAddressFromIP(const ANSICHAR* IPAddress, FInternetAddr& OutAddr);

	/**
	 * Specifies the default socket protocol family to use when creating a socket
	 * without explicitly passing in the protocol type on creation.
	 *
	 * This function is mostly here for backwards compatibility. For best practice, moving to
	 * the new CreateSocket that takes a protocol is advised.
	 *
	 * All sockets created using the base class's CreateSocket will use this function
	 * to determine domain.
	 */
	virtual ESocketProtocolFamily GetDefaultSocketProtocolFamily() const
	{
		return ESocketProtocolFamily::IPv4;
	}

	// ISocketSubsystem interface
	virtual TSharedRef<FInternetAddr> CreateInternetAddr(uint32 Address = 0, uint32 Port = 0) override;
	virtual class FSocket* CreateSocket( const FName& SocketType, const FString& SocketDescription, bool bForceUDP = false ) override;
	virtual class FSocket* CreateSocket(const FName& SocketType, const FString& SocketDescription, ESocketProtocolFamily ProtocolType) override;
	virtual void DestroySocket( class FSocket* Socket ) override;

	virtual FAddressInfoResult GetAddressInfo(const TCHAR* HostName, const TCHAR* ServiceName = nullptr,
		EAddressInfoFlags QueryFlags = EAddressInfoFlags::Default,
		ESocketProtocolFamily ProtocolType = ESocketProtocolFamily::None,
		ESocketType SocketType = ESocketType::SOCKTYPE_Unknown) override;

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

	virtual bool IsSocketWaitSupported() const override;

	/**
	 * Translates an ESocketProtocolFamily code into a value usable by raw socket apis.
	 */
	virtual int32 GetProtocolFamilyValue(ESocketProtocolFamily InProtocol) const;

	/**
	 * Translates an raw socket family type value into an enum that can be used by the network layer.
	 */
	virtual ESocketProtocolFamily GetProtocolFamilyType(int32 InProtocol) const;

	/**
	 * Translates an raw socket protocol type value into an enum that can be used by the network layer.
	 */
	virtual ESocketType GetSocketType(int32 InSocketType) const;

	/**
	 * Translates an ESocketAddressInfoFlags into a value usable by getaddrinfo
	 */
	virtual int32 GetAddressInfoHintFlag(EAddressInfoFlags InFlags) const;

protected:
	/**
	 * Translates return values of getaddrinfo() to socket error enum
	 */
	ESocketErrors TranslateGAIErrorCode(int32 Code) const;

	/**
	 * Allows a subsystem subclass to create a FSocketBSD sub class.
	 */
	virtual class FSocketBSD* InternalBSDSocketFactory( SOCKET Socket, ESocketType SocketType, const FString& SocketDescription, ESocketProtocolFamily SocketProtocol);

	/**
	 * Allows a subsystem subclass to create a FSocketBSD sub class.
	 */
	UE_DEPRECATED(4.22, "To support multiple stack types, move to the constructor that allows for specifying the protocol stack to initialize the socket on.")
	virtual class FSocketBSD* InternalBSDSocketFactory(SOCKET Socket, ESocketType SocketType, const FString& SocketDescription)
	{
		return InternalBSDSocketFactory(Socket, SocketType, SocketDescription, GetDefaultSocketProtocolFamily());
	}

	// allow BSD sockets to use this when creating new sockets from accept() etc
	friend FSocketBSD;
};

#endif
