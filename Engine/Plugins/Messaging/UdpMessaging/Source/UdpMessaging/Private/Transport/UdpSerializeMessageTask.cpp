// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Transport/UdpSerializeMessageTask.h"

#include "HAL/Event.h"
#include "IMessageContext.h"
#include "Backends/JsonStructSerializerBackend.h"
#include "Backends/CborStructSerializerBackend.h"
#include "StructSerializer.h"

#include "UdpMessagingPrivate.h"
#include "Transport/UdpSerializedMessage.h"

namespace UdpSerializeMessageTaskDetails
{

/** Serialization Routine for message using Protocol version 10 */
void SerializeMessageV10(FArchive& Archive, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& MessageContext)
{
	const FName& MessageType = MessageContext->GetMessageType();
	Archive << const_cast<FName&>(MessageType);

	const FMessageAddress& Sender = MessageContext->GetSender();
	Archive << const_cast<FMessageAddress&>(Sender);

	const TArray<FMessageAddress>& Recipients = MessageContext->GetRecipients();
	Archive << const_cast<TArray<FMessageAddress>&>(Recipients);

	EMessageScope Scope = MessageContext->GetScope();
	Archive << Scope;

	const FDateTime& TimeSent = MessageContext->GetTimeSent();
	Archive << const_cast<FDateTime&>(TimeSent);

	const FDateTime& Expiration = MessageContext->GetExpiration();
	Archive << const_cast<FDateTime&>(Expiration);

	int32 NumAnnotations = MessageContext->GetAnnotations().Num();
	Archive << NumAnnotations;

	for (const auto& AnnotationPair : MessageContext->GetAnnotations())
	{
		Archive << const_cast<FName&>(AnnotationPair.Key);
		Archive << const_cast<FString&>(AnnotationPair.Value);
	}

	// serialize message body
	FJsonStructSerializerBackend Backend(Archive, EStructSerializerBackendFlags::Legacy);
	FStructSerializer::Serialize(MessageContext->GetMessage(), *MessageContext->GetMessageTypeInfo(), Backend);
}

/** Serialization Routine for message using Protocol version 11 or 12 */
void SerializeMessageV11_12(FArchive& Archive, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& MessageContext, const EStructSerializerBackendFlags StructSerializerBackendFlags)
{
	const FName& MessageType = MessageContext->GetMessageType();
	Archive << const_cast<FName&>(MessageType);

	const FMessageAddress& Sender = MessageContext->GetSender();
	Archive << const_cast<FMessageAddress&>(Sender);

	const TArray<FMessageAddress>& Recipients = MessageContext->GetRecipients();
	Archive << const_cast<TArray<FMessageAddress>&>(Recipients);

	EMessageScope Scope = MessageContext->GetScope();
	Archive << Scope;

	EMessageFlags Flags = MessageContext->GetFlags();
	Archive << Flags;

	const FDateTime& TimeSent = MessageContext->GetTimeSent();
	Archive << const_cast<FDateTime&>(TimeSent);

	const FDateTime& Expiration = MessageContext->GetExpiration();
	Archive << const_cast<FDateTime&>(Expiration);

	int32 NumAnnotations = MessageContext->GetAnnotations().Num();
	Archive << NumAnnotations;

	for (const auto& AnnotationPair : MessageContext->GetAnnotations())
	{
		Archive << const_cast<FName&>(AnnotationPair.Key);
		Archive << const_cast<FString&>(AnnotationPair.Value);
	}

	// Message Wire Format Id
	uint8 MessageFormat = (uint8)EUdpMessageFormat::Cbor;
	Archive << MessageFormat;
	
	// serialize message body with cbor
	FCborStructSerializerBackend Backend(Archive, StructSerializerBackendFlags);
	FStructSerializer::Serialize(MessageContext->GetMessage(), *MessageContext->GetMessageTypeInfo(), Backend);
}

} // namespace UdpSerializeMessageTaskDetails


/* FUdpSerializeMessageTask interface
 *****************************************************************************/

void FUdpSerializeMessageTask::DoTask(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	if (MessageContext->IsValid())
	{
		// Note that some complex values are serialized manually here, so that we can ensure
		// a consistent wire format, if their implementations change. This allows us to sanity
		// check the values during deserialization. @see FUdpDeserializeMessage::Deserialize()

		// serialize context depending on supported protocol version
		int64 ProtocolMaxSegmentSize = UDP_MESSAGING_SEGMENT_SIZE * (int64)UINT16_MAX;
		FArchive& Archive = SerializedMessage.Get();
		bool Serialized = true;
		switch (SerializedMessage->GetProtocolVersion())
		{
			case 10:
				ProtocolMaxSegmentSize = UDP_MESSAGING_SEGMENT_SIZE * (int64)UINT16_MAX;
				UdpSerializeMessageTaskDetails::SerializeMessageV10(Archive, MessageContext);
				break;

			case 11:
				ProtocolMaxSegmentSize = UDP_MESSAGING_SEGMENT_SIZE * (int64)UINT16_MAX;
				UdpSerializeMessageTaskDetails::SerializeMessageV11_12(Archive, MessageContext, EStructSerializerBackendFlags::Legacy);
				break;

			case 12:
				ProtocolMaxSegmentSize = UDP_MESSAGING_SEGMENT_SIZE * (int64)INT32_MAX;
				UdpSerializeMessageTaskDetails::SerializeMessageV11_12(Archive, MessageContext, EStructSerializerBackendFlags::Default);
				break;

			default:
				// Unsupported protocol version
				Serialized = false;
				break;
		}

		// if the message wasn't serialized, flag it invalid
		if (!Serialized)
		{
			UE_LOG(LogUdpMessaging, Error, TEXT("Unsupported Protocol Version message tasked for serialization, discarding..."));
			SerializedMessage->UpdateState(EUdpSerializedMessageState::Invalid);
		}
		// Once serialized if the size of the message is bigger than the maximum allow mark it as invalid and log an error
		else if (SerializedMessage->TotalSize() > ProtocolMaxSegmentSize)
		{
			UE_LOG(LogUdpMessaging, Error, TEXT("Serialized Message total size '%i' is over the allowed maximum '%i', discarding..."), SerializedMessage->TotalSize(), ProtocolMaxSegmentSize);
			SerializedMessage->UpdateState(EUdpSerializedMessageState::Invalid);
		}
		else
		{
			SerializedMessage->UpdateState(EUdpSerializedMessageState::Complete);
		}
	}
	else
	{
		SerializedMessage->UpdateState(EUdpSerializedMessageState::Invalid);
	}

	// signal task completion
	TSharedPtr<FEvent, ESPMode::ThreadSafe> CompletionEvent = CompletionEventPtr.Pin();

	if (CompletionEvent.IsValid())
	{
		CompletionEvent->Trigger();
	}
}


ENamedThreads::Type FUdpSerializeMessageTask::GetDesiredThread()
{
	return ENamedThreads::AnyThread;
}


TStatId FUdpSerializeMessageTask::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FUdpSerializeMessageTask, STATGROUP_TaskGraphTasks);
}


ESubsequentsMode::Type FUdpSerializeMessageTask::GetSubsequentsMode() 
{ 
	return ESubsequentsMode::FireAndForget; 
}
