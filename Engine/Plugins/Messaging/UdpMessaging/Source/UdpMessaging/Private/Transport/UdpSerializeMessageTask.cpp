// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
	FJsonStructSerializerBackend Backend(Archive);
	FStructSerializer::Serialize(MessageContext->GetMessage(), *MessageContext->GetMessageTypeInfo(), Backend);
}

void SerializeMessageV11(FArchive& Archive, const TSharedRef<IMessageContext, ESPMode::ThreadSafe>& MessageContext)
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
	FCborStructSerializerBackend Backend(Archive);
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
		FArchive& Archive = SerializedMessage.Get();
		bool Serialized = true;
		switch (SerializedMessage->GetProtocolVersion())
		{
			case 10:
				UdpSerializeMessageTaskDetails::SerializeMessageV10(Archive, MessageContext);
				break;

			case 11:
				UdpSerializeMessageTaskDetails::SerializeMessageV11(Archive, MessageContext);
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
		else if (SerializedMessage->TotalSize() > UDP_MESSAGING_SEGMENT_SIZE * 65536)
		{
			UE_LOG(LogUdpMessaging, Error, TEXT("Serialized Message total size '%i' is over the allowed maximum '%i', discarding..."), SerializedMessage->TotalSize(), UDP_MESSAGING_SEGMENT_SIZE * 65536);
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
