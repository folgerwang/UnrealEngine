// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SocketSubsystemIOS.h"
#include "SocketSubsystemModule.h"
#include "Modules/ModuleManager.h"
#include "BSDSockets/SocketsBSD.h"
#include "IPAddress.h"
#include <net/if.h>
#include <ifaddrs.h>
#include "SocketsBSDIOS.h"
#include "IPAddressBSDIOS.h"

FSocketSubsystemIOS* FSocketSubsystemIOS::SocketSingleton = NULL;

class FSocketBSD* FSocketSubsystemIOS::InternalBSDSocketFactory(SOCKET Socket, ESocketType SocketType, const FString& SocketDescription, ESocketProtocolFamily SocketProtocol)
{
	UE_LOG(LogIOS, Log, TEXT(" FSocketSubsystemIOS::InternalBSDSocketFactory"));
	return new FSocketBSDIOS(Socket, SocketType, SocketDescription, SocketProtocol, this);
}

FName CreateSocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	FName SubsystemName(TEXT("IOS"));
	// Create and register our singleton factor with the main online subsystem for easy access
	FSocketSubsystemIOS* SocketSubsystem = FSocketSubsystemIOS::Create();
	FString Error;
	if (SocketSubsystem->Init(Error))
	{
		SocketSubsystemModule.RegisterSocketSubsystem(SubsystemName, SocketSubsystem);
		return SubsystemName;
	}
	else
	{
		FSocketSubsystemIOS::Destroy();
		return NAME_None;
	}
}

void DestroySocketSubsystem( FSocketSubsystemModule& SocketSubsystemModule )
{
	SocketSubsystemModule.UnregisterSocketSubsystem(FName(TEXT("IOS")));
	FSocketSubsystemIOS::Destroy();
}

FSocketSubsystemIOS* FSocketSubsystemIOS::Create()
{
	if (SocketSingleton == NULL)
	{
		SocketSingleton = new FSocketSubsystemIOS();
	}

	return SocketSingleton;
}

void FSocketSubsystemIOS::Destroy()
{
	if (SocketSingleton != NULL)
	{
		SocketSingleton->Shutdown();
		delete SocketSingleton;
		SocketSingleton = NULL;
	}
}

bool FSocketSubsystemIOS::Init(FString& Error)
{
	return true;
}

void FSocketSubsystemIOS::Shutdown(void)
{
}


bool FSocketSubsystemIOS::HasNetworkDevice()
{
	return true;
}

ESocketErrors FSocketSubsystemIOS::CreateAddressFromIP(const ANSICHAR* IPAddress, FInternetAddr& OutAddr)
{
	return GetHostByName(IPAddress, OutAddr);
}

ESocketErrors FSocketSubsystemIOS::GetHostByName(const ANSICHAR* HostName, FInternetAddr& OutAddr)
{
	FAddressInfoResult GAIResult = GetAddressInfo(ANSI_TO_TCHAR(HostName), nullptr,
		EAddressInfoFlags::AllResultsWithMapping | EAddressInfoFlags::OnlyUsableAddresses, ESocketProtocolFamily::IPv6);

	if (GAIResult.Results.Num() > 0)
	{
		TSharedRef<FInternetAddrBSDIOS> ResultAddr = StaticCastSharedRef<FInternetAddrBSDIOS>(GAIResult.Results[0].Address);
		OutAddr.SetRawIp(ResultAddr->GetRawIp());
		static_cast<FInternetAddrBSDIOS&>(OutAddr).SetScopeId(ResultAddr->GetScopeId());
		return SE_NO_ERROR;
	}

	return SE_HOST_NOT_FOUND;
}

FSocket* FSocketSubsystemIOS::CreateSocket(const FName& SocketType, const FString& SocketDescription, ESocketProtocolFamily ProtocolType)
{
	FSocketBSD* NewSocket = (FSocketBSD*)FSocketSubsystemBSD::CreateSocket(SocketType, SocketDescription, ProtocolType);
	if (NewSocket)
	{
		if (ProtocolType != ESocketProtocolFamily::IPv4)
		{
			NewSocket->SetIPv6Only(false);
		}

		// disable the SIGPIPE exception 
		int bAllow = 1;
		setsockopt(NewSocket->GetNativeSocket(), SOL_SOCKET, SO_NOSIGPIPE, &bAllow, sizeof(bAllow));
	}
	return NewSocket;
}

TSharedRef<FInternetAddr> FSocketSubsystemIOS::GetLocalHostAddr(FOutputDevice& Out, bool& bCanBindAll)
{
	TSharedRef<FInternetAddrBSDIOS> HostAddr = MakeShareable(new FInternetAddrBSDIOS(this));
	HostAddr->SetAnyAddress();

	ifaddrs* Interfaces = NULL;
	bool bWasWifiSet = false;
	bool bWasCellSet = false;
	bool bWasIPv6Set = false;

	// get all of the addresses
	if (getifaddrs(&Interfaces) == 0)
	{
		// Loop through linked list of interfaces
		for (ifaddrs* Travel = Interfaces; Travel != NULL; Travel = Travel->ifa_next)
		{
			if (Travel->ifa_addr == NULL)
			{
				continue;
			}

			sockaddr_storage* AddrData = reinterpret_cast<sockaddr_storage*>(Travel->ifa_addr);
			uint32 ScopeInterfaceId = ntohl(if_nametoindex(Travel->ifa_name));
			if (Travel->ifa_addr->sa_family == AF_INET6)
			{
				if (strcmp(Travel->ifa_name, "en0") == 0)
				{
					HostAddr->SetIp(*AddrData);
					HostAddr->SetScopeId(ScopeInterfaceId);
					bWasWifiSet = true;
					bWasIPv6Set = true;
					UE_LOG(LogSockets, Verbose, TEXT("Set IP to WIFI %s"), *HostAddr->ToString(false));
				}
				else if (!bWasWifiSet && strcmp(Travel->ifa_name, "pdp_ip0") == 0)
				{
					HostAddr->SetIp(*AddrData);
					HostAddr->SetScopeId(ScopeInterfaceId);
					bWasCellSet = true;
					UE_LOG(LogSockets, Verbose, TEXT("Set IP to CELL %s"), *HostAddr->ToString(false));
				}
			}
			else if (!bWasIPv6Set && Travel->ifa_addr->sa_family == AF_INET)
			{
				if (strcmp(Travel->ifa_name, "en0") == 0)
				{
					HostAddr->SetIp(*AddrData);
					HostAddr->SetScopeId(ScopeInterfaceId);
					bWasWifiSet = true;
					UE_LOG(LogSockets, Verbose, TEXT("Set IP to WIFI IPv4 %s"), *HostAddr->ToString(false));
				}
				else if (!bWasWifiSet && strcmp(Travel->ifa_name, "pdp_ip0") == 0)
				{
					HostAddr->SetIp(*AddrData);
					HostAddr->SetScopeId(ScopeInterfaceId);
					bWasCellSet = true;
					UE_LOG(LogSockets, Verbose, TEXT("Set IP to CELL IPv4 %s"), *HostAddr->ToString(false));
				}
			}
		}
		
		// Free memory
		freeifaddrs(Interfaces);

		if (bWasWifiSet)
		{
			UE_LOG(LogIOS, Log, TEXT("Host addr is WIFI: %s"), *HostAddr->ToString(false));
		}
		else if (bWasCellSet)
		{
			UE_LOG(LogIOS, Log, TEXT("Host addr is CELL: %s"), *HostAddr->ToString(false));
		}
		else
		{
			UE_LOG(LogIOS, Log, TEXT("Host addr is INVALID"));
		}
	}

	// return the newly created address
	bCanBindAll = true;
	return HostAddr;
}

TSharedRef<FInternetAddr> FSocketSubsystemIOS::CreateInternetAddr(uint32 Address, uint32 Port)
{
	TSharedRef<FInternetAddr> Result = MakeShareable(new FInternetAddrBSDIOS(this));
	Result->SetIp(Address);
	Result->SetPort(Port);
	return Result;
}
