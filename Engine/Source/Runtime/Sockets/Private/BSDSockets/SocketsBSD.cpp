// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BSDSockets/SocketsBSD.h"

#if PLATFORM_HAS_BSD_SOCKETS || PLATFORM_HAS_BSD_IPV6_SOCKETS

#include "BSDSockets/IPAddressBSD.h"
#include "BSDSockets/SocketSubsystemBSD.h"
//#include "Net/NetworkProfiler.h"

#if PLATFORM_HTML5 
	typedef unsigned long u_long;
#endif 


/* FSocket overrides
 *****************************************************************************/

bool FSocketBSD::Shutdown(ESocketShutdownMode Mode)
{
	int InternalMode = 0;

#if PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS
	// Windows uses different constants than POSIX
	switch (Mode)
	{
		case ESocketShutdownMode::Read:
			InternalMode = SD_RECEIVE;
			break;
		case ESocketShutdownMode::Write:
			InternalMode = SD_SEND;
			break;
		case ESocketShutdownMode::ReadWrite:
			InternalMode = SD_BOTH;
			break;
	}
#else
	switch (Mode)
	{
		case ESocketShutdownMode::Read:
			InternalMode = SHUT_RD;
			break;
		case ESocketShutdownMode::Write:
			InternalMode = SHUT_WR;
			break;
		case ESocketShutdownMode::ReadWrite:
			InternalMode = SHUT_RDWR;
			break;
	}
#endif

	return shutdown(Socket, InternalMode) == 0;
}

bool FSocketBSD::Close(void)
{
	if (Socket != INVALID_SOCKET)
	{
		int32 error = closesocket(Socket);
		Socket = INVALID_SOCKET;
		return error == 0;
	}
	return false;
}


bool FSocketBSD::Bind(const FInternetAddr& Addr)
{
	const FInternetAddrBSD& BSDAddr = static_cast<const FInternetAddrBSD&>(Addr);
	return bind(Socket, (const sockaddr*)&(BSDAddr.Addr), BSDAddr.GetStorageSize()) == 0;
}


bool FSocketBSD::Connect(const FInternetAddr& Addr)
{
	const FInternetAddrBSD& BSDAddr = static_cast<const FInternetAddrBSD&>(Addr);
	int32 Return = connect(Socket, (const sockaddr*)&(BSDAddr.Addr), BSDAddr.GetStorageSize());
	
	check(SocketSubsystem);
	ESocketErrors Error = SocketSubsystem->TranslateErrorCode(Return);

	// EWOULDBLOCK is not an error, and EINPROGRESS is fine on initial connection as it may still be creating for nonblocking sockets
	return ((Error == SE_NO_ERROR) || (Error == SE_EWOULDBLOCK) || (Error == SE_EINPROGRESS));
}


bool FSocketBSD::Listen(int32 MaxBacklog)
{
	return listen(Socket, MaxBacklog) == 0;
}


bool FSocketBSD::WaitForPendingConnection(bool& bHasPendingConnection, const FTimespan& WaitTime)
{
	bool bHasSucceeded = false;
	bHasPendingConnection = false;

	// make sure socket has no error state
	if (HasState(ESocketBSDParam::HasError) == ESocketBSDReturn::No)
	{
		// get the read state
		ESocketBSDReturn State = HasState(ESocketBSDParam::CanRead, WaitTime);
		
		// turn the result into the outputs
		bHasSucceeded = State != ESocketBSDReturn::EncounteredError;
		bHasPendingConnection = State == ESocketBSDReturn::Yes;
	}

	return bHasSucceeded;
}


bool FSocketBSD::HasPendingData(uint32& PendingDataSize)
{
	PendingDataSize = 0;

	// make sure socket has no error state
	if (HasState(ESocketBSDParam::CanRead) == ESocketBSDReturn::Yes)
	{
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_IOCTL
		// See if there is any pending data on the read socket
		if (ioctlsocket(Socket, FIONREAD, (u_long*)(&PendingDataSize)) == 0)
#endif
		{
			return (PendingDataSize > 0);
		}
	}

	return false;
}


FSocket* FSocketBSD::Accept(const FString& InSocketDescription)
{
	SOCKET NewSocket = accept(Socket, NULL, NULL);

	if (NewSocket != INVALID_SOCKET)
	{
		// we need the subclass to create the actual FSocket object
		check(SocketSubsystem);
		FSocketSubsystemBSD* BSDSystem = static_cast<FSocketSubsystemBSD*>(SocketSubsystem);
		return BSDSystem->InternalBSDSocketFactory(NewSocket, SocketType, InSocketDescription, SocketProtocol);
	}

	return NULL;
}


FSocket* FSocketBSD::Accept(FInternetAddr& OutAddr, const FString& InSocketDescription)
{
	FInternetAddrBSD& BSDAddr = static_cast<FInternetAddrBSD&>(OutAddr);
	SOCKLEN SizeOf = sizeof(sockaddr_storage);
	SOCKET NewSocket = accept(Socket, (sockaddr*)&(BSDAddr.Addr), &SizeOf);

	if (NewSocket != INVALID_SOCKET)
	{
		// we need the subclass to create the actual FSocket object
		check(SocketSubsystem);
		FSocketSubsystemBSD* BSDSystem = static_cast<FSocketSubsystemBSD*>(SocketSubsystem);
		return BSDSystem->InternalBSDSocketFactory(NewSocket, SocketType, InSocketDescription, BSDAddr.GetProtocolFamily());
	}

	return NULL;
}


bool FSocketBSD::SendTo(const uint8* Data, int32 Count, int32& BytesSent, const FInternetAddr& Destination)
{
	const FInternetAddrBSD& BSDAddr = static_cast<const FInternetAddrBSD&>(Destination);
	// Write the data and see how much was written
	BytesSent = sendto(Socket, (const char*)Data, Count, 0, (const sockaddr*)&(BSDAddr.Addr), BSDAddr.GetStorageSize());

//	NETWORK_PROFILER(FSocket::SendTo(Data,Count,BytesSent,Destination));

	bool Result = BytesSent >= 0;
	if (Result)
	{
		LastActivityTime = FPlatformTime::Seconds();
	}
	return Result;
}


bool FSocketBSD::Send(const uint8* Data, int32 Count, int32& BytesSent)
{
	BytesSent = send(Socket,(const char*)Data,Count,0);

//	NETWORK_PROFILER(FSocket::Send(Data,Count,BytesSent));

	bool Result = BytesSent >= 0;
	if (Result)
	{
		LastActivityTime = FPlatformTime::Seconds();
	}
	return Result;
}


bool FSocketBSD::RecvFrom(uint8* Data, int32 BufferSize, int32& BytesRead, FInternetAddr& Source, ESocketReceiveFlags::Type Flags)
{
	bool bSuccess = false;
	const bool bStreamSocket = (SocketType == SOCKTYPE_Streaming);
	const int TranslatedFlags = TranslateFlags(Flags);
	FInternetAddrBSD& BSDAddr = static_cast<FInternetAddrBSD&>(Source);
	SOCKLEN Size = sizeof(sockaddr_storage);
	sockaddr* Addr = (sockaddr*)&(BSDAddr.Addr);

	// Read into the buffer and set the source address
	BytesRead = recvfrom(Socket, (char*)Data, BufferSize, TranslatedFlags, Addr, &Size);
//	NETWORK_PROFILER(FSocket::RecvFrom(Data,BufferSize,BytesRead,Source));

	if (BytesRead >= 0)
	{
		// For Streaming sockets, 0 indicates a graceful failure
		bSuccess = !bStreamSocket || (BytesRead > 0);
	}
	else
	{
		// For Streaming sockets, don't treat SE_EWOULDBLOCK as an error
		bSuccess = bStreamSocket && (SocketSubsystem->TranslateErrorCode(BytesRead) == SE_EWOULDBLOCK);
		BytesRead = 0;
	}

	if (bSuccess)
	{
		LastActivityTime = FPlatformTime::Seconds();
	}

	return bSuccess;
}


bool FSocketBSD::Recv(uint8* Data, int32 BufferSize, int32& BytesRead, ESocketReceiveFlags::Type Flags)
{
	bool bSuccess = false;
	const bool bStreamSocket = (SocketType == SOCKTYPE_Streaming);
	const int TranslatedFlags = TranslateFlags(Flags);
	BytesRead = recv(Socket, (char*)Data, BufferSize, TranslatedFlags);

//	NETWORK_PROFILER(FSocket::Recv(Data,BufferSize,BytesRead));

	if (BytesRead >= 0)
	{
		// For Streaming sockets, 0 indicates a graceful failure
		bSuccess = !bStreamSocket || (BytesRead > 0);
	}
	else
	{
		// For Streaming sockets, don't treat SE_EWOULDBLOCK as an error
		bSuccess = bStreamSocket && (SocketSubsystem->TranslateErrorCode(BytesRead) == SE_EWOULDBLOCK);
		BytesRead = 0;
	}

	if (bSuccess)
	{
		LastActivityTime = FPlatformTime::Seconds();
	}

	return bSuccess;
}


bool FSocketBSD::Wait(ESocketWaitConditions::Type Condition, FTimespan WaitTime)
{
	if ((Condition == ESocketWaitConditions::WaitForRead) || (Condition == ESocketWaitConditions::WaitForReadOrWrite))
	{
		if (HasState(ESocketBSDParam::CanRead, WaitTime) == ESocketBSDReturn::Yes)
		{
			return true;
		}
	}

	if ((Condition == ESocketWaitConditions::WaitForWrite) || (Condition == ESocketWaitConditions::WaitForReadOrWrite))
	{
		if (HasState(ESocketBSDParam::CanWrite, WaitTime) == ESocketBSDReturn::Yes)
		{
			return true;
		}
	}

	return false;
}


ESocketConnectionState FSocketBSD::GetConnectionState(void)
{
	ESocketConnectionState CurrentState = SCS_ConnectionError;

	// look for an existing error
	if (HasState(ESocketBSDParam::HasError) == ESocketBSDReturn::No)
	{
		if (FPlatformTime::Seconds() - LastActivityTime > 5.0)
		{
			// get the write state
			ESocketBSDReturn WriteState = HasState(ESocketBSDParam::CanWrite, FTimespan::FromMilliseconds(1));
			ESocketBSDReturn ReadState = HasState(ESocketBSDParam::CanRead, FTimespan::FromMilliseconds(1));
		
			// translate yes or no (error is already set)
			if (WriteState == ESocketBSDReturn::Yes || ReadState == ESocketBSDReturn::Yes)
			{
				CurrentState = SCS_Connected;
				LastActivityTime = FPlatformTime::Seconds();
			}
			else if (WriteState == ESocketBSDReturn::No && ReadState == ESocketBSDReturn::No)
			{
				CurrentState = SCS_NotConnected;
			}
		}
		else
		{
			CurrentState = SCS_Connected;
		}
	}

	return CurrentState;
}


void FSocketBSD::GetAddress(FInternetAddr& OutAddr)
{
	FInternetAddrBSD& BSDAddr = static_cast<FInternetAddrBSD&>(OutAddr);
	SOCKLEN Size = sizeof(sockaddr_storage);

	// Figure out what ip/port we are bound to
	bool bOk = getsockname(Socket, (sockaddr*)&BSDAddr.Addr, &Size) == 0;

	if (bOk == false)
	{
		check(SocketSubsystem);
		UE_LOG(LogSockets, Error, TEXT("Failed to read address for socket (%s)"), SocketSubsystem->GetSocketError());
	}
}


bool FSocketBSD::GetPeerAddress(FInternetAddr& OutAddr)
{
	FInternetAddrBSD& BSDAddr = static_cast<FInternetAddrBSD&>(OutAddr);
	SOCKLEN Size = sizeof(sockaddr_storage);

	// Figure out what ip/port we are bound to
	int Result = getpeername(Socket, (sockaddr*)&BSDAddr.Addr, &Size);

	if (Result != 0)
	{
		check(SocketSubsystem);
		UE_LOG(LogSockets, Warning, TEXT("Failed to read address for socket (%s) with error %d"), SocketSubsystem->GetSocketError(), Result);
	}
	return Result == 0;
}

bool FSocketBSD::SetNonBlocking(bool bIsNonBlocking)
{
	u_long Value = bIsNonBlocking ? true : false;

#if PLATFORM_HTML5 
	// can't have blocking sockets.
	ensureMsgf(bIsNonBlocking, TEXT("Can't have blocking sockets on HTML5"));
    return true;
#else 

#if PLATFORM_HAS_BSD_SOCKET_FEATURE_WINSOCKETS
	return ioctlsocket(Socket,FIONBIO,&Value) == 0;
#else 
	int Flags = fcntl(Socket, F_GETFL, 0);
	//Set the flag or clear it, without destroying the other flags.
	Flags = bIsNonBlocking ? Flags | O_NONBLOCK : Flags ^ (Flags & O_NONBLOCK);
	int err = fcntl(Socket, F_SETFL, Flags);
	return (err == 0 ? true : false);
#endif
#endif 
}


bool FSocketBSD::SetBroadcast(bool bAllowBroadcast)
{
	int Param = bAllowBroadcast ? 1 : 0;
	return setsockopt(Socket,SOL_SOCKET,SO_BROADCAST,(char*)&Param,sizeof(Param)) == 0;
}


bool FSocketBSD::JoinMulticastGroup(const FInternetAddr& GroupAddress)
{
	const FInternetAddrBSD& BSDAddr = static_cast<const FInternetAddrBSD&>(GroupAddress);

#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	if (BSDAddr.GetProtocolFamily() == ESocketProtocolFamily::IPv6)
	{
		ipv6_mreq imr;
		imr.ipv6mr_interface = htonl(BSDAddr.GetScopeId());
		imr.ipv6mr_multiaddr = ((sockaddr_in6*)&(BSDAddr.Addr))->sin6_addr;
		return (setsockopt(Socket, IPPROTO_IPV6, IPV6_JOIN_GROUP, (char*)&imr, sizeof(imr)) == 0);
	}
#endif

	ip_mreq imr;
	imr.imr_interface.s_addr = INADDR_ANY;
	imr.imr_multiaddr = ((sockaddr_in*)&(BSDAddr.Addr))->sin_addr;
	return (setsockopt(Socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
}


bool FSocketBSD::JoinMulticastGroup(const FInternetAddr& GroupAddress, const FInternetAddr& InterfaceAddress)
{
	const FInternetAddrBSD& BSDAddr = static_cast<const FInternetAddrBSD&>(GroupAddress);
	const FInternetAddrBSD& BSDIFAddr = static_cast<const FInternetAddrBSD&>(InterfaceAddress);

#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	if (BSDAddr.GetProtocolFamily() == ESocketProtocolFamily::IPv6)
	{
		ipv6_mreq imr;
		imr.ipv6mr_interface = htonl(BSDIFAddr.GetScopeId());
		imr.ipv6mr_multiaddr = ((sockaddr_in6*)&(BSDAddr.Addr))->sin6_addr;
		return (setsockopt(Socket, IPPROTO_IPV6, IP_ADD_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
	}
#endif

	ip_mreq imr;
	imr.imr_interface.s_addr = ((sockaddr_in*)&(BSDIFAddr.Addr))->sin_addr.s_addr;
	imr.imr_multiaddr = ((sockaddr_in*)&(BSDAddr.Addr))->sin_addr;
	return (setsockopt(Socket, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
}


bool FSocketBSD::LeaveMulticastGroup(const FInternetAddr& GroupAddress)
{
	const FInternetAddrBSD& BSDAddr = static_cast<const FInternetAddrBSD&>(GroupAddress);

#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	if (BSDAddr.GetProtocolFamily() == ESocketProtocolFamily::IPv6)
	{
		ipv6_mreq imr;
		imr.ipv6mr_interface = htonl(BSDAddr.GetScopeId());
		imr.ipv6mr_multiaddr = ((sockaddr_in6*)&(BSDAddr.Addr))->sin6_addr;
		return (setsockopt(Socket, IPPROTO_IPV6, IPV6_LEAVE_GROUP, (char*)&imr, sizeof(imr)) == 0);
	}
#endif

	ip_mreq imr;
	imr.imr_interface.s_addr = INADDR_ANY;
	imr.imr_multiaddr = ((sockaddr_in*)&(BSDAddr.Addr))->sin_addr;
	return (setsockopt(Socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
}


bool FSocketBSD::LeaveMulticastGroup(const FInternetAddr& GroupAddress, const FInternetAddr& InterfaceAddress)
{
	const FInternetAddrBSD& BSDAddr = static_cast<const FInternetAddrBSD&>(GroupAddress);
	const FInternetAddrBSD& BSDIFAddr = static_cast<const FInternetAddrBSD&>(InterfaceAddress);

#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	if (BSDAddr.GetProtocolFamily() == ESocketProtocolFamily::IPv6)
	{
		ipv6_mreq imr;
		imr.ipv6mr_interface = htonl(BSDIFAddr.GetScopeId());
		imr.ipv6mr_multiaddr = ((sockaddr_in6*)&(BSDAddr.Addr))->sin6_addr;
		return (setsockopt(Socket, IPPROTO_IPV6, IP_DROP_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
	}
#endif

	ip_mreq imr;
	imr.imr_interface.s_addr = ((sockaddr_in*)&(BSDIFAddr.Addr))->sin_addr.s_addr;
	imr.imr_multiaddr = ((sockaddr_in*)&(BSDAddr.Addr))->sin_addr;
	return (setsockopt(Socket, IPPROTO_IP, IP_DROP_MEMBERSHIP, (char*)&imr, sizeof(imr)) == 0);
}


bool FSocketBSD::SetMulticastLoopback(bool bLoopback)
{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	if (SocketProtocol == ESocketProtocolFamily::IPv6)
	{
		uint32 ShouldLoopback = bLoopback ? 1 : 0;
		return (setsockopt(Socket, IPPROTO_IPV6, IP_MULTICAST_LOOP, (char*)&ShouldLoopback, sizeof(ShouldLoopback)) == 0);
	}
#endif

	return (setsockopt(Socket, IPPROTO_IP, IP_MULTICAST_LOOP, (char*)&bLoopback, sizeof(bLoopback)) == 0);
}


bool FSocketBSD::SetMulticastTtl(uint8 TimeToLive)
{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	if (SocketProtocol == ESocketProtocolFamily::IPv6)
	{
		uint32 RealTimeToLive = TimeToLive;
		return (setsockopt(Socket, IPPROTO_IPV6, IPV6_MULTICAST_HOPS, (char*)&RealTimeToLive, sizeof(RealTimeToLive)) == 0);
	}
#endif

	return (setsockopt(Socket, IPPROTO_IP, IP_MULTICAST_TTL, (char*)&TimeToLive, sizeof(TimeToLive)) == 0);
}


bool FSocketBSD::SetMulticastInterface(const FInternetAddr& InterfaceAddress)
{
	const FInternetAddrBSD& BSDIFAddr = static_cast<const FInternetAddrBSD&>(InterfaceAddress);

#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	if (BSDIFAddr.GetProtocolFamily() == ESocketProtocolFamily::IPv6)
	{
		uint32 InterfaceIndex = htonl(BSDIFAddr.GetScopeId());
		return (setsockopt(Socket, IPPROTO_IPV6, IPV6_MULTICAST_IF, (char*)&InterfaceIndex, sizeof(InterfaceIndex)) == 0);
	}
#endif

	in_addr InterfaceAddr = ((sockaddr_in*)&(BSDIFAddr.Addr))->sin_addr;
	return (setsockopt(Socket, IPPROTO_IP, IP_MULTICAST_IF, (char*)&InterfaceAddr, sizeof(InterfaceAddr)) == 0);
}


bool FSocketBSD::SetReuseAddr(bool bAllowReuse)
{
	int Param = bAllowReuse ? 1 : 0;
	int ReuseAddrResult = setsockopt(Socket, SOL_SOCKET, SO_REUSEADDR, (char*)&Param, sizeof(Param));
#ifdef SO_REUSEPORT // Linux kernel 3.9+ and FreeBSD define this separately
	if (ReuseAddrResult == 0)
	{
		return setsockopt(Socket, SOL_SOCKET, SO_REUSEPORT, (char *)&Param, sizeof(Param)) == 0;
	}
#endif
	return ReuseAddrResult == 0;
}


bool FSocketBSD::SetLinger(bool bShouldLinger,int32 Timeout)
{
	linger ling;

	ling.l_onoff = bShouldLinger;
	ling.l_linger = Timeout;

	return setsockopt(Socket,SOL_SOCKET,SO_LINGER,(char*)&ling,sizeof(ling)) == 0;
}


bool FSocketBSD::SetRecvErr(bool bUseErrorQueue)
{
	// Not supported, but return true to avoid spurious log messages
	return true;
}


bool FSocketBSD::SetSendBufferSize(int32 Size,int32& NewSize)
{
	SOCKLEN SizeSize = sizeof(int32);
	bool bOk = setsockopt(Socket,SOL_SOCKET,SO_SNDBUF,(char*)&Size,sizeof(int32)) == 0;

	// Read the value back in case the size was modified
	getsockopt(Socket,SOL_SOCKET,SO_SNDBUF,(char*)&NewSize, &SizeSize);

	return bOk;
}


bool FSocketBSD::SetReceiveBufferSize(int32 Size,int32& NewSize)
{
	SOCKLEN SizeSize = sizeof(int32);
	bool bOk = setsockopt(Socket,SOL_SOCKET,SO_RCVBUF,(char*)&Size,sizeof(int32)) == 0;

	// Read the value back in case the size was modified
	getsockopt(Socket,SOL_SOCKET,SO_RCVBUF,(char*)&NewSize, &SizeSize);

	return bOk;
}


int32 FSocketBSD::GetPortNo(void)
{
	sockaddr_storage Addr;
	SOCKLEN Size = sizeof(sockaddr_storage);

	// Figure out what ip/port we are bound to
	bool bOk = getsockname(Socket, (sockaddr*)&Addr, &Size) == 0;
	
	if (bOk == false)
	{
		check(SocketSubsystem);
		UE_LOG(LogSockets, Error, TEXT("Failed to read address for socket (%s)"), SocketSubsystem->GetSocketError());
		
		return 0;
	}

#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	if (Addr.ss_family == AF_INET6)
	{
		return ntohs(((sockaddr_in6&)Addr).sin6_port);
	}
#endif

	// Convert big endian port to native endian port.
	return ntohs(((sockaddr_in&)Addr).sin_port);
}

bool FSocketBSD::SetIPv6Only(bool bIPv6Only)
{
#if PLATFORM_HAS_BSD_IPV6_SOCKETS
	int v6only = bIPv6Only ? 1 : 0;
	bool bOk = (setsockopt(Socket, IPPROTO_IPV6, IPV6_V6ONLY, (char*)&v6only, sizeof(v6only)) == 0);

	if (bOk == false)
	{
		check(SocketSubsystem);
		UE_LOG(LogSockets, Error, TEXT("Failed to set sock opt for socket (%s)"), SocketSubsystem->GetSocketError());
	}

	return bOk;
#else 
	return false;
#endif
}


/* FSocketBSD implementation
*****************************************************************************/

ESocketBSDReturn FSocketBSD::HasState(ESocketBSDParam State, FTimespan WaitTime)
{
#if PLATFORM_HAS_BSD_SOCKET_FEATURE_SELECT
	// convert WaitTime to a timeval
	timeval Time;
	Time.tv_sec = (int32)WaitTime.GetTotalSeconds();
	Time.tv_usec = WaitTime.GetFractionMicro();

	fd_set SocketSet;

	// Set up the socket sets we are interested in (just this one)
	FD_ZERO(&SocketSet);
	FD_SET(Socket, &SocketSet);

	timeval* TimePointer = WaitTime.GetTicks() >= 0 ? &Time : nullptr;

	// Check the status of the state
	int32 SelectStatus = 0;
	switch (State)
	{
	case ESocketBSDParam::CanRead:
		SelectStatus = select(Socket + 1, &SocketSet, NULL, NULL, TimePointer);
		break;

	case ESocketBSDParam::CanWrite:
		SelectStatus = select(Socket + 1, NULL, &SocketSet, NULL, TimePointer);
		break;

	case ESocketBSDParam::HasError:
		SelectStatus = select(Socket + 1, NULL, NULL, &SocketSet, TimePointer);
		break;
	}

	// if the select returns a positive number, the socket had the state, 0 means didn't have it, and negative is API error condition (not socket's error state)
	return SelectStatus > 0 ? ESocketBSDReturn::Yes :
		SelectStatus == 0 ? ESocketBSDReturn::No :
		ESocketBSDReturn::EncounteredError;
#else
	UE_LOG(LogSockets, Fatal, TEXT("This platform doesn't support select(), but FSocketBSD::HasState was not overridden"));
	return ESocketBSDReturn::EncounteredError;
#endif
}


#endif	//PLATFORM_HAS_BSD_SOCKETS
