// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ConcertLogger.h"
#include "ConcertMessages.h"
#include "IConcertMessages.h"
#include "ConcertLogGlobal.h"

#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "HAL/FileManager.h"
#include "Containers/Ticker.h"
#include "UObject/UnrealType.h"
#include "UObject/StructOnScope.h"
#include "UObject/PropertyPortFlags.h"


namespace ConcertLoggerUtil
{

FString PayloadToString(const FStructOnScope& InPayload)
{
	FString Result;
	if (InPayload.GetStruct() && InPayload.GetStructMemory())
	{
		check(InPayload.GetStruct()->IsA<UScriptStruct>());
		((UScriptStruct*)InPayload.GetStruct())->ExportText(Result, InPayload.GetStructMemory(), InPayload.GetStructMemory(), nullptr, PPF_None, nullptr);
	}
	return Result;
}

FString SerializedPayloadToString(const FConcertSessionSerializedPayload& InPayload)
{
	FStructOnScope TempPayload;
	if (InPayload.GetPayload(TempPayload))
	{
		return PayloadToString(TempPayload);
	}
	return FString();
}

void PopulateLogMessagePayload(const FConcertSessionSerializedPayload& InPayload, FConcertLog& InOutLogMessage)
{
	InOutLogMessage.CustomPayloadTypename = InPayload.PayloadTypeName;
	InOutLogMessage.CustomPayloadUncompressedByteSize = InPayload.UncompressedPayloadSize;
	InOutLogMessage.StringPayload = SerializedPayloadToString(InPayload);
}

void PopulateLogMessage(const UScriptStruct* InMessageType, const void* InMessageData, const FGuid& InSourceEndpoint, const FGuid& InDestinationEndpoint, const FDateTime& InUtcNow, const EConcertLogMessageAction InMessageAction, FConcertLog& InOutLogMessage)
{
	checkf(InMessageType->IsChildOf(FConcertMessageData::StaticStruct()), TEXT("PopulateLogMessage can only be used with messages deriving from FConcertMessageData!"));

	const FConcertMessageData* ConcertMessageData = (const FConcertMessageData*)InMessageData;

	InOutLogMessage.Frame = GFrameCounter;
	InOutLogMessage.MessageId = ConcertMessageData->MessageId;
	InOutLogMessage.MessageOrderIndex = ConcertMessageData->MessageOrderIndex;
	InOutLogMessage.ChannelId = ConcertMessageData->ChannelId;
	InOutLogMessage.Timestamp = InUtcNow;
	InOutLogMessage.MessageAction = InMessageAction;
	InOutLogMessage.MessageTypeName = InMessageType->GetFName();
	InOutLogMessage.OriginEndpointId = InSourceEndpoint;
	InOutLogMessage.DestinationEndpointId = InDestinationEndpoint;
	InOutLogMessage.CustomPayloadUncompressedByteSize = 0;

	if (InMessageType->IsChildOf(FConcertSession_CustomEvent::StaticStruct()))
	{
		const FConcertSession_CustomEvent* ConcertCustomEventData = (const FConcertSession_CustomEvent*)InMessageData;
		InOutLogMessage.SerializedPayload = ConcertCustomEventData->SerializedPayload;
	}
	else if (InMessageType->IsChildOf(FConcertSession_CustomRequest::StaticStruct()))
	{
		const FConcertSession_CustomRequest* ConcertCustomRequestData = (const FConcertSession_CustomRequest*)InMessageData;
		InOutLogMessage.SerializedPayload = ConcertCustomRequestData->SerializedPayload;
	}
	else if (InMessageType->IsChildOf(FConcertSession_CustomResponse::StaticStruct()))
	{
		const FConcertSession_CustomResponse* ConcertCustomResponseData = (const FConcertSession_CustomResponse*)InMessageData;
		InOutLogMessage.SerializedPayload = ConcertCustomResponseData->SerializedPayload;
	}
}

FConcertLog BuildLogMessage(const UScriptStruct* InMessageType, const void* InMessageData, const FGuid& InSourceEndpoint, const FGuid& InDestinationEndpoint, const FDateTime& InUtcNow, const EConcertLogMessageAction InMessageAction)
{
	FConcertLog LogMessage;
	PopulateLogMessage(InMessageType, InMessageData, InSourceEndpoint, InDestinationEndpoint, InUtcNow, InMessageAction, LogMessage);
	return LogMessage;
}

FString MessageTypeToString(const UScriptStruct* InMessageType, const void* InMessageData)
{
	FString MessageTypeStr = InMessageType->GetName();

	if (InMessageData)
	{
		if (InMessageType->IsChildOf(FConcertSession_CustomEvent::StaticStruct()))
		{
			const FConcertSession_CustomEvent* ConcertCustomEventData = (const FConcertSession_CustomEvent*)InMessageData;
			MessageTypeStr += FString::Printf(TEXT("(%s)"), *ConcertCustomEventData->SerializedPayload.PayloadTypeName.ToString());
		}
		else if (InMessageType->IsChildOf(FConcertSession_CustomRequest::StaticStruct()))
		{
			const FConcertSession_CustomRequest* ConcertCustomRequestData = (const FConcertSession_CustomRequest*)InMessageData;
			MessageTypeStr += FString::Printf(TEXT("(%s)"), *ConcertCustomRequestData->SerializedPayload.PayloadTypeName.ToString());
		}
		else if (InMessageType->IsChildOf(FConcertSession_CustomResponse::StaticStruct()))
		{
			const FConcertSession_CustomResponse* ConcertCustomResponseData = (const FConcertSession_CustomResponse*)InMessageData;
			MessageTypeStr += FString::Printf(TEXT("(%s)"), *ConcertCustomResponseData->SerializedPayload.PayloadTypeName.ToString());
		}
	}

	return MessageTypeStr;
}

FString GetMessageTypeString(const TSharedRef<IConcertMessage>& Message)
{
	return MessageTypeToString(Message->GetMessageType(), Message->GetMessageTemplate());
}

FString GetMessageTypeString(const FConcertMessageContext& ConcertContext)
{
	return MessageTypeToString(ConcertContext.MessageType, ConcertContext.Message);
}

FString MessageToString(const FGuid& MessageId, const UScriptStruct* InMessageType, const void* InMessageData)
{
	return FString::Printf(TEXT("'%s' (%s)"), *MessageId.ToString(), *MessageTypeToString(InMessageType, InMessageData));
}

FString GetMessageString(const TSharedRef<IConcertMessage>& Message)
{
	return MessageToString(Message->GetMessageId(), Message->GetMessageType(), Message->GetMessageTemplate());
}

FString GetMessageString(const FConcertMessageContext& ConcertContext)
{
	return MessageToString(ConcertContext.Message->MessageId, ConcertContext.MessageType, ConcertContext.Message);
}

const TCHAR* ReliableHandshakeStateToString(const EConcertReliableHandshakeState InState)
{
	switch (InState)
	{
	case EConcertReliableHandshakeState::Negotiate:
		return TEXT("negotiate");
	case EConcertReliableHandshakeState::Success:
		return TEXT("success");
	default:
		return TEXT("");
	}
}

}

IConcertTransportLoggerRef FConcertLogger::CreateLogger(const FConcertEndpointContext& InOwnerContext)
{
	return MakeShared<FConcertLogger, ESPMode::ThreadSafe>(InOwnerContext);
}

FConcertLogger::FConcertLogger(const FConcertEndpointContext& InOwnerContext)
	: bIsLogging(false)
	, OwnerContext(InOwnerContext)
{
}

FConcertLogger::~FConcertLogger()
{
	InternalStopLogging();
}

bool FConcertLogger::IsLogging() const
{
	return bIsLogging;
}

void FConcertLogger::StartLogging()
{
	FScopeLock CSVArchiveLock(&CSVArchiveCS);
	InternalStartLogging();
}

void FConcertLogger::StopLogging()
{
	FScopeLock CSVArchiveLock(&CSVArchiveCS);
	InternalStopLogging();
}

void FConcertLogger::FlushLog()
{
	FScopeLock CSVArchiveLock(&CSVArchiveCS);
	InternalFlushLog();
}

void FConcertLogger::LogTimeOut(const TSharedRef<IConcertMessage>& Message, const FGuid& EndpointId, const FDateTime& UtcNow)
{
	UE_LOG(LogConcert, Warning, TEXT("%s: Message %s timed-out sending to '%s'."), *OwnerContext.ToString(), *ConcertLoggerUtil::GetMessageString(Message), *EndpointId.ToString());

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(ConcertLoggerUtil::BuildLogMessage(Message->GetMessageType(), Message->GetMessageTemplate(), Message->GetSenderId(), EndpointId, UtcNow, EConcertLogMessageAction::TimeOut));
}

void FConcertLogger::LogSendAck(const FConcertAckData& AckData, const FGuid& DestEndpoint)
{
	UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Acknowledgement '%s' sent to '%s' for message '%s'."), *OwnerContext.ToString(), *AckData.MessageId.ToString(), *DestEndpoint.ToString(), *AckData.SourceMessageId.ToString());

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(FConcertLog
		{
			GFrameCounter,
			AckData.MessageId,
			AckData.MessageOrderIndex,
			AckData.ChannelId,
			FDateTime(AckData.AckSendTimeTicks),
			EConcertLogMessageAction::Send,
			FConcertAckData::StaticStruct()->GetFName(),
			AckData.ConcertEndpointId,
			DestEndpoint,
			NAME_None,
			0
		}
	);
}

void FConcertLogger::LogSendEndpointClosed(const FConcertEndpointClosedData& EndpointClosedData, const FGuid& DestEndpoint, const FDateTime& UtcNow)
{
	UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Endpoint closure '%s' sent to '%s'."), *OwnerContext.ToString(), *EndpointClosedData.MessageId.ToString(), *DestEndpoint.ToString());

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(FConcertLog
		{
			GFrameCounter,
			EndpointClosedData.MessageId,
			EndpointClosedData.MessageOrderIndex,
			EndpointClosedData.ChannelId,
			UtcNow,
			EConcertLogMessageAction::Send,
			FConcertEndpointClosedData::StaticStruct()->GetFName(),
			EndpointClosedData.ConcertEndpointId,
			DestEndpoint,
			NAME_None,
			0
		}
	);
}

void FConcertLogger::LogSendReliableHandshake(const FConcertReliableHandshakeData& ReliableHandshakeData, const FGuid& DestEndpoint, const FDateTime& UtcNow)
{
	UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Handshake sent to '%s' (state: %s, channel: %d, index: %d)."), 
		*OwnerContext.ToString(),
		*DestEndpoint.ToString(), 
		ConcertLoggerUtil::ReliableHandshakeStateToString(ReliableHandshakeData.HandshakeState),
		ReliableHandshakeData.ReliableChannelId, 
		ReliableHandshakeData.NextMessageIndex
	);

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(FConcertLog
		{
			GFrameCounter,
			ReliableHandshakeData.MessageId,
			ReliableHandshakeData.MessageOrderIndex,
			ReliableHandshakeData.ChannelId,
			UtcNow,
			EConcertLogMessageAction::Send,
			FConcertReliableHandshakeData::StaticStruct()->GetFName(),
			ReliableHandshakeData.ConcertEndpointId,
			DestEndpoint,
			NAME_None,
			0
		}
	);
}

void FConcertLogger::LogReceiveReliableHandshake(const FConcertReliableHandshakeData& ReliableHandshakeData, const FGuid& DestEndpoint, const FDateTime& UtcNow)
{
	UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Handshake received from '%s' (state: %s, channel: %d, index: %d)."), 
		*OwnerContext.ToString(),
		*ReliableHandshakeData.ConcertEndpointId.ToString(), 
		ConcertLoggerUtil::ReliableHandshakeStateToString(ReliableHandshakeData.HandshakeState),
		ReliableHandshakeData.ReliableChannelId, 
		ReliableHandshakeData.NextMessageIndex
	);

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(FConcertLog
		{
			GFrameCounter,
			ReliableHandshakeData.MessageId,
			ReliableHandshakeData.MessageOrderIndex,
			ReliableHandshakeData.ChannelId,
			UtcNow,
			EConcertLogMessageAction::Receive,
			FConcertReliableHandshakeData::StaticStruct()->GetFName(),
			ReliableHandshakeData.ConcertEndpointId,
			DestEndpoint,
			NAME_None,
			0
		}
	);
}

void FConcertLogger::LogPublish(const TSharedRef<IConcertMessage>& Message)
{
	UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Message %s published."), *OwnerContext.ToString(), *ConcertLoggerUtil::GetMessageString(Message));

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(ConcertLoggerUtil::BuildLogMessage(Message->GetMessageType(), Message->GetMessageTemplate(), Message->GetSenderId(), FGuid(), Message->GetCreationDate(), EConcertLogMessageAction::Publish));
}

void FConcertLogger::LogSend(const TSharedRef<IConcertMessage>& Message, const FGuid& DestEndpoint)
{
	UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Message %s sent to '%s'."), *OwnerContext.ToString(), *ConcertLoggerUtil::GetMessageString(Message), *DestEndpoint.ToString());

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(ConcertLoggerUtil::BuildLogMessage(Message->GetMessageType(), Message->GetMessageTemplate(), Message->GetSenderId(), DestEndpoint, Message->GetCreationDate(), EConcertLogMessageAction::Send));
}

void FConcertLogger::LogMessageReceived(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint)
{
	UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Message %s received from '%s'."), *OwnerContext.ToString(), *ConcertLoggerUtil::GetMessageString(ConcertContext), *ConcertContext.SenderConcertEndpointId.ToString());

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(ConcertLoggerUtil::BuildLogMessage(ConcertContext.MessageType, ConcertContext.Message, ConcertContext.SenderConcertEndpointId, DestEndpoint, ConcertContext.UtcNow, EConcertLogMessageAction::Receive));
}

void FConcertLogger::LogMessageQueued(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint)
{
	UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Message %s queued."), *OwnerContext.ToString(), *ConcertLoggerUtil::GetMessageString(ConcertContext));

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(ConcertLoggerUtil::BuildLogMessage(ConcertContext.MessageType, ConcertContext.Message, ConcertContext.SenderConcertEndpointId, DestEndpoint, ConcertContext.UtcNow, EConcertLogMessageAction::Queue));
}

void FConcertLogger::LogMessageDiscarded(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint, const EMessageDiscardedReason Reason)
{
	switch (Reason)
	{
	case EMessageDiscardedReason::NotRequired:
		UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Message %s discarded. Message was not required."), *OwnerContext.ToString(), *ConcertLoggerUtil::GetMessageString(ConcertContext));
		break;

	case EMessageDiscardedReason::AlreadyProcessed:
		UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Message %s discarded. Message was already processed."), *OwnerContext.ToString(), *ConcertLoggerUtil::GetMessageString(ConcertContext));
		break;

	case EMessageDiscardedReason::UnknownEndpoint:
		UE_LOG(LogConcert, Warning, TEXT("%s: Message %s discarded. Unknown remote endpoint '%s'."), *OwnerContext.ToString(), *ConcertLoggerUtil::GetMessageString(ConcertContext), *ConcertContext.Message->ConcertEndpointId.ToString());
		break;

	default:
		checkf(false, TEXT("Unknown EMessageDiscardedReason!"));
		break;
	}

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(ConcertLoggerUtil::BuildLogMessage(ConcertContext.MessageType, ConcertContext.Message, ConcertContext.SenderConcertEndpointId, DestEndpoint, ConcertContext.UtcNow, Reason == EMessageDiscardedReason::AlreadyProcessed ? EConcertLogMessageAction::Duplicate : EConcertLogMessageAction::Discard));
}

void FConcertLogger::LogProcessEvent(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint)
{
	UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Event %s processed."), *OwnerContext.ToString(), *ConcertLoggerUtil::GetMessageString(ConcertContext));

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(ConcertLoggerUtil::BuildLogMessage(ConcertContext.MessageType, ConcertContext.Message, ConcertContext.SenderConcertEndpointId, DestEndpoint, ConcertContext.UtcNow, EConcertLogMessageAction::Process));
}

void FConcertLogger::LogProcessRequest(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint)
{
	UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Request %s processed."), *OwnerContext.ToString(), *ConcertLoggerUtil::GetMessageString(ConcertContext));

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(ConcertLoggerUtil::BuildLogMessage(ConcertContext.MessageType, ConcertContext.Message, ConcertContext.SenderConcertEndpointId, DestEndpoint, ConcertContext.UtcNow, EConcertLogMessageAction::Process));
}

void FConcertLogger::LogProcessResponse(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint) 
{
	const FConcertResponseData* ResponseMessage = ConcertContext.GetMessage<FConcertResponseData>();
	UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Response %s processed for request '%s'."), *OwnerContext.ToString(), *ConcertLoggerUtil::GetMessageString(ConcertContext), *ResponseMessage->RequestMessageId.ToString());

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(ConcertLoggerUtil::BuildLogMessage(ConcertContext.MessageType, ConcertContext.Message, ConcertContext.SenderConcertEndpointId, DestEndpoint, ConcertContext.UtcNow, EConcertLogMessageAction::Process));
}

void FConcertLogger::LogProcessAck(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint)
{
	const FConcertAckData* AckMessage = ConcertContext.GetMessage<FConcertAckData>();
	UE_LOG(LogConcert, VeryVerbose, TEXT("%s: Acknowledgement '%s' processed for message '%s'."), *OwnerContext.ToString(), *AckMessage->MessageId.ToString(), *AckMessage->SourceMessageId.ToString());

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(ConcertLoggerUtil::BuildLogMessage(ConcertContext.MessageType, ConcertContext.Message, ConcertContext.SenderConcertEndpointId, DestEndpoint, ConcertContext.UtcNow, EConcertLogMessageAction::Process));
}

void FConcertLogger::LogRemoteEndpointDiscovery(const FConcertMessageContext& ConcertContext, const FGuid& DestEndpoint)
{
	UE_LOG(LogConcert, Display, TEXT("%s: Remote endpoint '%s' discovered."), *OwnerContext.ToString(), *ConcertContext.SenderConcertEndpointId.ToString());

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(ConcertLoggerUtil::BuildLogMessage(ConcertContext.MessageType, ConcertContext.Message, ConcertContext.SenderConcertEndpointId, DestEndpoint, ConcertContext.UtcNow, EConcertLogMessageAction::EndpointDiscovery));
}

void FConcertLogger::LogRemoteEndpointTimeOut(const FGuid& EndpointId, const FDateTime& UtcNow)
{
	UE_LOG(LogConcert, Display, TEXT("%s: Remote endpoint '%s' timed-out."), *OwnerContext.ToString(), *EndpointId.ToString());

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(FConcertLog
		{
			GFrameCounter,
			FGuid(),
			0,
			FConcertMessageData::UnreliableChannelId,
			UtcNow,
			EConcertLogMessageAction::EndpointTimeOut,
			NAME_None,
			EndpointId,
			FGuid(),
			NAME_None,
			0
		}
	);
}

void FConcertLogger::LogRemoteEndpointClosure(const FGuid& EndpointId, const FDateTime& UtcNow)
{
	UE_LOG(LogConcert, Display, TEXT("%s: Remote endpoint '%s' closed by remote peer."), *OwnerContext.ToString(), *EndpointId.ToString());

	if (!IsLogging())
	{
		return;
	}

	LogQueue.Enqueue(FConcertLog
		{
			GFrameCounter,
			FGuid(),
			0,
			FConcertMessageData::UnreliableChannelId,
			UtcNow,
			EConcertLogMessageAction::EndpointClosure,
			NAME_None,
			EndpointId,
			FGuid(),
			NAME_None,
			0
		}
	);
}

void FConcertLogger::InternalStartLogging()
{
	const FString CSVFilename = FPaths::ProjectLogDir() / TEXT("Concert") / FString::Printf(TEXT("%s-%s-%s.csv"), FApp::GetProjectName(), *OwnerContext.EndpointFriendlyName, *FDateTime::Now().ToString());
	CSVArchive = TUniquePtr<FArchive>(IFileManager::Get().CreateFileWriter(*CSVFilename, EFileWrite::FILEWRITE_AllowRead));
	if (CSVArchive)
	{
		UTF8CHAR UTF8BOM[] = { 0xEF, 0xBB, 0xBF };
		CSVArchive->Serialize(&UTF8BOM, ARRAY_COUNT(UTF8BOM) * sizeof(UTF8CHAR));
	}
	LogHeader();
	bIsLogging = CSVArchive.IsValid();
}

void FConcertLogger::InternalStopLogging()
{
	InternalFlushLog();

	bIsLogging = false;
	CSVArchive.Reset();
}

void FConcertLogger::InternalFlushLog()
{
	if (!IsLogging())
	{
		return;
	}

	if (GIsSavingPackage || IsGarbageCollecting())
	{
		// Cannot process payload data when this is happening
		return;
	}

	// Process logs
	FConcertLog Log;
	while (LogQueue.Dequeue(Log))
	{
		LogEntry(Log);
	}
	if (CSVArchive.IsValid())
	{
		CSVArchive->Flush();
	}
}

void FConcertLogger::LogHeader()
{
	FString CSVHeader;
	for (TFieldIterator<const UProperty> PropertyIt(FConcertLog::StaticStruct(), EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); PropertyIt; ++PropertyIt)
	{
		if (PropertyIt->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		if (!CSVHeader.IsEmpty())
		{
			CSVHeader += TEXT(",");
		}

		FString PropertyName = PropertyIt->GetName();
		PropertyName.ReplaceInline(TEXT("\""), TEXT("\"\""));

		CSVHeader += TEXT("\"");
		CSVHeader += PropertyName;
		CSVHeader += TEXT("\"");
	}
	CSVHeader += LINE_TERMINATOR;

	if (CSVArchive.IsValid())
	{
		FTCHARToUTF8 CSVHeaderUTF8(*CSVHeader);
		CSVArchive->Serialize((UTF8CHAR*)CSVHeaderUTF8.Get(), CSVHeaderUTF8.Length() * sizeof(UTF8CHAR));
	}
}

void FConcertLogger::LogEntry(FConcertLog& Log)
{
	static const FName MessageOrderIndexPropertyName = TEXT("MessageOrderIndex");
	const bool bIsReliable = Log.ChannelId != FConcertMessageData::UnreliableChannelId;

	// Process the log payload into its string form (we do this now as it may not be safe to do it when the logging actually happens)
	if (!Log.SerializedPayload.PayloadTypeName.IsNone())
	{
		ConcertLoggerUtil::PopulateLogMessagePayload(Log.SerializedPayload, Log);
	}

	FString CSVRow;
	for (TFieldIterator<const UProperty> PropertyIt(FConcertLog::StaticStruct(), EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); PropertyIt; ++PropertyIt)
	{
		if (PropertyIt->HasAnyPropertyFlags(CPF_Transient))
		{
			continue;
		}

		if (!CSVRow.IsEmpty())
		{
			CSVRow += TEXT(",");
		}

		// Skip exporting the MessageOrderIndex value for unreliable messages as they add a lot of noise to the log
		const void* PropertyValuePtr = PropertyIt->ContainerPtrToValuePtr<void>(&Log);
		const bool bExportValue = bIsReliable || PropertyIt->GetFName() != MessageOrderIndexPropertyName;

		FString PropertyValue;
		if (bExportValue)
		{
			PropertyIt->ExportTextItem(PropertyValue, PropertyValuePtr, nullptr, nullptr, PPF_None);
		}
		PropertyValue.ReplaceInline(TEXT("\""), TEXT("\"\""));

		CSVRow += TEXT("\"");
		CSVRow += PropertyValue;
		CSVRow += TEXT("\"");
	}
	CSVRow += LINE_TERMINATOR;

	if (CSVArchive.IsValid())
	{
		FTCHARToUTF8 CSVRowUTF8(*CSVRow);
		CSVArchive->Serialize((UTF8CHAR*)CSVRowUTF8.Get(), CSVRowUTF8.Length() * sizeof(UTF8CHAR));
	}
}
