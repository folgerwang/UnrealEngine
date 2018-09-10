// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SocketTypes.h"

class FInternetAddr;

enum class EAddressInfoFlags : uint16
{
	/* Return all addresses regardless of spec. */
	AllResults = 1 << 0,
	/* Don't use DNS resolution of the string (AI_NUMERICHOST),
	 * this makes the address resolution non-blocking, requiring
	 * the hostname to be in the form of an ip address already
	 */
	NoResolveHost = 1 << 1,
	/* Don't resolve the service name (requires the service argument to be nullptr
	 * or a string based representation of a number). This is also-nonblocking.
	 */
	NoResolveService = 1 << 2,
	/* Only return addresses that adapters on this machine can use (AI_ADDRCONFIG) */
	OnlyUsableAddresses = 1 << 3,
	/* Return bindable addresses (AI_PASSIVE). Only works if the hostname argument is null. */
	BindableAddress = 1 << 4,
	/* Include the canonical name of the host with the results list */
	CanonicalName = 1 << 5,
	/* Include the fully qualified domain name of the host with the results list */
	FQDomainName = 1 << 6,
	/* Allow for IPv4 mapped IPv6 addresses */
	AllowV4MappedAddresses = 1 << 7,
	/* Get all addresses, but return V4 mapped IPv6 addresses */
	AllResultsWithMapping = AllowV4MappedAddresses | AllResults,
	/* The default value of a hints flag for the platform (typically just 0) */
	Default = 0,
};

ENUM_CLASS_FLAGS(EAddressInfoFlags);

struct FAddressInfoResultData
{
	FAddressInfoResultData(TSharedRef<FInternetAddr> InAddr, SIZE_T InAddrLen, ESocketProtocolFamily InProtocol, ESocketType InSocketConfiguration) :
		AddressProtocol(InProtocol),
		SocketConfiguration(InSocketConfiguration),
		AddressLen(InAddrLen),
		Address(InAddr)
	{
	}

	FName GetSocketTypeName() const
	{
		switch (SocketConfiguration)
		{
		case SOCKTYPE_Datagram:
			return NAME_DGram;
			break;
		case SOCKTYPE_Streaming:
			return NAME_Stream;
			break;
		default:
		case SOCKTYPE_Unknown:
			return NAME_None;
			break;
		}
	}

	/* The protocol of the address stored */
	ESocketProtocolFamily AddressProtocol;
	/* Streaming or datagram */
	ESocketType SocketConfiguration;
	/* Length of the returned address data */
	SIZE_T AddressLen;
	/* The address associated with this result */
	TSharedRef<FInternetAddr> Address;
};

struct FAddressInfoResult
{
	FAddressInfoResult(const TCHAR* InHostName, const TCHAR* InServiceName) :
		QueryHostName(InHostName),
		QueryServiceName(InServiceName)
	{
	}

	/* The hostname that generated these results */
	FString QueryHostName;
	/* The service name that was used in the query */
	FString QueryServiceName;
	/* The Canonical Name of the query (empty unless FQDomainName or CanonicalName are specified) */
	FString CanonicalNameResult;
	/* The list of results */
	TArray<FAddressInfoResultData> Results;
};

