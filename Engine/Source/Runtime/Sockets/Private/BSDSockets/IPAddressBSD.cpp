// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IPAddressBSD.h"
#include "SocketSubsystemBSD.h"

#if PLATFORM_HAS_BSD_SOCKETS || PLATFORM_HAS_BSD_IPV6_SOCKETS

FInternetAddrBSD::FInternetAddrBSD()
{
	Clear();
}

void FInternetAddrBSD::Clear()
{
	FMemory::Memzero(&Addr, sizeof(Addr));
	Addr.ss_family = AF_UNSPEC;
}

void FInternetAddrBSD::ResetScopeId()
{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	((sockaddr_in6*)&Addr)->sin6_scope_id = 0;
#endif
}

void FInternetAddrBSD::SetIp(const sockaddr_storage& IpAddr)
{
	// Instead of just replacing the structures entirely, we copy only the ip portion.
	// As this should not also set port.
	if (IpAddr.ss_family == AF_INET)
	{
		const sockaddr_in* SockAddr = (const sockaddr_in*)&IpAddr;
		SetIp(SockAddr->sin_addr);
	}

#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	if (IpAddr.ss_family == AF_INET6)
	{
		const sockaddr_in6* SockAddr = (const sockaddr_in6*)&IpAddr;
		SetIp(SockAddr->sin6_addr);
		// Remember to set the scope id.
		((sockaddr_in6*)&Addr)->sin6_scope_id = SockAddr->sin6_scope_id;
	}
#endif
}

void FInternetAddrBSD::SetIp(const TCHAR* InAddr, bool& bIsValid)
{
	bIsValid = false;
	FString AddressString(InAddr);
	FString Port;

	const bool bHasOpenBracket = AddressString.Contains("[");
	const int32 CloseBracketIndex = AddressString.Find("]");
	const bool bHasCloseBracket = CloseBracketIndex != INDEX_NONE;
	bool bIsIPv6 = bHasOpenBracket && bHasCloseBracket;

	const int32 LastColonIndex = AddressString.Find(":", ESearchCase::IgnoreCase, ESearchDir::FromEnd);

	// IPv4 address will only have a port when a colon is present.
	// IPv6 address will only have a port when surrounded by brackets.
	const bool bHasPort = (INDEX_NONE != LastColonIndex) && (!bIsIPv6 || (bHasCloseBracket && LastColonIndex > CloseBracketIndex));

	if (bHasPort)
	{
		Port = AddressString.RightChop(LastColonIndex + 1);
		AddressString = AddressString.Left(LastColonIndex);
	}

	AddressString.RemoveFromStart("[");
	AddressString.RemoveFromEnd("]");
	
	const auto InAddrAnsi = StringCast<ANSICHAR>(*AddressString);
	FSocketSubsystemBSD* SocketSubsystem = static_cast<FSocketSubsystemBSD*>(ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM));
	if (SocketSubsystem)
	{
		bIsValid = (SocketSubsystem->CreateAddressFromIP(InAddrAnsi.Get(), *this) == SE_NO_ERROR);
		if (bHasPort && bIsValid)
		{
			SetPort(FCString::Atoi(*Port));
		}
	}
}

TArray<uint8> FInternetAddrBSD::GetRawIp() const
{
	TArray<uint8> RawAddressArray;
	if (Addr.ss_family == AF_INET)
	{
		sockaddr_in* IPv4Addr = ((sockaddr_in*)&Addr);
		uint32 IntAddr = IPv4Addr->sin_addr.s_addr;
		RawAddressArray.Add((IntAddr >> 24) & 0xFF);
		RawAddressArray.Add((IntAddr >> 16) & 0xFF);
		RawAddressArray.Add((IntAddr >> 8) & 0xFF);
		RawAddressArray.Add((IntAddr >> 0) & 0xFF);
	}
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	else if (Addr.ss_family == AF_INET6)
	{
		sockaddr_in6* IPv6Addr = ((sockaddr_in6*)&Addr);
		for (int i = 0; i < 16; ++i)
		{
			RawAddressArray.Add(IPv6Addr->sin6_addr.s6_addr[i]);
		}

		// Copy over the interface if we have it explicitly set (anything other than 0)
		if (IPv6Addr->sin6_scope_id != 0)
		{
			uint32 RawScopeId = IPv6Addr->sin6_scope_id;
			RawAddressArray.Add((RawScopeId >> 24) & 0xFF);
			RawAddressArray.Add((RawScopeId >> 16) & 0xFF);
			RawAddressArray.Add((RawScopeId >> 8) & 0xFF);
			RawAddressArray.Add((RawScopeId >> 0) & 0xFF);
		}
	}
#endif

	return RawAddressArray;
}

void FInternetAddrBSD::SetRawIp(const TArray<uint8>& RawAddr)
{
	if (RawAddr.Num() == 4) // This is IPv4
	{
		Addr.ss_family = AF_INET;
		sockaddr_in* IPv4Addr = ((sockaddr_in*)&Addr);
		IPv4Addr->sin_addr.s_addr = (RawAddr[0] << 24) | (RawAddr[1] << 16) | (RawAddr[2] << 8) | (RawAddr[3] << 0);
	}
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	else if(RawAddr.Num() >= 16) // We are IPv6
	{
		sockaddr_in6* IPv6Addr = ((sockaddr_in6*)&Addr);
		for (int i = 0; i < 16; ++i)
		{
			IPv6Addr->sin6_addr.s6_addr[i] = RawAddr[i];
		}

		// If this address has an interface, we'll copy it over too.
		if (RawAddr.Num() == 20)
		{
			IPv6Addr->sin6_scope_id = (RawAddr[16] << 24) | (RawAddr[17] << 16) | (RawAddr[18] << 8) | (RawAddr[19] << 0);
		}

		Addr.ss_family = AF_INET6;
	}
#endif
	else
	{
		Clear();
	}
}

void FInternetAddrBSD::SetPort(int32 InPort)
{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	if (GetProtocolFamily() == ESocketProtocolFamily::IPv6)
	{
		((sockaddr_in6*)&Addr)->sin6_port = htons(InPort);
		return;
	}
#endif

	((sockaddr_in*)&Addr)->sin_port = htons(InPort);
}

int32 FInternetAddrBSD::GetPort() const
{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	if (GetProtocolFamily() == ESocketProtocolFamily::IPv6)
	{
		return ntohs(((sockaddr_in6*)&Addr)->sin6_port);
	}
#endif

	return ntohs(((sockaddr_in*)&Addr)->sin_port);
}

void FInternetAddrBSD::SetAnyAddress()
{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	SetAnyIPv6Address();
#else
	SetAnyIPv4Address();
#endif
}

void FInternetAddrBSD::SetAnyIPv4Address()
{
	((sockaddr_in*)&Addr)->sin_addr.s_addr = htonl(INADDR_ANY);
	Addr.ss_family = AF_INET;
	SetPort(0);
}

void FInternetAddrBSD::SetAnyIPv6Address()
{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	SetIp(in6addr_any);
#endif
	ResetScopeId();
	SetPort(0);
}

void FInternetAddrBSD::SetBroadcastAddress()
{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	SetIPv6BroadcastAddress();
#else
	SetIPv4BroadcastAddress();
#endif
}

void FInternetAddrBSD::SetIPv4BroadcastAddress()
{
	((sockaddr_in*)&Addr)->sin_addr.s_addr = htonl(INADDR_BROADCAST);
	Addr.ss_family = AF_INET;
	SetPort(0);
}

void FInternetAddrBSD::SetIPv6BroadcastAddress()
{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	// broadcast means something different in IPv6, but this is a rough equivalent
#ifndef in6addr_allnodesonlink
		// see RFC 4291, link-local multicast address http://tools.ietf.org/html/rfc4291
	static in6_addr in6addr_allnodesonlink =
	{
		{ { 0xff, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 } }
	};
#endif // in6addr_allnodesonlink
	SetIp(in6addr_allnodesonlink);
#endif
	ResetScopeId();
	SetPort(0);
}

void FInternetAddrBSD::SetLoopbackAddress()
{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	SetIPv6LoopbackAddress();
#else
	SetIPv4LoopbackAddress();
#endif
}

void FInternetAddrBSD::SetIPv4LoopbackAddress()
{
	((sockaddr_in*)&Addr)->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	Addr.ss_family = AF_INET;
	SetPort(0);
}

void FInternetAddrBSD::SetIPv6LoopbackAddress()
{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	SetIp(in6addr_loopback);
#endif
	ResetScopeId();
	SetPort(0);
}

FString FInternetAddrBSD::ToString(bool bAppendPort) const
{
	FString ReturnVal("");
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_GETNAMEINFO
	char IPStr[NI_MAXHOST];
	if (getnameinfo((const sockaddr*)&Addr, GetStorageSize(), IPStr, NI_MAXHOST, nullptr, 0, NI_NUMERICHOST) == 0)
	{
		if (GetProtocolFamily() == ESocketProtocolFamily::IPv6)
		{
			FString IPv6Str(ANSI_TO_TCHAR(IPStr));
			// Remove the scope interface if it exists.
			const int32 InterfaceMarkerIndex = IPv6Str.Find("%", ESearchCase::IgnoreCase, ESearchDir::FromEnd);
			if (InterfaceMarkerIndex != INDEX_NONE)
			{
				IPv6Str = IPv6Str.Left(InterfaceMarkerIndex);
			}

			ReturnVal = FString::Printf(TEXT("[%s]"), *IPv6Str);
		}
		else
		{
			ReturnVal = IPStr;
		}

		if (bAppendPort)
		{
			ReturnVal += FString::Printf(TEXT(":%d"), GetPort());
		}
	}
#else
	UE_LOG(LogSockets, Error, TEXT("Platform has no getnameinfo(), but did not override FInternetAddrBSD::ToString()"));
#endif
	return ReturnVal;
}

bool FInternetAddrBSD::operator==(const FInternetAddr& Other) const
{
	const FInternetAddrBSD& OtherBSD = static_cast<const FInternetAddrBSD&>(Other);
	ESocketProtocolFamily CurrentFamily = GetProtocolFamily();

	// Check if the addr families match
	if (OtherBSD.Addr.ss_family != Addr.ss_family)
	{
		return false;
	}

#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	if (CurrentFamily == ESocketProtocolFamily::IPv6)
	{
		const sockaddr_in6* OtherBSDAddr = (sockaddr_in6*)&(OtherBSD.Addr);
		const sockaddr_in6* ThisBSDAddr = ((sockaddr_in6*)&Addr);
		return memcmp(&(ThisBSDAddr->sin6_addr), &(OtherBSDAddr->sin6_addr), sizeof(in6_addr)) == 0 &&
			ThisBSDAddr->sin6_port == OtherBSDAddr->sin6_port;
	}
#endif

	if (CurrentFamily == ESocketProtocolFamily::IPv4)
	{
		const sockaddr_in* OtherBSDAddr = (sockaddr_in*)&(OtherBSD.Addr);
		const sockaddr_in* ThisBSDAddr = ((sockaddr_in*)&Addr);
		return ThisBSDAddr->sin_addr.s_addr == OtherBSDAddr->sin_addr.s_addr &&
			ThisBSDAddr->sin_port == OtherBSDAddr->sin_port;
	}

	return false;
}

bool FInternetAddrBSD::IsValid() const
{
	ESocketProtocolFamily CurrentFamily = GetProtocolFamily();

	if (CurrentFamily == ESocketProtocolFamily::IPv4)
	{
		return ((sockaddr_in*)&Addr)->sin_addr.s_addr != 0;
	}

#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	if (CurrentFamily == ESocketProtocolFamily::IPv6)
	{
		in6_addr EmptyAddr;
		FMemory::Memzero(EmptyAddr);

		sockaddr_in6* ThisBSDAddr = ((sockaddr_in6*)&Addr);
		return memcmp(&(ThisBSDAddr->sin6_addr), &EmptyAddr, sizeof(in6_addr)) != 0;
	}
#endif

	return false;
}

TSharedRef<FInternetAddr> FInternetAddrBSD::Clone() const
{
	TSharedRef<FInternetAddrBSD> NewAddress = MakeShareable(new FInternetAddrBSD);
	NewAddress->Addr = Addr;
	return NewAddress;
}

ESocketProtocolFamily FInternetAddrBSD::GetProtocolFamily() const
{
	switch (Addr.ss_family)
	{
		case AF_INET:
			return ESocketProtocolFamily::IPv4;
		break;
		case AF_INET6:
			return ESocketProtocolFamily::IPv6;
		break;
		default:
			return ESocketProtocolFamily::None;
	}
}

SOCKLEN FInternetAddrBSD::GetStorageSize() const
{
	if (GetProtocolFamily() == ESocketProtocolFamily::IPv4)
	{
		return sizeof(sockaddr_in);
	}
	else
	{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
		return sizeof(sockaddr_in6);
#endif
	}

	return sizeof(sockaddr_storage);
}

uint32 FInternetAddrBSD::GetTypeHash()
{
	ESocketProtocolFamily CurrentFamily = GetProtocolFamily();

	if (CurrentFamily == ESocketProtocolFamily::IPv4)
	{
		uint32 NumericAddress;
		GetIp(NumericAddress);
		return NumericAddress + (GetPort() * 23);
	}
	else if (CurrentFamily == ESocketProtocolFamily::IPv6)
	{
		return ::GetTypeHash(*ToString(true));
	}

	return 0;
}

#endif
