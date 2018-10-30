// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	IpNetDriver.cpp: Unreal IP network driver.
Notes:
	* See \msdev\vc98\include\winsock.h and \msdev\vc98\include\winsock2.h 
	  for Winsock WSAE* errors returned by Windows Sockets.
=============================================================================*/

#include "IpNetDriver.h"
#include "Misc/CommandLine.h"
#include "EngineGlobals.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "UObject/Package.h"
#include "PacketHandlers/StatelessConnectHandlerComponent.h"
#include "Engine/NetConnection.h"
#include "Engine/ChildConnection.h"
#include "SocketSubsystem.h"
#include "IpConnection.h"
#include "HAL/LowLevelMemTracker.h"

#include "PacketAudit.h"

#include "IPAddress.h"
#include "Sockets.h"

/** For backwards compatibility with the engine stateless connect code */
#ifndef STATELESSCONNECT_HAS_RANDOM_SEQUENCE
	#define STATELESSCONNECT_HAS_RANDOM_SEQUENCE 0
#endif

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

DECLARE_CYCLE_STAT(TEXT("IpNetDriver Add new connection"), Stat_IpNetDriverAddNewConnection, STATGROUP_Net);
DECLARE_CYCLE_STAT(TEXT("IpNetDriver Socket RecvFrom"), STAT_IpNetDriver_RecvFromSocket, STATGROUP_Net);
DECLARE_CYCLE_STAT(TEXT("IpNetDriver Destroy WaitForReceiveThread"), STAT_IpNetDriver_Destroy_WaitForReceiveThread, STATGROUP_Net);

UIpNetDriver::FOnNetworkProcessingCausingSlowFrame UIpNetDriver::OnNetworkProcessingCausingSlowFrame;

// Time before the alarm delegate is called (in seconds)
float GIpNetDriverMaxDesiredTimeSliceBeforeAlarmSecs = 1.0f;

FAutoConsoleVariableRef GIpNetDriverMaxDesiredTimeSliceBeforeAlarmSecsCVar(
	TEXT("n.IpNetDriverMaxFrameTimeBeforeAlert"),
	GIpNetDriverMaxDesiredTimeSliceBeforeAlarmSecs,
	TEXT("Time to spend processing networking data in a single frame before an alert is raised (in seconds)\n")
	TEXT("It may get called multiple times in a single frame if additional processing after a previous alert exceeds the threshold again\n")
	TEXT(" default: 1 s"));

// Time before the time taken in a single frame is printed out (in seconds)
float GIpNetDriverLongFramePrintoutThresholdSecs = 10.0f;

FAutoConsoleVariableRef GIpNetDriverLongFramePrintoutThresholdSecsCVar(
	TEXT("n.IpNetDriverMaxFrameTimeBeforeLogging"),
	GIpNetDriverLongFramePrintoutThresholdSecs,
	TEXT("Time to spend processing networking data in a single frame before an output log warning is printed (in seconds)\n")
	TEXT(" default: 10 s"));

TAutoConsoleVariable<int32> CVarNetIpNetDriverUseReceiveThread(
	TEXT("net.IpNetDriverUseReceiveThread"),
	0,
	TEXT("If true, the IpNetDriver will call the socket's RecvFrom function on a separate thread (not the game thread)"));

TAutoConsoleVariable<int32> CVarNetIpNetDriverReceiveThreadQueueMaxPackets(
	TEXT("net.IpNetDriverReceiveThreadQueueMaxPackets"),
	1024,
	TEXT("If net.IpNetDriverUseReceiveThread is true, the maximum number of packets that can be waiting in the queue. Additional packets received will be dropped."));

TAutoConsoleVariable<int32> CVarNetIpNetDriverReceiveThreadPollTimeMS(
	TEXT("net.IpNetDriverReceiveThreadPollTimeMS"),
	250,
	TEXT("If net.IpNetDriverUseReceiveThread is true, the number of milliseconds to use as the timeout value for FSocket::Wait on the receive thread. A negative value means to wait indefinitely (FSocket::Shutdown should cancel it though)."));

UIpNetDriver::UIpNetDriver(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ServerDesiredSocketReceiveBufferBytes(0x20000)
	, ServerDesiredSocketSendBufferBytes(0x20000)
	, ClientDesiredSocketReceiveBufferBytes(0x8000)
	, ClientDesiredSocketSendBufferBytes(0x8000)
{
}

bool UIpNetDriver::IsAvailable() const
{
	// IP driver always valid for now
	return true;
}

ISocketSubsystem* UIpNetDriver::GetSocketSubsystem()
{
	return ISocketSubsystem::Get();
}

FSocket * UIpNetDriver::CreateSocket()
{
	// Create UDP socket and enable broadcasting.
	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();

	if (SocketSubsystem == NULL)
	{
		UE_LOG(LogNet, Warning, TEXT("UIpNetDriver::CreateSocket: Unable to find socket subsystem"));
		return NULL;
	}

	return SocketSubsystem->CreateSocket( NAME_DGram, TEXT( "Unreal" ) );
}

int UIpNetDriver::GetClientPort()
{
	return 0;
}

bool UIpNetDriver::InitBase( bool bInitAsClient, FNetworkNotify* InNotify, const FURL& URL, bool bReuseAddressAndPort, FString& Error )
{
	if (!Super::InitBase(bInitAsClient, InNotify, URL, bReuseAddressAndPort, Error))
	{
		return false;
	}

	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
	if (SocketSubsystem == NULL)
	{
		UE_LOG(LogNet, Warning, TEXT("Unable to find socket subsystem"));
		return false;
	}

	// Derived types may have already allocated a socket

	// Create the socket that we will use to communicate with
	Socket = CreateSocket();

	if( Socket == NULL )
	{
		Socket = 0;
		Error = FString::Printf( TEXT("%s: socket failed (%i)"), SocketSubsystem->GetSocketAPIName(), (int32)SocketSubsystem->GetLastErrorCode() );
		return false;
	}
	if (SocketSubsystem->RequiresChatDataBeSeparate() == false &&
		Socket->SetBroadcast() == false)
	{
		Error = FString::Printf( TEXT("%s: setsockopt SO_BROADCAST failed (%i)"), SocketSubsystem->GetSocketAPIName(), (int32)SocketSubsystem->GetLastErrorCode() );
		return false;
	}

	if (Socket->SetReuseAddr(bReuseAddressAndPort) == false)
	{
		UE_LOG(LogNet, Log, TEXT("setsockopt with SO_REUSEADDR failed"));
	}

	if (Socket->SetRecvErr() == false)
	{
		UE_LOG(LogNet, Log, TEXT("setsockopt with IP_RECVERR failed"));
	}

	// Increase socket queue size, because we are polling rather than threading
	// and thus we rely on the OS socket to buffer a lot of data.
	int32 RecvSize = bInitAsClient ? ClientDesiredSocketReceiveBufferBytes	: ServerDesiredSocketReceiveBufferBytes;
	int32 SendSize = bInitAsClient ? ClientDesiredSocketSendBufferBytes		: ServerDesiredSocketSendBufferBytes;
	Socket->SetReceiveBufferSize(RecvSize,RecvSize);
	Socket->SetSendBufferSize(SendSize,SendSize);
	UE_LOG(LogInit, Log, TEXT("%s: Socket queue %i / %i"), SocketSubsystem->GetSocketAPIName(), RecvSize, SendSize );

	// Bind socket to our port.
	LocalAddr = SocketSubsystem->GetLocalBindAddr(*GLog);
	
	LocalAddr->SetPort(bInitAsClient ? GetClientPort() : URL.Port);
	
	int32 AttemptPort = LocalAddr->GetPort();
	int32 BoundPort = SocketSubsystem->BindNextPort( Socket, *LocalAddr, MaxPortCountToTry + 1, 1 );
	if (BoundPort == 0)
	{
		Error = FString::Printf( TEXT("%s: binding to port %i failed (%i)"), SocketSubsystem->GetSocketAPIName(), AttemptPort,
			(int32)SocketSubsystem->GetLastErrorCode() );
		return false;
	}
	if( Socket->SetNonBlocking() == false )
	{
		Error = FString::Printf( TEXT("%s: SetNonBlocking failed (%i)"), SocketSubsystem->GetSocketAPIName(),
			(int32)SocketSubsystem->GetLastErrorCode());
		return false;
	}

	// If the cvar is set and the socket subsystem supports it, create the receive thread.
	if (CVarNetIpNetDriverUseReceiveThread.GetValueOnAnyThread() != 0 && SocketSubsystem->IsSocketWaitSupported())
	{
		SocketReceiveThreadRunnable = MakeUnique<FReceiveThreadRunnable>(this);
		SocketReceiveThread.Reset(FRunnableThread::Create(SocketReceiveThreadRunnable.Get(), *FString::Printf(TEXT("IpNetDriver Receive Thread"), *NetDriverName.ToString())));
	}

	// Success.
	return true;
}

bool UIpNetDriver::InitConnect( FNetworkNotify* InNotify, const FURL& ConnectURL, FString& Error )
{
	if( !InitBase( true, InNotify, ConnectURL, false, Error ) )
	{
		UE_LOG(LogNet, Warning, TEXT("Failed to init net driver ConnectURL: %s: %s"), *ConnectURL.ToString(), *Error);
		return false;
	}

	// Create new connection.
	ServerConnection = NewObject<UNetConnection>(GetTransientPackage(), NetConnectionClass);
	ServerConnection->InitLocalConnection( this, Socket, ConnectURL, USOCK_Pending);
	UE_LOG(LogNet, Log, TEXT("Game client on port %i, rate %i"), ConnectURL.Port, ServerConnection->CurrentNetSpeed );

	// Create channel zero.
	GetServerConnection()->CreateChannel( CHTYPE_Control, 1, 0 );

	return true;
}

bool UIpNetDriver::InitListen( FNetworkNotify* InNotify, FURL& LocalURL, bool bReuseAddressAndPort, FString& Error )
{
	if( !InitBase( false, InNotify, LocalURL, bReuseAddressAndPort, Error ) )
	{
		UE_LOG(LogNet, Warning, TEXT("Failed to init net driver ListenURL: %s: %s"), *LocalURL.ToString(), *Error);
		return false;
	}


	InitConnectionlessHandler();

	// Update result URL.
	//LocalURL.Host = LocalAddr->ToString(false);
	LocalURL.Port = LocalAddr->GetPort();
	UE_LOG(LogNet, Log, TEXT("%s IpNetDriver listening on port %i"), *GetDescription(), LocalURL.Port );

	return true;
}

void UIpNetDriver::TickDispatch(float DeltaTime)
{
	LLM_SCOPE(ELLMTag::Networking);

	Super::TickDispatch( DeltaTime );

	// Set the context on the world for this driver's level collection.
	const int32 FoundCollectionIndex = World ? World->GetLevelCollections().IndexOfByPredicate([this](const FLevelCollection& Collection)
	{
		return Collection.GetNetDriver() == this;
	}) : INDEX_NONE;

	FScopedLevelCollectionContextSwitch LCSwitch(FoundCollectionIndex, World);

	ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();

	DDoS.PreFrameReceive(DeltaTime);

	const double StartReceiveTime = FPlatformTime::Seconds();
	double AlarmTime = StartReceiveTime + GIpNetDriverMaxDesiredTimeSliceBeforeAlarmSecs;

	// Process all incoming packets.
	uint8 Data[MAX_PACKET_SIZE];
	uint8* DataRef = Data;
	TSharedRef<FInternetAddr> FromAddr = SocketSubsystem->CreateInternetAddr();

	for (; Socket != nullptr;)
	{
		{
			const double CurrentTime = FPlatformTime::Seconds();
			if (CurrentTime > AlarmTime)
			{
				OnNetworkProcessingCausingSlowFrame.Broadcast();

				AlarmTime = CurrentTime + GIpNetDriverMaxDesiredTimeSliceBeforeAlarmSecs;
			}
		}

		int32 BytesRead = 0;

		// Reset the address on every pass. Otherwise if there's an error receiving, the address may be from a previous packet.
		FromAddr->SetAnyAddress();

		// Get data, if any.
		bool bOk = false;
		ESocketErrors Error = SE_NO_ERROR;
		bool bUsingReceiveThread = SocketReceiveThreadRunnable.IsValid();

		if (bUsingReceiveThread)
		{
			FReceivedPacket IncomingPacket;
			const bool bHasPacket = SocketReceiveThreadRunnable->ReceiveQueue.Dequeue(IncomingPacket);
			if (!bHasPacket)
			{
				break;
			}

			if (IncomingPacket.FromAddress.IsValid())
			{
				FromAddr = IncomingPacket.FromAddress.ToSharedRef();
			}
			Error = IncomingPacket.Error;
			bOk = Error == SE_NO_ERROR;
			
			if (IncomingPacket.PacketBytes.Num() <= sizeof(Data))
			{
				FMemory::Memcpy(Data, IncomingPacket.PacketBytes.GetData(), IncomingPacket.PacketBytes.Num());
				BytesRead = IncomingPacket.PacketBytes.Num();
			}
			else
			{
				UE_LOG(LogNet, Log, TEXT("IpNetDriver receive thread received a packet of %d bytes, which is larger than the data buffer size of %d bytes."), IncomingPacket.PacketBytes.Num(), sizeof(Data));
				continue;
			}
		}
		else
		{
			SCOPE_CYCLE_COUNTER(STAT_IpNetDriver_RecvFromSocket);
			bOk = Socket->RecvFrom(Data, sizeof(Data), BytesRead, *FromAddr);
		}

		UIpConnection* Connection = nullptr;
		UIpConnection* const MyServerConnection = GetServerConnection();

		if (bOk)
		{
			// Immediately stop processing (continuing to next receive), for empty packets (usually a DDoS)
			if (BytesRead == 0)
			{
				DDoS.IncBadPacketCounter();
				continue;
			}

			FPacketAudit::NotifyLowLevelReceive(DataRef, BytesRead);
		}
		else
		{
			if (!bUsingReceiveThread)
			{
				Error = SocketSubsystem->GetLastErrorCode();
			}

			if (Error == SE_EWOULDBLOCK || Error == SE_NO_ERROR || Error == SE_ECONNABORTED)
			{
				// No data or no error? (SE_ECONNABORTED is for PS4 LAN cable pulls)
				break;
			}
			else if (Error != SE_ECONNRESET && Error != SE_UDP_ERR_PORT_UNREACH)
			{
				// MalformedPacket: Client tried receiving a packet that exceeded the maximum packet limit
				// enforced by the server
				if (Error == SE_EMSGSIZE)
				{
					DDoS.IncBadPacketCounter();

					if (MyServerConnection)
					{
						if (MyServerConnection->RemoteAddr->CompareEndpoints(*FromAddr))
						{
							Connection = MyServerConnection;
						}
						else
						{
							UE_LOG(LogNet, Log, TEXT("Received packet with bytes > max MTU from an incoming IP address that doesn't match expected server address: Actual: %s Expected: %s"),
								*FromAddr->ToString(true),
								MyServerConnection->RemoteAddr.IsValid() ? *MyServerConnection->RemoteAddr->ToString(true) : TEXT("Invalid"));
							continue;
						}
					}

					if (Connection != nullptr)
					{
						UE_SECURITY_LOG(Connection, ESecurityEvent::Malformed_Packet, TEXT("Received Packet with bytes > max MTU"));
					}
				}
				else
				{
					DDoS.IncErrorPacketCounter();
				}

				FString ErrorString = FString::Printf(TEXT("UIpNetDriver::TickDispatch: Socket->RecvFrom: %i (%s) from %s"),
					static_cast<int32>(Error),
					SocketSubsystem->GetSocketError(Error),
					*FromAddr->ToString(true));


				// This should only occur on clients - on servers it leaves the NetDriver in an invalid/vulnerable state
				if (MyServerConnection != nullptr)
				{
					GEngine->BroadcastNetworkFailure(GetWorld(), this, ENetworkFailure::ConnectionLost, ErrorString);
					Shutdown();

					break;
				}
				else
				{
					UE_CLOG(!DDoS.CheckLogRestrictions(), LogNet, Warning, TEXT("%s"), *ErrorString);
				}

				// Unexpected packet errors should continue to the next iteration, rather than block all further receives this tick
				continue;
			}
		}

		// Very-early-out - the NetConnection per frame time limit, limits all packet processing
		if (DDoS.ShouldBlockNetConnPackets())
		{
			if (bOk)
			{
				DDoS.IncDroppedPacketCounter();
			}

			continue;
		}

		// Figure out which socket the received data came from.
		if (MyServerConnection)
		{
			if (MyServerConnection->RemoteAddr->CompareEndpoints(*FromAddr))
			{
				Connection = MyServerConnection;
			}
			else
			{
				UE_LOG(LogNet, Warning, TEXT("Incoming ip address doesn't match expected server address: Actual: %s Expected: %s"),
					*FromAddr->ToString(true),
					MyServerConnection->RemoteAddr.IsValid() ? *MyServerConnection->RemoteAddr->ToString(true) : TEXT("Invalid"));
			}
		}

		if (Connection == nullptr)
		{
			UNetConnection** Result = MappedClientConnections.Find(FromAddr);

			Connection = (Result != nullptr ? Cast<UIpConnection>(*Result) : nullptr);

			check(Connection == nullptr || Connection->RemoteAddr->CompareEndpoints(*FromAddr));
		}


		if( bOk == false )
		{
			if( Connection )
			{
				if( Connection != GetServerConnection() )
				{
					// We received an ICMP port unreachable from the client, meaning the client is no longer running the game
					// (or someone is trying to perform a DoS attack on the client)

					// rcg08182002 Some buggy firewalls get occasional ICMP port
					// unreachable messages from legitimate players. Still, this code
					// will drop them unceremoniously, so there's an option in the .INI
					// file for servers with such flakey connections to let these
					// players slide...which means if the client's game crashes, they
					// might get flooded to some degree with packets until they timeout.
					// Either way, this should close up the usual DoS attacks.
					if ((Connection->State != USOCK_Open) || (!AllowPlayerPortUnreach))
					{
						if (LogPortUnreach)
						{
							UE_LOG(LogNet, Log, TEXT("Received ICMP port unreachable from client %s.  Disconnecting."),
								*FromAddr->ToString(true));
						}
						Connection->CleanUp();
					}
				}
			}
			else
			{
				DDoS.IncNonConnPacketCounter();

				if (LogPortUnreach && !DDoS.CheckLogRestrictions())
				{
					UE_LOG(LogNet, Log, TEXT("Received ICMP port unreachable from %s.  No matching connection found."),
						*FromAddr->ToString(true));
				}
			}
		}
		else
		{
			bool bIgnorePacket = false;

			// If we didn't find a client connection, maybe create a new one.
			if( !Connection )
			{
				if (DDoS.IsDDoSDetectionEnabled())
				{
					// If packet limits were reached, stop processing
					if (DDoS.ShouldBlockNonConnPackets())
					{
						DDoS.IncDroppedPacketCounter();
						continue;
					}

					DDoS.IncNonConnPacketCounter();
					DDoS.CondCheckNonConnQuotasAndLimits();
				}

				// Determine if allowing for client/server connections
				const bool bAcceptingConnection = Notify != nullptr && Notify->NotifyAcceptingConnection() == EAcceptConnection::Accept;

				if (bAcceptingConnection)
				{
					UE_CLOG(!DDoS.CheckLogRestrictions(), LogNet, Log, TEXT("NotifyAcceptingConnection accepted from: %s"),
							*FromAddr->ToString(true));

					bool bPassedChallenge = false;
					TSharedPtr<StatelessConnectHandlerComponent> StatelessConnect;

					bIgnorePacket = true;

					if (ConnectionlessHandler.IsValid() && StatelessConnectComponent.IsValid())
					{
						StatelessConnect = StatelessConnectComponent.Pin();
						FString IncomingAddress = FromAddr->ToString(true);

						const ProcessedPacket UnProcessedPacket =
												ConnectionlessHandler->IncomingConnectionless(IncomingAddress, DataRef, BytesRead);

						bPassedChallenge = !UnProcessedPacket.bError && StatelessConnect->HasPassedChallenge(IncomingAddress);

						if (bPassedChallenge)
						{
							BytesRead = FMath::DivideAndRoundUp(UnProcessedPacket.CountBits, 8);

							if (BytesRead > 0)
							{
								DataRef = UnProcessedPacket.Data;
								bIgnorePacket = false;
							}
						}
					}
#if !UE_BUILD_SHIPPING
					else if (FParse::Param(FCommandLine::Get(), TEXT("NoPacketHandler")))
					{
						UE_CLOG(!DDoS.CheckLogRestrictions(), LogNet, Log,
									TEXT("Accepting connection without handshake, due to '-NoPacketHandler'."))

						bIgnorePacket = false;
						bPassedChallenge = true;
					}
#endif
					else
					{
						UE_LOG(LogNet, Log,
								TEXT("Invalid ConnectionlessHandler (%i) or StatelessConnectComponent (%i); can't accept connections."),
								(int32)(ConnectionlessHandler.IsValid()), (int32)(StatelessConnectComponent.IsValid()));
					}

					if (bPassedChallenge)
					{
						SCOPE_CYCLE_COUNTER(Stat_IpNetDriverAddNewConnection);

						UE_LOG(LogNet, Log, TEXT("Server accepting post-challenge connection from: %s"), *FromAddr->ToString(true));

						Connection = NewObject<UIpConnection>(GetTransientPackage(), NetConnectionClass);
						check(Connection);

#if STATELESSCONNECT_HAS_RANDOM_SEQUENCE
						// Set the initial packet sequence from the handshake data
						if (StatelessConnect.IsValid())
						{
							int32 ServerSequence = 0;
							int32 ClientSequence = 0;

							StatelessConnect->GetChallengeSequence(ServerSequence, ClientSequence);

							Connection->InitSequence(ClientSequence, ServerSequence);
						}
#endif

						Connection->InitRemoteConnection( this, Socket, World ? World->URL : FURL(), *FromAddr, USOCK_Open);

						if (Connection->Handler.IsValid())
						{
							Connection->Handler->BeginHandshaking();
						}

						Notify->NotifyAcceptedConnection( Connection );
						AddClientConnection(Connection);
					}
					else
					{
						UE_LOG( LogNet, VeryVerbose, TEXT( "Server failed post-challenge connection from: %s" ), *FromAddr->ToString( true ) );
					}
				}
				else
				{
					UE_LOG( LogNet, VeryVerbose, TEXT( "NotifyAcceptingConnection denied from: %s" ), *FromAddr->ToString( true ) );
				}
			}

			// Send the packet to the connection for processing.
			if (Connection && !bIgnorePacket)
			{
				if (DDoS.IsDDoSDetectionEnabled())
				{
					DDoS.IncNetConnPacketCounter();
					DDoS.CondCheckNetConnLimits();
				}

				Connection->ReceivedRawPacket( DataRef, BytesRead );
			}
		}
	}

	DDoS.PostFrameReceive();

	const float DeltaReceiveTime = FPlatformTime::Seconds() - StartReceiveTime;

	if (DeltaReceiveTime > GIpNetDriverLongFramePrintoutThresholdSecs)
	{
		UE_LOG( LogNet, Warning, TEXT( "UIpNetDriver::TickDispatch: Took too long to receive packets. Time: %2.2f %s" ), DeltaReceiveTime, *GetName() );
	}
}

void UIpNetDriver::LowLevelSend(FString Address, void* Data, int32 CountBits, FOutPacketTraits& Traits)
{
	bool bValidAddress = !Address.IsEmpty();
	TSharedRef<FInternetAddr> RemoteAddr = GetSocketSubsystem()->CreateInternetAddr();

	if (bValidAddress)
	{
		RemoteAddr->SetIp(*Address, bValidAddress);
	}

	if (bValidAddress)
	{
		const uint8* DataToSend = reinterpret_cast<uint8*>(Data);

		if (ConnectionlessHandler.IsValid())
		{
			const ProcessedPacket ProcessedData =
					ConnectionlessHandler->OutgoingConnectionless(Address, (uint8*)DataToSend, CountBits, Traits);

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


		int32 BytesSent = 0;

		if (CountBits > 0)
		{
			CLOCK_CYCLES(SendCycles);
			Socket->SendTo(DataToSend, FMath::DivideAndRoundUp(CountBits, 8), BytesSent, *RemoteAddr);
			UNCLOCK_CYCLES(SendCycles);
		}


		// @todo: Can't implement these profiling events (require UNetConnections)
		//NETWORK_PROFILER(GNetworkProfiler.FlushOutgoingBunches(/* UNetConnection */));
		//NETWORK_PROFILER(GNetworkProfiler.TrackSocketSendTo(Socket->GetDescription(),Data,BytesSent,NumPacketIdBits,NumBunchBits,
							//NumAckBits,NumPaddingBits, /* UNetConnection */));
	}
	else
	{
		UE_LOG(LogNet, Warning, TEXT("UIpNetDriver::LowLevelSend: Invalid send address '%s'"), *Address);
	}
}



FString UIpNetDriver::LowLevelGetNetworkNumber()
{
	return LocalAddr->ToString(true);
}

void UIpNetDriver::LowLevelDestroy()
{
	Super::LowLevelDestroy();

	// Close the socket.
	if( Socket && !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Wait for send tasks if needed before closing the socket,
		// since at this point CleanUp() may not have been called on the server connection.
		UIpConnection* const IpServerConnection = GetServerConnection();
		if (IpServerConnection)
		{
			IpServerConnection->WaitForSendTasks();
		}

		// If using a recieve thread, shut down the socket, which will signal the thread to exit gracefully, then wait on the thread.
		if (SocketReceiveThread.IsValid() && SocketReceiveThreadRunnable.IsValid())
		{
			SocketReceiveThreadRunnable->bIsRunning = false;
			Socket->Shutdown(ESocketShutdownMode::Read);

			SCOPE_CYCLE_COUNTER(STAT_IpNetDriver_Destroy_WaitForReceiveThread);
			SocketReceiveThread->WaitForCompletion();
		}

		ISocketSubsystem* SocketSubsystem = GetSocketSubsystem();
		if( !Socket->Close() )
		{
			UE_LOG(LogExit, Log, TEXT("closesocket error (%i)"), (int32)SocketSubsystem->GetLastErrorCode() );
		}
		// Free the memory the OS allocated for this socket
		SocketSubsystem->DestroySocket(Socket);
		Socket = NULL;
		UE_LOG(LogExit, Log, TEXT("%s shut down"),*GetDescription() );
	}

}


bool UIpNetDriver::HandleSocketsCommand( const TCHAR* Cmd, FOutputDevice& Ar, UWorld* InWorld )
{
	Ar.Logf(TEXT(""));
	if (Socket != NULL)
	{
		TSharedRef<FInternetAddr> LocalInternetAddr = GetSocketSubsystem()->CreateInternetAddr();
		Socket->GetAddress(*LocalInternetAddr);
		Ar.Logf(TEXT("%s Socket: %s"), *GetDescription(), *LocalInternetAddr->ToString(true));
	}		
	else
	{
		Ar.Logf(TEXT("%s Socket: null"), *GetDescription());
	}
	return UNetDriver::Exec( InWorld, TEXT("SOCKETS"),Ar);
}

bool UIpNetDriver::Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar )
{
	if (FParse::Command(&Cmd,TEXT("SOCKETS")))
	{
		return HandleSocketsCommand( Cmd, Ar, InWorld );
	}
	return UNetDriver::Exec( InWorld, Cmd,Ar);
}

UIpConnection* UIpNetDriver::GetServerConnection() 
{
	return (UIpConnection*)ServerConnection;
}

UIpNetDriver::FReceiveThreadRunnable::FReceiveThreadRunnable(UIpNetDriver* InOwningNetDriver)
	: ReceiveQueue(CVarNetIpNetDriverReceiveThreadQueueMaxPackets.GetValueOnAnyThread())
	, bIsRunning(true)
	, OwningNetDriver(InOwningNetDriver)
{
	SocketSubsystem = OwningNetDriver->GetSocketSubsystem();
}

uint32 UIpNetDriver::FReceiveThreadRunnable::Run()
{
	const FTimespan Timeout = FTimespan::FromMilliseconds(CVarNetIpNetDriverReceiveThreadPollTimeMS.GetValueOnAnyThread());

	UE_LOG(LogNet, Log, TEXT("Receive Thread Startup."));

	while (bIsRunning && OwningNetDriver->Socket)
	{
		FReceivedPacket IncomingPacket;

		if (OwningNetDriver->Socket->Wait(ESocketWaitConditions::WaitForRead, Timeout))
		{
			bool bOk = false;
			int32 BytesRead = 0;

			IncomingPacket.FromAddress = SocketSubsystem->CreateInternetAddr();

			IncomingPacket.PacketBytes.AddUninitialized(MAX_PACKET_SIZE);

			{
				SCOPE_CYCLE_COUNTER(STAT_IpNetDriver_RecvFromSocket);
				bOk = OwningNetDriver->Socket->RecvFrom(IncomingPacket.PacketBytes.GetData(), IncomingPacket.PacketBytes.Num(), BytesRead, *IncomingPacket.FromAddress);
			}

			if (bOk)
			{
				if (BytesRead == 0)
				{
					// Don't even queue empty packets, they can be ignored.
					continue;
				}
			}
			else
			{
				// This relies on the platform's implementation using thread-local storage for the last socket error code.
				IncomingPacket.Error = SocketSubsystem->GetLastErrorCode();

				// Pass all other errors back to the Game Thread
				if (IncomingPacket.Error == SE_EWOULDBLOCK || IncomingPacket.Error == SE_NO_ERROR || IncomingPacket.Error == SE_ECONNABORTED)
				{
					continue;
				}
			}


			IncomingPacket.PacketBytes.SetNum(FMath::Max(BytesRead, 0), false);
			IncomingPacket.PlatformTimeSeconds = FPlatformTime::Seconds();

			// Add packet to queue. Since ReceiveQueue is a TCircularQueue, if the queue is full, this will simply return false without adding anything.
			ReceiveQueue.Enqueue(MoveTemp(IncomingPacket));
		}
		else
		{
			const ESocketErrors WaitError = SocketSubsystem->GetLastErrorCode();
			if(WaitError != ESocketErrors::SE_NO_ERROR)
			{
				IncomingPacket.Error = WaitError;
				IncomingPacket.PlatformTimeSeconds = FPlatformTime::Seconds();

				ReceiveQueue.Enqueue(MoveTemp(IncomingPacket));
			}
		}
	}

	return 0;
}
