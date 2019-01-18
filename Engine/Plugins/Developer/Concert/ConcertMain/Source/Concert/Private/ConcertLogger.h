// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "IConcertEndpoint.h"
#include "ConcertMessageData.h"
#include "IConcertTransportLogger.h"
#include "ConcertLogger.generated.h"

struct FConcertMessageContext;

UENUM()
enum class EConcertLogMessageAction : uint8
{
	Send,
	Publish,
	Receive,
	Queue,
	Discard,
	Duplicate,
	TimeOut,
	Process,
	EndpointDiscovery,
	EndpointTimeOut,
	EndpointClosure,
};

USTRUCT()
struct FConcertLog
{
	GENERATED_BODY()

	UPROPERTY()
	uint64 Frame;

	UPROPERTY()
	FGuid MessageId;

	UPROPERTY()
	uint16 MessageOrderIndex;

	UPROPERTY()
	uint16 ChannelId;

	UPROPERTY()
	FDateTime Timestamp;

	UPROPERTY()
	EConcertLogMessageAction MessageAction;

	UPROPERTY()
	FName MessageTypeName;

	UPROPERTY()
	FGuid OriginEndpointId;

	UPROPERTY()
	FGuid DestinationEndpointId;

	UPROPERTY()
	FName CustomPayloadTypename;

	UPROPERTY()
	int32 CustomPayloadUncompressedByteSize;

	UPROPERTY()
	FString StringPayload;

	UPROPERTY(Transient)
	FConcertSessionSerializedPayload SerializedPayload;
};

class FConcertLogger : public IConcertTransportLogger
{
public:
	/** Factory function for use with FConcertTransportLoggerFactory */
	static IConcertTransportLoggerRef CreateLogger(const FConcertEndpointContext& InOwnerContext);

	explicit FConcertLogger(const FConcertEndpointContext& InOwnerContext);
	virtual ~FConcertLogger();

	// IConcertTransportLogger interface
	virtual bool IsLogging() const override;
	virtual void StartLogging() override;
	virtual void StopLogging() override;
	virtual void FlushLog() override;
	virtual void LogTimeOut(const TSharedRef<IConcertMessage>& Message, const FGuid& EndpointId, const FDateTime& UtcNow) override;
	virtual void LogSendAck(const FConcertAckData& AckData, const FGuid& DestEndpoint) override;
	virtual void LogSendEndpointClosed(const FConcertEndpointClosedData& EndpointClosedData, const FGuid& DestEndpoint, const FDateTime& UtcNow) override;
	virtual void LogSendReliableHandshake(const FConcertReliableHandshakeData& ReliableHandshakeData, const FGuid& DestEndpoint, const FDateTime& UtcNow) override;
	virtual void LogReceiveReliableHandshake(const FConcertReliableHandshakeData& ReliableHandshakeData, const FGuid& DestEndpoint, const FDateTime& UtcNow) override;
	virtual void LogPublish(const TSharedRef<IConcertMessage>& Message) override;
	virtual void LogSend(const TSharedRef<IConcertMessage>& Message, const FGuid& DestEndpoint) override;
	virtual void LogMessageReceived(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint) override;
	virtual void LogMessageQueued(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint) override;
	virtual void LogMessageDiscarded(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint, const EMessageDiscardedReason Reason) override;
	virtual void LogProcessEvent(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint) override;
	virtual void LogProcessRequest(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint) override;
	virtual void LogProcessResponse(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint) override;
	virtual void LogProcessAck(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint) override;
	virtual void LogRemoteEndpointDiscovery(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint) override;
	virtual void LogRemoteEndpointTimeOut(const FGuid& EndpointId, const FDateTime& UtcNow) override;
	virtual void LogRemoteEndpointClosure(const FGuid& EndpointId, const FDateTime& UtcNow) override;

private:
	void InternalStartLogging();
	void InternalStopLogging();
	void InternalFlushLog();

	void LogHeader();
	void LogEntry(FConcertLog& Log);

	/** */
	bool bIsLogging;

	/** */
	FConcertEndpointContext OwnerContext;

	/** Queue for unprocessed logs */
	TQueue<FConcertLog, EQueueMode::Mpsc> LogQueue;

	/** Archive & CS used to write CSV file, if any */
	mutable FCriticalSection CSVArchiveCS;
	TUniquePtr<FArchive> CSVArchive;
};
