// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "BackChannel/Protocol/OSC/BackChannelOSCConnection.h"
#include "BackChannel/Private/BackChannelCommon.h"
#include "BackChannel/Transport/IBackChannelTransport.h"
#include "BackChannel/Protocol/OSC/BackChannelOSC.h"
#include "Sockets.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeLock.h"

FBackChannelOSCConnection::FBackChannelOSCConnection(TSharedRef<IBackChannelConnection> InConnection)
	: Connection(InConnection)
{
	LastReceiveTime = 0;
	LastSendTime = 0;
	PingTime = 3;
	HasErrorState = false;

	// OSC connections expect a size followed by payload for TCP connections
	ExpectedDataSize = 4;
	ReceivedDataSize = 0;

	const int32 kDefaultBufferSize = 4096;
	ReceiveBuffer.AddUninitialized(kDefaultBufferSize);
}

FBackChannelOSCConnection::~FBackChannelOSCConnection()
{
	UE_LOG(LogBackChannel, Verbose, TEXT("Destroying OSC Connection to %s"), *GetDescription());
	if (IsRunning)
	{
		Stop();
	}
}

void FBackChannelOSCConnection::ReceivePackets(const float MaxTime /*= 0*/)
{
	ReceiveData(MaxTime);
	DispatchMessages();

	const float TimeSinceSend = FPlatformTime::Seconds() - LastSendTime;

	if (TimeSinceSend >= PingTime)
	{
		FBackChannelOSCMessage Msg(TEXT("/ping"));
		SendPacket(Msg);
	}
}

void FBackChannelOSCConnection::ReceiveData(const float MaxTime /*= 0*/)
{
	const double StarTime = FPlatformTime::Seconds();

	bool KeepReceiving = false;

	int PacketsReceived = 0;

	do
	{
		FScopeLock Lock(&ReceiveMutex);

		Connection->GetSocket()->Wait(ESocketWaitConditions::WaitForRead, FTimespan(0, 0, MaxTime));

		int32 Received = Connection->ReceiveData(ReceiveBuffer.GetData() + ReceivedDataSize, ExpectedDataSize - ReceivedDataSize);

		if (Received > 0)
		{
			LastReceiveTime = FPlatformTime::Seconds();

			ReceivedDataSize += Received;

			if (ReceivedDataSize == ExpectedDataSize)
			{
				// reset this
				ReceivedDataSize = 0;

				if (ExpectedDataSize == 4)
				{
					int32 Size(0);
					FMemory::Memcpy(&Size, ReceiveBuffer.GetData(), sizeof(int32));

					if (Size > ReceiveBuffer.Num())
					{
						ReceiveBuffer.AddUninitialized(Size - ReceiveBuffer.Num());
					}

					ExpectedDataSize = Size;
				}
				else
				{
					// read packet
					TSharedPtr<FBackChannelOSCPacket> Packet = FBackChannelOSCPacket::CreateFromBuffer(ReceiveBuffer.GetData(), ExpectedDataSize);

					if (Packet.IsValid())
					{
						bool bAddPacket = true;

						if (Packet->GetType() == OSCPacketType::Message)
						{
							auto MsgPacket = StaticCastSharedPtr<FBackChannelOSCMessage>(Packet);

							const FString& NewAddress = MsgPacket->GetAddress();

							UE_CLOG(GBackChannelLogPackets, LogBackChannel, Log, TEXT("Received msg to %s of %d bytes"), *NewAddress, ExpectedDataSize);

							int32 CurrentCount = GetMessageCountForPath(*NewAddress);

							int32 MaxMessages = GetMessageLimitForPath(*NewAddress);

							if (CurrentCount > 0)
							{
								UE_CLOG(GBackChannelLogPackets, LogBackChannel, Log, TEXT("%s has %d pending messages"), *NewAddress, CurrentCount + 1);

								if (MaxMessages > 0 && CurrentCount >= MaxMessages)
								{
									UE_CLOG(GBackChannelLogPackets, LogBackChannel, Log, TEXT("Discarding old messages due to limit of %d"), MaxMessages);
									RemoveMessagesWithPath(*NewAddress, 1);
								}
							}
						}
						else
						{
							UE_CLOG(GBackChannelLogPackets, LogBackChannel, Log, TEXT("Received #bundle of %d bytes"), ExpectedDataSize);
						}

						ReceivedPackets.Add(Packet);
					}

					ExpectedDataSize = 4;
					PacketsReceived++;
				}
			}
		}

		const double ElapsedTime = FPlatformTime::Seconds() - StarTime;

		KeepReceiving = PacketsReceived == 0 && ElapsedTime < MaxTime;

	} while (KeepReceiving);
}

void FBackChannelOSCConnection::DispatchMessages()
{
	FScopeLock Lock(&ReceiveMutex);

	for (auto& Packet : ReceivedPackets)
	{
		if (Packet->GetType() == OSCPacketType::Message)
		{
			TSharedPtr<FBackChannelOSCMessage> Msg = StaticCastSharedPtr<FBackChannelOSCMessage>(Packet);

			UE_LOG(LogBackChannel, Verbose, TEXT("Dispatching %s"), *Msg->GetAddress());
			DispatchMap.DispatchMessage(*Msg.Get());
		}
	}

	ReceivedPackets.Empty();
}

bool FBackChannelOSCConnection::StartReceiveThread()
{
	check(IsRunning == false);

	ExitRequested = false;

	// todo- expose this priority
	FRunnableThread* Thread = FRunnableThread::Create(this, TEXT("OSCHostConnection"), 1024 * 1024, TPri_AboveNormal);

	if (Thread)
	{
		IsRunning = true;
	}

	UE_LOG(LogBackChannel, Verbose, TEXT("Started OSC Connection to %s"), *GetDescription());

	return Thread != nullptr;
}

/* Returns true if running in the background */
bool FBackChannelOSCConnection::IsThreaded() const
{
	return IsRunning;
}

uint32 FBackChannelOSCConnection::Run()
{
	const int32 kDefaultBufferSize = 4096;

	TArray<uint8> Buffer;
	Buffer.AddUninitialized(kDefaultBufferSize);

	const float kTimeout = 10;

	LastReceiveTime = LastSendTime = FPlatformTime::Seconds();

	UE_LOG(LogBackChannel, Verbose, TEXT("OSC Connection to %s is Running"), *Connection->GetDescription());

	while (ExitRequested == false)
	{		
		ReceivePackets(1);

		const double TimeSinceActivity = (FPlatformTime::Seconds() - LastReceiveTime);
		if (TimeSinceActivity >= kTimeout)
		{
			UE_LOG(LogBackChannel, Error, TEXT("Connection to %s timed out after %.02f seconds"), *Connection->GetDescription(), TimeSinceActivity);
			HasErrorState = true;
			ExitRequested = true;
		}

		FPlatformProcess::SleepNoStats(0);
	}

	UE_LOG(LogBackChannel, Verbose, TEXT("OSC Connection to %s is exiting."), *Connection->GetDescription());
	IsRunning = false;
	return 0;
}

void FBackChannelOSCConnection::Stop()
{
	if (IsRunning)
	{
		UE_LOG(LogBackChannel, Verbose, TEXT("Requesting OSC Connection to stop.."));

		ExitRequested = true;

		while (IsRunning)
		{
			FPlatformProcess::SleepNoStats(0.01);
		}
	}

	UE_LOG(LogBackChannel, Verbose, TEXT("OSC Connection is stopped"));
	Connection = nullptr;
}

bool FBackChannelOSCConnection::IsConnected() const
{
	return Connection->IsConnected() && (HasErrorState == false);
}

bool FBackChannelOSCConnection::SendPacket(FBackChannelOSCPacket& Packet)
{
	FBackChannelOSCMessage* MsgPacket = (FBackChannelOSCMessage*)&Packet;

	UE_LOG(LogBackChannel, Verbose, TEXT("Sending packet to %s"), *MsgPacket->GetAddress());
	TArray<uint8> Data = Packet.WriteToBuffer();
	return SendPacketData(Data.GetData(), Data.Num());
}

bool FBackChannelOSCConnection::SendPacketData(const void* Data, const int32 DataLen)
{
	FScopeLock Lock(&SendMutex);

	if (!IsConnected())
	{
		return false;
	}

	// write size
	int32 Sent = 0;

	// TODO - differentiate between TCP / UDP 
	if (DataLen > 0)
	{
		// OSC over TCP requires a size followed by the packet (TODO - combine these?)
		Connection->SendData(&DataLen, sizeof(DataLen));

		ANSICHAR* RawData = (ANSICHAR*)Data;
		check(FCStringAnsi::Strlen(RawData) < 64);
		Sent = Connection->SendData(Data, DataLen);

		LastSendTime = FPlatformTime::Seconds();
	}

	return Sent > 0;
}

FString FBackChannelOSCConnection::GetDescription()
{
	return FString::Printf(TEXT("OSCConnection to %s"), *Connection->GetDescription());
}

void FBackChannelOSCConnection::SetMessageOptions(const TCHAR* Path, int32 MaxQueuedMessages)
{
	FScopeLock Lock(&ReceiveMutex);
	MessageLimits.FindOrAdd(Path) = MaxQueuedMessages;
}

FDelegateHandle FBackChannelOSCConnection::AddMessageHandler(const TCHAR* Path, FBackChannelDispatchDelegate::FDelegate Delegate)
{
	FScopeLock Lock(&ReceiveMutex);
	return DispatchMap.GetAddressHandler(Path).Add(Delegate);
}

/* Remove a delegate handle */
void FBackChannelOSCConnection::RemoveMessageHandler(const TCHAR* Path, FDelegateHandle& InHandle)
{
	DispatchMap.GetAddressHandler(Path).Remove(InHandle);
	InHandle.Reset();
}

int32 FBackChannelOSCConnection::GetMessageCountForPath(const TCHAR* Path)
{
	FScopeLock Lock(&ReceiveMutex);
	
	int32 Count = 0;

	for (auto& Packet : ReceivedPackets)
	{
		if (Packet->GetType() == OSCPacketType::Message)
		{
			auto Msg = StaticCastSharedPtr<FBackChannelOSCMessage>(Packet);

			if (Msg->GetAddress() == Path)
			{
				Count++;
			}
		}
	}

	return Count;
}


int32 FBackChannelOSCConnection::GetMessageLimitForPath(const TCHAR* InPath)
{
	FScopeLock Lock(&ReceiveMutex);

	FString Path = InPath;

	if (Path.EndsWith(TEXT("*")))
	{
		Path.LeftChop(1);
	}

	// todo - search for vest match, not first match
	for (auto KV : MessageLimits)
	{
		const FString& Key = KV.Key;
		if (Path.StartsWith(Key))
		{
			return KV.Value;
		}
	}

	return -1;
}

void FBackChannelOSCConnection::RemoveMessagesWithPath(const TCHAR* Path, const int32 Num /*= 0*/)
{
	FScopeLock Lock(&ReceiveMutex);

	auto It = ReceivedPackets.CreateIterator();

	int32 RemovedCount = 0;

	while (It)
	{
		auto Packet = *It;
		bool bRemove = false;

		if (Packet->GetType() == OSCPacketType::Message)
		{
			TSharedPtr<FBackChannelOSCMessage> Msg = StaticCastSharedPtr<FBackChannelOSCMessage>(Packet);

			if (Msg->GetAddress() == Path)
			{
				bRemove = true;
			}
		}

		if (bRemove)
		{
			It.RemoveCurrent();
			RemovedCount++;

			if (Num > 0 && RemovedCount == Num)
			{
				break;
			}
		}
		else
		{
			It++;
		}
	}
}

