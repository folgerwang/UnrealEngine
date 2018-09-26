// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IpConnection.cpp: Unreal IP network connection.
Notes:
	* See \msdev\vc98\include\winsock.h and \msdev\vc98\include\winsock2.h 
	  for Winsock WSAE* errors returned by Windows Sockets.
=============================================================================*/

#include "IpConnection.h"
#include "SocketSubsystem.h"
#include "Engine/Engine.h"

#include "IPAddress.h"
#include "Sockets.h"
#include "Net/NetworkProfiler.h"
#include "Net/DataChannel.h"

#include "PacketAudit.h"

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// Size of a UDP header.
#define IP_HEADER_SIZE     (20)
#define UDP_HEADER_SIZE    (IP_HEADER_SIZE+8)

DECLARE_CYCLE_STAT(TEXT("IpConnection InitRemoteConnection"), Stat_IpConnectionInitRemoteConnection, STATGROUP_Net);
DECLARE_CYCLE_STAT(TEXT("IpConnection Socket SendTo"), STAT_IpConnection_SendToSocket, STATGROUP_Net);
DECLARE_CYCLE_STAT(TEXT("IpConnection WaitForSendTasks"), STAT_IpConnection_WaitForSendTasks, STATGROUP_Net);

TAutoConsoleVariable<int32> CVarNetIpConnectionUseSendTasks(
	TEXT("net.IpConnectionUseSendTasks"),
	0,
	TEXT("If true, the IpConnection will call the socket's SendTo function in a task graph task so that it can run off the game thread."));

UIpConnection::UIpConnection(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer),
	RemoteAddr(NULL),
	Socket(NULL),
	ResolveInfo(NULL)
{
}

void UIpConnection::InitBase(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	// Pass the call up the chain
	Super::InitBase(InDriver, InSocket, InURL, InState, 
		// Use the default packet size/overhead unless overridden by a child class
		(InMaxPacket == 0 || InMaxPacket > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : InMaxPacket,
		InPacketOverhead == 0 ? UDP_HEADER_SIZE : InPacketOverhead);

	Socket = InSocket;
	ResolveInfo = NULL;
}

void UIpConnection::InitLocalConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	InitBase(InDriver, InSocket, InURL, InState, 
		// Use the default packet size/overhead unless overridden by a child class
		(InMaxPacket == 0 || InMaxPacket > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : InMaxPacket,
		InPacketOverhead == 0 ? UDP_HEADER_SIZE : InPacketOverhead);

	// Figure out IP address from the host URL
	bool bIsValid = false;
	// Get numerical address directly.
	RemoteAddr = InDriver->GetSocketSubsystem()->CreateInternetAddr();
	RemoteAddr->SetIp(*InURL.Host, bIsValid);
	RemoteAddr->SetPort(InURL.Port);

	// Try to resolve it if it failed
	if (bIsValid == false)
	{
		// Create thread to resolve the address.
		ResolveInfo = InDriver->GetSocketSubsystem()->GetHostByName(TCHAR_TO_ANSI(*InURL.Host));
		if (ResolveInfo == NULL)
		{
			Close();
			UE_LOG(LogNet, Verbose, TEXT("IpConnection::InitConnection: Unable to resolve %s"), *InURL.Host);
		}
	}

	// Initialize our send bunch
	InitSendBuffer();
}

void UIpConnection::InitRemoteConnection(UNetDriver* InDriver, class FSocket* InSocket, const FURL& InURL, const class FInternetAddr& InRemoteAddr, EConnectionState InState, int32 InMaxPacket, int32 InPacketOverhead)
{
	SCOPE_CYCLE_COUNTER(Stat_IpConnectionInitRemoteConnection);

	InitBase(InDriver, InSocket, InURL, InState, 
		// Use the default packet size/overhead unless overridden by a child class
		(InMaxPacket == 0 || InMaxPacket > MAX_PACKET_SIZE) ? MAX_PACKET_SIZE : InMaxPacket,
		InPacketOverhead == 0 ? UDP_HEADER_SIZE : InPacketOverhead);

	// Copy the remote IPAddress passed in
	bool bIsValid = false;
	FString IpAddrStr = InRemoteAddr.ToString(false);
	RemoteAddr = InDriver->GetSocketSubsystem()->CreateInternetAddr();
	RemoteAddr->SetIp(*IpAddrStr, bIsValid);
	RemoteAddr->SetPort(InRemoteAddr.GetPort());

	URL.Host = RemoteAddr->ToString(false);

	// Initialize our send bunch
	InitSendBuffer();

	// This is for a client that needs to log in, setup ClientLoginState and ExpectedClientLoginMsgType to reflect that
	SetClientLoginState( EClientLoginState::LoggingIn );
	SetExpectedClientLoginMsgType( NMT_Hello );
}

void UIpConnection::Tick()
{
	if (CVarNetIpConnectionUseSendTasks.GetValueOnGameThread() != 0)
	{
		ISocketSubsystem* const SocketSubsystem = Driver->GetSocketSubsystem();

		FScopeLock ScopeLock(&SocketSendResultsCriticalSection);
		
		for (const FSocketSendResult& Result : SocketSendResults)
		{
			HandleSocketSendResult(Result, SocketSubsystem);
		}

		SocketSendResults.Reset();
	}

	Super::Tick();
}

void UIpConnection::CleanUp()
{
	Super::CleanUp();

	WaitForSendTasks();
}

void UIpConnection::WaitForSendTasks()
{
	if (CVarNetIpConnectionUseSendTasks.GetValueOnGameThread() != 0 && LastSendTask.IsValid())
	{
		check(IsInGameThread());

		SCOPE_CYCLE_COUNTER(STAT_IpConnection_WaitForSendTasks);
		FTaskGraphInterface::Get().WaitUntilTaskCompletes(LastSendTask, ENamedThreads::GameThread);
	}
}

void UIpConnection::LowLevelSend(void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	const uint8* DataToSend = reinterpret_cast<uint8*>(Data);

	if( ResolveInfo )
	{
		// If destination address isn't resolved yet, send nowhere.
		if( !ResolveInfo->IsComplete() )
		{
			// Host name still resolving.
			return;
		}
		else if( ResolveInfo->GetErrorCode() != SE_NO_ERROR )
		{
			// Host name resolution just now failed.
			UE_LOG(LogNet, Log,  TEXT("Host name resolution failed with %d"), ResolveInfo->GetErrorCode() );
			Driver->ServerConnection->State = USOCK_Closed;
			delete ResolveInfo;
			ResolveInfo = NULL;
			return;
		}
		else
		{
			// Host name resolution just now succeeded.
			int32 CurPort = RemoteAddr->GetPort();
			RemoteAddr = ResolveInfo->GetResolvedAddress().Clone();
			RemoteAddr->SetPort(CurPort);

			UE_LOG(LogNet, Log, TEXT("Host name resolution completed"));
			delete ResolveInfo;
			ResolveInfo = NULL;
		}
	}


	// Process any packet modifiers
	if (Handler.IsValid() && !Handler->GetRawSend())
	{
		const ProcessedPacket ProcessedData = Handler->Outgoing(reinterpret_cast<uint8*>(Data), CountBits, Traits);

		if (!ProcessedData.bError)
		{
			DataToSend = ProcessedData.Data;
			CountBits = ProcessedData.CountBits;
		}
		else
		{
			CountBits = 0;
		}
	}

	bool bBlockSend = false;
	int32 CountBytes = FMath::DivideAndRoundUp(CountBits, 8);

#if !UE_BUILD_SHIPPING
	LowLevelSendDel.ExecuteIfBound((void*)DataToSend, CountBytes, bBlockSend);
#endif

	if (!bBlockSend)
	{
		// Send to remote.
		FSocketSendResult SendResult;
		CLOCK_CYCLES(Driver->SendCycles);

		if ( CountBytes > MaxPacket )
		{
			UE_LOG( LogNet, Warning, TEXT( "UIpConnection::LowLevelSend: CountBytes > MaxPacketSize! Count: %i, MaxPacket: %i %s" ), CountBytes, MaxPacket, *Describe() );
		}

		FPacketAudit::NotifyLowLevelSend((uint8*)DataToSend, CountBytes, CountBits);

		if (CountBytes > 0)
		{
			if (CVarNetIpConnectionUseSendTasks.GetValueOnAnyThread() != 0)
			{
				DECLARE_CYCLE_STAT(TEXT("IpConnection SendTo task"), STAT_IpConnection_SendToTask, STATGROUP_TaskGraphTasks);

				FGraphEventArray Prerequisites;
				if (LastSendTask.IsValid())
				{
					Prerequisites.Add(LastSendTask);
				}

				ISocketSubsystem* const SocketSubsystem = Driver->GetSocketSubsystem();
				LastSendTask = FFunctionGraphTask::CreateAndDispatchWhenReady([this, Packet = TArray<uint8>(DataToSend, CountBytes), SocketSubsystem]
				{
					bool bWasSendSuccessful = false;
					UIpConnection::FSocketSendResult Result;

					{
						SCOPE_CYCLE_COUNTER(STAT_IpConnection_SendToSocket);
						bWasSendSuccessful = Socket->SendTo(Packet.GetData(), Packet.Num(), Result.BytesSent, *RemoteAddr);
					}

					if (!bWasSendSuccessful && SocketSubsystem)
					{
						Result.Error = SocketSubsystem->GetLastErrorCode();
						if (Result.Error != SE_EWOULDBLOCK &&
							Result.Error != SE_NO_ERROR)
						{
							FScopeLock ScopeLock(&SocketSendResultsCriticalSection);
							SocketSendResults.Add(MoveTemp(Result));
						}
					}
				},
				GET_STATID(STAT_IpConnection_SendToTask), &Prerequisites);

				// Always flush this profiler data now. Technically this could be incorrect if the send in the task fails,
				// but this keeps the bookkeeping simpler for now.
				NETWORK_PROFILER(GNetworkProfiler.FlushOutgoingBunches(this));
				NETWORK_PROFILER(GNetworkProfiler.TrackSocketSendTo(Socket->GetDescription(), DataToSend, CountBytes, NumPacketIdBits, NumBunchBits, NumAckBits, NumPaddingBits, this));
			}
			else
			{
				bool bWasSendSuccessful = false;
				{
					SCOPE_CYCLE_COUNTER(STAT_IpConnection_SendToSocket);
					bWasSendSuccessful = Socket->SendTo(DataToSend, CountBytes, SendResult.BytesSent, *RemoteAddr);
				}
				if (bWasSendSuccessful)
				{
					UNCLOCK_CYCLES(Driver->SendCycles);
					NETWORK_PROFILER(GNetworkProfiler.FlushOutgoingBunches(this));
					NETWORK_PROFILER(GNetworkProfiler.TrackSocketSendTo(Socket->GetDescription(), DataToSend, SendResult.BytesSent, NumPacketIdBits, NumBunchBits, NumAckBits, NumPaddingBits, this));
				}
				else
				{
					ISocketSubsystem* const SocketSubsystem = Driver->GetSocketSubsystem();
					SendResult.Error = SocketSubsystem->GetLastErrorCode();

					HandleSocketSendResult(SendResult, SocketSubsystem);
				}
			}
		}
	}
}

void UIpConnection::HandleSocketSendResult(const FSocketSendResult& Result, ISocketSubsystem* SocketSubsystem)
{
	if (Result.Error != SE_EWOULDBLOCK &&
		Result.Error != SE_NO_ERROR)
	{
		FString ErrorString = FString::Printf(TEXT("UIpNetConnection::LowLevelSend: Socket->SendTo failed with error %i (%s). %s"),
			static_cast<int32>(Result.Error),
			SocketSubsystem->GetSocketError(Result.Error),
			*Describe());

		GEngine->BroadcastNetworkFailure(Driver->GetWorld(), Driver, ENetworkFailure::ConnectionLost, ErrorString);

		// Reset the send buffer before closing, as it could have been (almost) full and the close process may
		// write a bunch that could cause an overflow.  We're closing the connection anyway, and given that
		// the socket is returning errors, the close bunch probably won't be delivered either.
		InitSendBuffer();
		Close();
	}
}

FString UIpConnection::LowLevelGetRemoteAddress(bool bAppendPort)
{
	return RemoteAddr->ToString(bAppendPort);
}

FString UIpConnection::LowLevelDescribe()
{
	TSharedRef<FInternetAddr> LocalAddr = Driver->GetSocketSubsystem()->CreateInternetAddr();

	if (Socket != nullptr)
	{
		Socket->GetAddress(*LocalAddr);
	}

	return FString::Printf
	(
		TEXT("url=%s remote=%s local=%s uniqueid=%s state: %s"),
		*URL.Host,
		(RemoteAddr.IsValid() ? *RemoteAddr->ToString(true) : TEXT("nullptr")),
		*LocalAddr->ToString(true),
		(PlayerId.IsValid() ? *PlayerId->ToDebugString() : TEXT("nullptr")),
			State==USOCK_Pending	?	TEXT("Pending")
		:	State==USOCK_Open		?	TEXT("Open")
		:	State==USOCK_Closed		?	TEXT("Closed")
		:								TEXT("Invalid")
	);
}

int32 UIpConnection::GetAddrAsInt(void)
{
	uint32 OutAddr = 0;
	// Get the host byte order ip addr
	RemoteAddr->GetIp(OutAddr);
	return (int32)OutAddr;
}

int32 UIpConnection::GetAddrPort(void)
{
	return RemoteAddr->GetPort();
}

TSharedPtr<FInternetAddr> UIpConnection::GetInternetAddr()
{
	return RemoteAddr;
}

FString UIpConnection::RemoteAddressToString()
{
	return RemoteAddr->ToString(true);
}
