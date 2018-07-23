// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BSDIPv6Sockets/SocketSubsystemBSDIPv6.h"
#include "Misc/ScopeLock.h"

#if PLATFORM_HAS_BSD_IPV6_SOCKETS

#include "BSDIPv6Sockets/IPAddressBSDIPv6.h"

PRAGMA_DISABLE_DEPRECATION_WARNINGS
class FSocketBSDIPv6* FSocketSubsystemBSDIPv6::InternalBSDSocketFactory(SOCKET Socket, ESocketType SocketType, const FString& SocketDescription)
{
	// return a new socket object
	return new FSocketBSDIPv6(Socket, SocketType, SocketDescription, this);
}

ESocketErrors FSocketSubsystemBSDIPv6::TranslateGAIErrorCode(int32 Code) const
{
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_GETADDRINFO
	switch (Code)
	{
		// getaddrinfo() has its own error codes
	case EAI_AGAIN:			return SE_TRY_AGAIN;
	case EAI_BADFLAGS:		return SE_EINVAL;
	case EAI_FAIL:			return SE_NO_RECOVERY;
	case EAI_FAMILY:		return SE_EAFNOSUPPORT;
	case EAI_MEMORY:		return SE_ENOBUFS;
	case EAI_NONAME:		return SE_HOST_NOT_FOUND;
	case EAI_SERVICE:		return SE_EPFNOSUPPORT;
	case EAI_SOCKTYPE:		return SE_ESOCKTNOSUPPORT;
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS
	case WSANO_DATA:		return SE_NO_DATA;
	case WSANOTINITIALISED: return SE_NOTINITIALISED;
#else			
	case EAI_NODATA:		return SE_NO_DATA;
	case EAI_ADDRFAMILY:	return SE_ADDRFAMILY;
	case EAI_SYSTEM:		return SE_SYSTEM;
#endif
	case 0:					break; // 0 means success
	default:
		UE_LOG(LogSockets, Warning, TEXT("Unhandled getaddrinfo() socket error! Code: %d"), Code);
		return SE_EINVAL;
	}
#endif // PLATFORM_HAS_BSD_SOCKET_FEATURE_GETADDRINFO

	return SE_NO_ERROR;
}

FSocket* FSocketSubsystemBSDIPv6::CreateSocket(const FName& SocketType, const FString& SocketDescription, bool bForceUDP)
{
	SOCKET Socket = INVALID_SOCKET;
	FSocket* NewSocket = NULL;
	switch (SocketType.GetComparisonIndex())
	{
	case NAME_DGram:
		// Creates a data gram (UDP) socket
		Socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
		NewSocket = (Socket != INVALID_SOCKET) ? InternalBSDSocketFactory(Socket, SOCKTYPE_Datagram, SocketDescription) : NULL;
		break;
	case NAME_Stream:
		// Creates a stream (TCP) socket
		Socket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
		NewSocket = (Socket != INVALID_SOCKET) ? InternalBSDSocketFactory(Socket, SOCKTYPE_Streaming, SocketDescription) : NULL;
		break;
	}

	if (!NewSocket)
	{
		UE_LOG(LogSockets, Warning, TEXT("Failed to create IPv6 socket %s [%s]"), *SocketType.ToString(), *SocketDescription);
	}

	return NewSocket;
}

FResolveInfoCached* FSocketSubsystemBSDIPv6::CreateResolveInfoCached(TSharedPtr<FInternetAddr> Addr) const
{
	return new FResolveInfoCachedBSDIPv6(*Addr);
}

void FSocketSubsystemBSDIPv6::DestroySocket(FSocket* Socket)
{
	delete Socket;
}

TArray<TSharedRef<FInternetAddr> > FSocketSubsystemBSDIPv6::GetAddressInfo(const ANSICHAR* HostName, bool bResolveAddress, ESocketProtocolFamily ProtocolType)
{
	TArray<TSharedRef<FInternetAddr> > GetAddrArray;

#if PLATFORM_HAS_BSD_SOCKET_FEATURE_GETADDRINFO
	addrinfo* AddrInfo = nullptr;

	addrinfo HintAddrInfo;
	FMemory::Memzero(&HintAddrInfo, sizeof(HintAddrInfo));
	HintAddrInfo.ai_family = AF_UNSPEC;
	HintAddrInfo.ai_flags = AI_ADDRCONFIG | AI_PASSIVE;

	if (!bResolveAddress)
	{
		HintAddrInfo.ai_flags |= AI_NUMERICHOST;
	}

	int32 ErrorCode = getaddrinfo(HostName, nullptr, &HintAddrInfo, &AddrInfo);
	ESocketErrors SocketError = TranslateGAIErrorCode(ErrorCode);
	if (SocketError == SE_NO_ERROR)
	{
		for (; AddrInfo != nullptr; AddrInfo = AddrInfo->ai_next)
		{
			if (AddrInfo->ai_family == AF_INET6 || AddrInfo->ai_family == AF_INET)
			{
				TSharedRef<FInternetAddrBSDIPv6> NewAddr = MakeShareable(new FInternetAddrBSDIPv6);
				if (AddrInfo->ai_family == AF_INET6)
				{
					sockaddr_in6* IPv6SockAddr = reinterpret_cast<sockaddr_in6*>(AddrInfo->ai_addr);
					if (IPv6SockAddr != nullptr)
					{
#if PLATFORM_IOS
						NewAddr->SetIp(*IPv6SockAddr);
#else
						NewAddr->SetIp(IPv6SockAddr->sin6_addr);
#endif
					}
				}
				else if (AddrInfo->ai_family == AF_INET)
				{
					sockaddr_in* IPv4SockAddr = reinterpret_cast<sockaddr_in*>(AddrInfo->ai_addr);
					if (IPv4SockAddr != nullptr)
					{
						NewAddr->SetIp(IPv4SockAddr->sin_addr);
					}
				}
				GetAddrArray.Add(NewAddr);
			}
		}
		freeaddrinfo(AddrInfo);
	}
#else
	UE_LOG(LogSockets, Error, TEXT("Platform has no getaddrinfo(), but did not override FSocketSubsystem::GetAddressInfo()"));
#endif
	return GetAddrArray;
}

ESocketErrors FSocketSubsystemBSDIPv6::GetHostByName(const ANSICHAR* HostName, FInternetAddr& OutAddr)
{
	TArray<TSharedRef<FInternetAddr> > Results = GetAddressInfo(HostName, true);
	if (Results.Num() > 0)
	{
		OutAddr.SetRawIp(Results[0]->GetRawIp());
		return SE_NO_ERROR;
	}

	return SE_HOST_NOT_FOUND;
}

bool FSocketSubsystemBSDIPv6::GetHostName(FString& HostName)
{
	ANSICHAR Buffer[256];
	bool bRead = gethostname(Buffer,256) == 0;
	if (bRead == true)
	{
		HostName = Buffer;
	}
	return bRead;
}

const TCHAR* FSocketSubsystemBSDIPv6::GetSocketAPIName() const
{
	return TEXT("BSD IPv6");
}

TSharedRef<FInternetAddr> FSocketSubsystemBSDIPv6::CreateInternetAddr(uint32 Address, uint32 Port)
{
	TSharedRef<FInternetAddr> Result = MakeShareable(new FInternetAddrBSDIPv6);
	Result->SetIp(Address);
	Result->SetPort(Port);
	return Result;
}

ESocketErrors FSocketSubsystemBSDIPv6::GetLastErrorCode()
{
	return TranslateErrorCode(errno);
}


ESocketErrors FSocketSubsystemBSDIPv6::TranslateErrorCode(int32 Code)
{
	// @todo sockets: Windows for some reason doesn't seem to have all of the standard error messages,
	// but it overrides this function anyway - however, this 
#if !PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS

	// handle the generic -1 error
	if (Code == SOCKET_ERROR)
	{
		return GetLastErrorCode();
	}

	switch (Code)
	{
	case 0: return SE_NO_ERROR;
	case EINTR: return SE_EINTR;
	case EBADF: return SE_EBADF;
	case EACCES: return SE_EACCES;
	case EFAULT: return SE_EFAULT;
	case EINVAL: return SE_EINVAL;
	case EMFILE: return SE_EMFILE;
	case EWOULDBLOCK: return SE_EWOULDBLOCK;
	case EINPROGRESS: return SE_EINPROGRESS;
	case EALREADY: return SE_EALREADY;
	case ENOTSOCK: return SE_ENOTSOCK;
	case EDESTADDRREQ: return SE_EDESTADDRREQ;
	case EMSGSIZE: return SE_EMSGSIZE;
	case EPROTOTYPE: return SE_EPROTOTYPE;
	case ENOPROTOOPT: return SE_ENOPROTOOPT;
	case EPROTONOSUPPORT: return SE_EPROTONOSUPPORT;
	case ESOCKTNOSUPPORT: return SE_ESOCKTNOSUPPORT;
	case EOPNOTSUPP: return SE_EOPNOTSUPP;
	case EPFNOSUPPORT: return SE_EPFNOSUPPORT;
	case EAFNOSUPPORT: return SE_EAFNOSUPPORT;
	case EADDRINUSE: return SE_EADDRINUSE;
	case EADDRNOTAVAIL: return SE_EADDRNOTAVAIL;
	case ENETDOWN: return SE_ENETDOWN;
	case ENETUNREACH: return SE_ENETUNREACH;
	case ENETRESET: return SE_ENETRESET;
	case ECONNABORTED: return SE_ECONNABORTED;
	case ECONNRESET: return SE_ECONNRESET;
	case ENOBUFS: return SE_ENOBUFS;
	case EISCONN: return SE_EISCONN;
	case ENOTCONN: return SE_ENOTCONN;
	case ESHUTDOWN: return SE_ESHUTDOWN;
	case ETOOMANYREFS: return SE_ETOOMANYREFS;
	case ETIMEDOUT: return SE_ETIMEDOUT;
	case ECONNREFUSED: return SE_ECONNREFUSED;
	case ELOOP: return SE_ELOOP;
	case ENAMETOOLONG: return SE_ENAMETOOLONG;
	case EHOSTDOWN: return SE_EHOSTDOWN;
	case EHOSTUNREACH: return SE_EHOSTUNREACH;
	case ENOTEMPTY: return SE_ENOTEMPTY;
	case EUSERS: return SE_EUSERS;
	case EDQUOT: return SE_EDQUOT;
	case ESTALE: return SE_ESTALE;
	case EREMOTE: return SE_EREMOTE;
#if !PLATFORM_HAS_NO_EPROCLIM
	case EPROCLIM: return SE_EPROCLIM;
#endif
	case EPIPE: return SE_ECONNRESET; // for when backgrounding with an open pipe to a server
	case HOST_NOT_FOUND: return SE_HOST_NOT_FOUND;
	case TRY_AGAIN: return SE_TRY_AGAIN;
	case NO_RECOVERY: return SE_NO_RECOVERY;
//	case NO_DATA: return SE_NO_DATA;
		// case : return SE_UDP_ERR_PORT_UNREACH; //@TODO Find it's replacement
	}
#endif

	UE_LOG(LogSockets, Warning, TEXT("Unhandled socket error! Error Code: %d. Returning SE_EINVAL!"), Code);
	ensure(0);
	return SE_EINVAL;
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif
