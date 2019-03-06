// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Evaluation/MovieSceneEventTemplate.h"
#include "Tracks/MovieSceneEventTrack.h"
#include "MovieSceneSequence.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "EngineGlobals.h"
#include "MovieScene.h"
#include "Evaluation/MovieSceneEvaluation.h"
#include "IMovieScenePlayer.h"
#include "Algo/Accumulate.h"
#include "Engine/LevelScriptActor.h"
#include "Logging/TokenizedMessage.h"
#include "Logging/MessageLog.h"
#include "Misc/UObjectToken.h"

#define LOCTEXT_NAMESPACE "MovieSceneEventTemplate"

DECLARE_CYCLE_STAT(TEXT("Event Track Token Execute"), MovieSceneEval_EventTrack_TokenExecute, STATGROUP_MovieSceneEval);

struct FMovieSceneEventData
{
	FMovieSceneEventData(const FEventPayload& InPayload, float InGlobalPosition) : Payload(InPayload), GlobalPosition(InGlobalPosition) {}

	FEventPayload Payload;
	float GlobalPosition;
};

/** A movie scene execution token that stores a specific transform, and an operand */
struct FEventTrackExecutionToken
	: IMovieSceneExecutionToken
{
	FEventTrackExecutionToken(TArray<FMovieSceneEventData> InEvents, const TArray<FMovieSceneObjectBindingID>& InEventReceivers) : Events(MoveTemp(InEvents)), EventReceivers(InEventReceivers) {}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_EventTrack_TokenExecute)
		
		TArray<float> PerformanceCaptureEventPositions;

		// Resolve event contexts to trigger the event on
		TArray<UObject*> EventContexts;

		// If we have specified event receivers, use those
		if (EventReceivers.Num())
		{
			EventContexts.Reserve(EventReceivers.Num());
			for (FMovieSceneObjectBindingID ID : EventReceivers)
			{
				// Ensure that this ID is resolvable from the root, based on the current local sequence ID
				ID = ID.ResolveLocalToRoot(Operand.SequenceID, Player.GetEvaluationTemplate().GetHierarchy());

				// Lookup the object(s) specified by ID in the player
				for (TWeakObjectPtr<> WeakEventContext : Player.FindBoundObjects(ID.GetGuid(), ID.GetSequenceID()))
				{
					if (UObject* EventContext = WeakEventContext.Get())
					{
						EventContexts.Add(EventContext);
					}
				}
			}
		}
		else
		{
			// If we haven't specified event receivers, use the default set defined on the player
			EventContexts = Player.GetEventContexts();
		}

		for (UObject* EventContextObject : EventContexts)
		{
			if (!EventContextObject)
			{
				continue;
			}

			for (FMovieSceneEventData& Event : Events)
			{
#if !UE_BUILD_SHIPPING
				if (Event.Payload.EventName == NAME_PerformanceCapture)
				{
					PerformanceCaptureEventPositions.Add(Event.GlobalPosition);
				}
#endif
				TriggerEvent(Event, *EventContextObject, Player);
			}
		}

#if !UE_BUILD_SHIPPING
		if (PerformanceCaptureEventPositions.Num())
		{
			UObject* PlaybackContext = Player.GetPlaybackContext();
			UWorld* World = PlaybackContext ? PlaybackContext->GetWorld() : nullptr;
			if (World)
			{
				FString LevelSequenceName = Player.GetEvaluationTemplate().GetSequence(MovieSceneSequenceID::Root)->GetName();
			
				for (float EventPosition : PerformanceCaptureEventPositions)
				{
					GEngine->PerformanceCapture(World, World->GetName(), LevelSequenceName, EventPosition);
				}
			}
		}
#endif	// UE_BUILD_SHIPPING
	}

	void TriggerEvent(FMovieSceneEventData& Event, UObject& EventContextObject, IMovieScenePlayer& Player)
	{
		UFunction* EventFunction = EventContextObject.FindFunction(Event.Payload.EventName);

		if (EventFunction == nullptr)
		{
			// Don't want to log out a warning for every event context.
			return;
		}
		else
		{
			FStructOnScope ParameterStruct(nullptr);
			Event.Payload.Parameters.GetInstance(ParameterStruct);

			uint8* Parameters = ParameterStruct.GetStructMemory();

			const UStruct* Struct = ParameterStruct.GetStruct();
			if (EventFunction->ReturnValueOffset != MAX_uint16)
			{
				UE_LOG(LogMovieScene, Warning, TEXT("Sequencer Event Track: Cannot trigger events that return values (for event '%s')."), *Event.Payload.EventName.ToString());
				return;
			}
			else
			{
				TFieldIterator<UProperty> ParamIt(EventFunction);
				TFieldIterator<UProperty> ParamInstanceIt(Struct);
				for (int32 NumParams = 0; ParamIt || ParamInstanceIt; ++NumParams, ++ParamIt, ++ParamInstanceIt)
				{
					if (!ParamInstanceIt)
					{
						UE_LOG(LogMovieScene, Warning, TEXT("Sequencer Event Track: Parameter count mismatch for event '%s'. Required parameter of type '%s' at index '%d'."), *Event.Payload.EventName.ToString(), *ParamIt->GetName(), NumParams);
						return;
					}
					else if (!ParamIt)
					{
						// Mismatch (too many params)
						UE_LOG(LogMovieScene, Warning, TEXT("Sequencer Event Track: Parameter count mismatch for event '%s'. Parameter struct contains too many parameters ('%s' is superfluous at index '%d'."), *Event.Payload.EventName.ToString(), *ParamInstanceIt->GetName(), NumParams);
						return;
					}
					else if (!ParamInstanceIt->SameType(*ParamIt) || ParamInstanceIt->GetOffset_ForUFunction() != ParamIt->GetOffset_ForUFunction() || ParamInstanceIt->GetSize() != ParamIt->GetSize())
					{
						UE_LOG(LogMovieScene, Warning, TEXT("Sequencer Event Track: Parameter type mismatch for event '%s' ('%s' != '%s')."),
							*Event.Payload.EventName.ToString(),
							*ParamInstanceIt->GetClass()->GetName(),
							*ParamIt->GetClass()->GetName()
						);
						return;
					}
				}
			}

			// Technically, anything bound to the event could mutate the parameter payload,
			// but we're going to treat that as misuse, rather than copy the parameters each time
			EventContextObject.ProcessEvent(EventFunction, Parameters);
		}
	}

	TArray<FMovieSceneEventData> Events;
	TArray<FMovieSceneObjectBindingID, TInlineAllocator<2>> EventReceivers;
};

struct FEventTriggerExecutionToken
	: IMovieSceneExecutionToken
{
	FEventTriggerExecutionToken(TArray<FName> InEvents, const TArray<FMovieSceneObjectBindingID>& InEventReceivers)
		: Events(MoveTemp(InEvents)), EventReceivers(InEventReceivers)
	{}

	/** Execute this token, operating on all objects referenced by 'Operand' */
	virtual void Execute(const FMovieSceneContext& Context, const FMovieSceneEvaluationOperand& Operand, FPersistentEvaluationData& PersistentData, IMovieScenePlayer& Player) override
	{
		MOVIESCENE_DETAILED_SCOPE_CYCLE_COUNTER(MovieSceneEval_EventTrack_TokenExecute)

		UObject* DirectorInstance = Player.GetEvaluationTemplate().GetOrCreateDirectorInstance(Operand.SequenceID, Player);
		if (!DirectorInstance)
		{
#if !NO_LOGGING
			UE_LOG(LogMovieScene, Warning, TEXT("Failed to trigger the following events because no director instance was available: %s."), *GenerateEventListString());
#endif			
			return;
		}

		// Resolve event contexts to trigger the event on
		TArray<UObject*> EventContexts;

		// If the event track resides within an object binding, add those to the event contexts
		if (Operand.ObjectBindingID.IsValid())
		{
			for (TWeakObjectPtr<> WeakEventContext : Player.FindBoundObjects(Operand))
			{
				if (UObject* EventContext = WeakEventContext.Get())
				{
					EventContexts.Add(EventContext);
				}
			}
		}
		// If we have specified event receivers
		else if (EventReceivers.Num())
		{
			EventContexts.Reserve(EventReceivers.Num());
			for (FMovieSceneObjectBindingID ID : EventReceivers)
			{
				// Ensure that this ID is resolvable from the root, based on the current local sequence ID
				ID = ID.ResolveLocalToRoot(Operand.SequenceID, Player.GetEvaluationTemplate().GetHierarchy());

				// Lookup the object(s) specified by ID in the player
				for (TWeakObjectPtr<> WeakEventContext : Player.FindBoundObjects(ID.GetGuid(), ID.GetSequenceID()))
				{
					if (UObject* EventContext = WeakEventContext.Get())
					{
						EventContexts.Add(EventContext);
					}
				}
			}
		}
		// If we haven't specified event receivers, use the default set defined on the player
		else
		{
			EventContexts = Player.GetEventContexts();
		}

#if WITH_EDITOR
		const static FName NAME_CallInEditor(TEXT("CallInEditor"));

		UWorld* World = DirectorInstance->GetWorld();
		bool bIsGameWorld = World && World->IsGameWorld();
#endif // WITH_EDITOR

		for (FName EventName : Events)
		{
			if (EventName == NAME_None)
			{
				continue;
			}

			UFunction* Function = DirectorInstance->FindFunction(EventName);
			// Event must have only a single object parameter, and the director instance must be an implementation of the function's class
			if (!Function)
			{
				FMessageLog("PIE").Warning()
					->AddToken(FTextToken::Create(FText::Format(LOCTEXT("LevelBP_MissingEvent_Error1", "Failed to trigger event '{0}' for"), FText::FromName(EventName))))
					->AddToken(FUObjectToken::Create(Player.GetEvaluationTemplate().GetSequence(Operand.SequenceID)))
					->AddToken(FTextToken::Create(LOCTEXT("LevelBP_MissingEvent_Error2", "because the function does not exist on the director instance.")));
				continue;
			}
#if WITH_EDITOR
			else if (!bIsGameWorld && !Function->HasMetaData(NAME_CallInEditor))
			{
				UE_LOG(LogMovieScene, Verbose, TEXT("Refusing to trigger event '%s' in editor world when 'Call in Editor' is false."), *EventName.ToString());
				continue;
			}
#endif // WITH_EDITOR

			if (Function->NumParms == 0)
			{
				UE_LOG(LogMovieScene, VeryVerbose, TEXT("Triggering event '%s'."), *EventName.ToString());
				DirectorInstance->ProcessEvent(Function, nullptr);
			}
			else if (Function->NumParms == 1 && Function->PropertyLink && (Function->PropertyLink->GetPropertyFlags() & CPF_ReferenceParm) == 0)
			{
				const int32 NumLevelScripts = Algo::Accumulate(EventContexts, 0, 
					[](int32 Count, UObject* Obj)
					{
						return Obj && Obj->IsA<ALevelScriptActor>() ? Count + 1 : Count;
					}
				);

				// Never pass through level script actors to event endpoints on non-interface pins.
				if (NumLevelScripts > 0 && NumLevelScripts == EventContexts.Num() && !Function->PropertyLink->IsA<UInterfaceProperty>())
				{
					FMessageLog("PIE").Warning()
						->AddToken(FTextToken::Create(LOCTEXT("LevelBP_ObjectPin_Error1", "Failed to trigger event")))
						->AddToken(FUObjectToken::Create(Function))
						->AddToken(FTextToken::Create(LOCTEXT("LevelBP_ObjectPin_Error2", "within")))
						->AddToken(FUObjectToken::Create(Player.GetEvaluationTemplate().GetSequence(Operand.SequenceID)))
						->AddToken(FTextToken::Create(LOCTEXT("LevelBP_ObjectPin_Error3", "because only Interface pins are supported for master tracks within Level Sequences. Please remove the pin, or change it to an interface that is implemented on the desired level blueprint.")));
					continue;
				}

				for (UObject* EventContextObject : EventContexts)
				{
					TriggerEvent(DirectorInstance, Function, EventContextObject, Player, Operand.SequenceID);
				}
			}
			else
			{
				FMessageLog("PIE").Warning()
					->AddToken(FTextToken::Create(LOCTEXT("LevelBP_InvalidEvent_Error1", "Failed to trigger event")))
					->AddToken(FUObjectToken::Create(Function))
					->AddToken(FTextToken::Create(LOCTEXT("LevelBP_InvalidEvent_Error2", "within")))
					->AddToken(FUObjectToken::Create(Player.GetEvaluationTemplate().GetSequence(Operand.SequenceID)))
					->AddToken(FTextToken::Create(LOCTEXT("LevelBP_InvalidEvent_Error3", "because its signature is not compatible. Function signatures must have either 0 or 1 (non-ref) parameters.")));
			}
		}
	}

	void TriggerEvent(UObject* DirectorInstance, UFunction* Function, UObject* ObjectParamValue, IMovieScenePlayer& Player, FMovieSceneSequenceID SequenceID)
	{
		// We know by now that the parameter type is compatible because it wouldn't be added to the array if not
		if (UObjectProperty* ObjectParameter = Cast<UObjectProperty>(Function->PropertyLink))
		{
			if (!ObjectParameter->PropertyClass || ObjectParamValue->IsA(ObjectParameter->PropertyClass))
			{
				DirectorInstance->ProcessEvent(Function, &ObjectParamValue);
				return;
			}


			UE_LOG(LogMovieScene, VeryVerbose, TEXT("Failed to trigger event '%s' with object '%s' because it is not the correct type. Function expects a '%s' but target object is a '%s'."),
				*Function->GetName(),
				*ObjectParamValue->GetName(),
				*ObjectParameter->PropertyClass->GetName(),
				*ObjectParamValue->GetClass()->GetName()
				);

			return;
		}

		if (UInterfaceProperty* InterfaceParameter = Cast<UInterfaceProperty>(Function->PropertyLink))
		{
			if (ObjectParamValue->GetClass()->ImplementsInterface(InterfaceParameter->InterfaceClass))
			{
				DirectorInstance->ProcessEvent(Function, &ObjectParamValue);
				return;
			}


			UE_LOG(LogMovieScene, VeryVerbose, TEXT("Failed to trigger event '%s' with object '%s' because it does not implement the necessary interface. Function expects a '%s'."),
				*Function->GetName(),
				*ObjectParamValue->GetName(),
				*InterfaceParameter->InterfaceClass->GetName()
				);

			return;
		}

		FMessageLog("PIE").Warning()
			->AddToken(FTextToken::Create(LOCTEXT("LevelBP_InvalidObjectEvent_Error1", "Failed to trigger event")))
			->AddToken(FUObjectToken::Create(Function))
			->AddToken(FTextToken::Create(LOCTEXT("LevelBP_InvalidObjectEvent_Error2", "within")))
			->AddToken(FUObjectToken::Create(Player.GetEvaluationTemplate().GetSequence(SequenceID)))
			->AddToken(FTextToken::Create(FText::Format(LOCTEXT("LevelBP_InvalidObjectEvent_Error3", "because its signature is not compatible. Function expects a '%s' parameter, but only object and interface parameters are supported."), FText::FromName(Function->PropertyLink->GetClass()->GetFName()))));
	}

#if !NO_LOGGING
	FString GenerateEventListString() const
	{
		return Algo::Accumulate(Events, FString(), [](FString&& InString, FName Event){
			if (InString.Len() > 0)
			{
				InString += TEXT(", ");
			}
			return InString + Event.ToString();
		});
	}
#endif

	TArray<FName> Events;
	TArray<FMovieSceneObjectBindingID, TInlineAllocator<2>> EventReceivers;
};


FMovieSceneEventTemplateBase::FMovieSceneEventTemplateBase(const UMovieSceneEventTrack& Track)
	: EventReceivers(Track.EventReceivers)
	, bFireEventsWhenForwards(Track.bFireEventsWhenForwards)
	, bFireEventsWhenBackwards(Track.bFireEventsWhenBackwards)
{
}

FMovieSceneEventSectionTemplate::FMovieSceneEventSectionTemplate(const UMovieSceneEventSection& Section, const UMovieSceneEventTrack& Track)
	: FMovieSceneEventTemplateBase(Track)
	, EventData(Section.GetEventData())
{
}

void FMovieSceneEventSectionTemplate::EvaluateSwept(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const TRange<FFrameNumber>& SweptRange, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	// Don't allow events to fire when playback is in a stopped state. This can occur when stopping 
	// playback and returning the current position to the start of playback. It's not desireable to have 
	// all the events from the last playback position to the start of playback be fired.
	if (Context.GetStatus() == EMovieScenePlayerStatus::Stopped || Context.IsSilent())
	{
		return;
	}

	const bool bBackwards = Context.GetDirection() == EPlayDirection::Backwards;

	if ((!bBackwards && !bFireEventsWhenForwards) ||
		(bBackwards && !bFireEventsWhenBackwards))
	{
		return;
	}

	TArray<FMovieSceneEventData> Events;

	TArrayView<const FFrameNumber>  KeyTimes   = EventData.GetKeyTimes();
	TArrayView<const FEventPayload> KeyValues  = EventData.GetKeyValues();

	const int32 First = bBackwards ? KeyTimes.Num() - 1 : 0;
	const int32 Last = bBackwards ? 0 : KeyTimes.Num() - 1;
	const int32 Inc = bBackwards ? -1 : 1;

	const float PositionInSeconds = Context.GetTime() * Context.GetRootToSequenceTransform().Inverse() / Context.GetFrameRate();

	if (bBackwards)
	{
		// Trigger events backwards
		for (int32 KeyIndex = KeyTimes.Num() - 1; KeyIndex >= 0; --KeyIndex)
		{
			FFrameNumber Time = KeyTimes[KeyIndex];
			if (SweptRange.Contains(Time))
			{
				Events.Add(FMovieSceneEventData(KeyValues[KeyIndex], PositionInSeconds));
			}
		}
	}
	// Trigger events forwards
	else for (int32 KeyIndex = 0; KeyIndex < KeyTimes.Num(); ++KeyIndex)
	{
		FFrameNumber Time = KeyTimes[KeyIndex];
		if (SweptRange.Contains(Time))
		{
			Events.Add(FMovieSceneEventData(KeyValues[KeyIndex], PositionInSeconds));
		}
	}


	if (Events.Num())
	{
		ExecutionTokens.Add(FEventTrackExecutionToken(MoveTemp(Events), EventReceivers));
	}
}



FMovieSceneEventTriggerTemplate::FMovieSceneEventTriggerTemplate(const UMovieSceneEventTriggerSection& Section, const UMovieSceneEventTrack& Track)
	: FMovieSceneEventTemplateBase(Track)
{
	TMovieSceneChannelData<const FMovieSceneEvent> EventData = Section.EventChannel.GetData();
	TArrayView<const FFrameNumber>     Times  = EventData.GetTimes();
	TArrayView<const FMovieSceneEvent> Events = EventData.GetValues();

	EventTimes.Reserve(Times.Num());
	EventFunctions.Reserve(Times.Num());

	for (int32 Index = 0; Index < Times.Num(); ++Index)
	{
		EventTimes.Add(Times[Index]);
		EventFunctions.Add(Events[Index].FunctionName);
	}
}

void FMovieSceneEventTriggerTemplate::EvaluateSwept(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const TRange<FFrameNumber>& SweptRange, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	// Don't allow events to fire when playback is in a stopped state. This can occur when stopping 
	// playback and returning the current position to the start of playback. It's not desireable to have 
	// all the events from the last playback position to the start of playback be fired.
	if (Context.GetStatus() == EMovieScenePlayerStatus::Stopped || Context.IsSilent())
	{
		return;
	}

	const bool bBackwards = Context.GetDirection() == EPlayDirection::Backwards;

	if ((!bBackwards && !bFireEventsWhenForwards) ||
		(bBackwards && !bFireEventsWhenBackwards))
	{
		return;
	}

	TArray<FName> Events;

	const int32 First = bBackwards ? EventTimes.Num() - 1 : 0;
	const int32 Last = bBackwards ? 0 : EventTimes.Num() - 1;
	const int32 Inc = bBackwards ? -1 : 1;

	const float PositionInSeconds = Context.GetTime() * Context.GetRootToSequenceTransform().Inverse() / Context.GetFrameRate();

	if (bBackwards)
	{
		// Trigger events backwards
		for (int32 KeyIndex = EventTimes.Num() - 1; KeyIndex >= 0; --KeyIndex)
		{
			FFrameNumber Time = EventTimes[KeyIndex];
			if (SweptRange.Contains(Time))
			{
				Events.Add(EventFunctions[KeyIndex]);
			}
		}
	}
	// Trigger events forwards
	else for (int32 KeyIndex = 0; KeyIndex < EventTimes.Num(); ++KeyIndex)
	{
		FFrameNumber Time = EventTimes[KeyIndex];
		if (SweptRange.Contains(Time))
		{
			Events.Add(EventFunctions[KeyIndex]);
		}
	}


	if (Events.Num())
	{
		ExecutionTokens.Add(FEventTriggerExecutionToken(MoveTemp(Events), EventReceivers));
	}
}



FMovieSceneEventRepeaterTemplate::FMovieSceneEventRepeaterTemplate(const UMovieSceneEventRepeaterSection& Section, const UMovieSceneEventTrack& Track)
	: FMovieSceneEventTemplateBase(Track)
	, EventToTrigger(Section.Event.FunctionName)
{
}

void FMovieSceneEventRepeaterTemplate::EvaluateSwept(const FMovieSceneEvaluationOperand& Operand, const FMovieSceneContext& Context, const TRange<FFrameNumber>& SweptRange, const FPersistentEvaluationData& PersistentData, FMovieSceneExecutionTokens& ExecutionTokens) const
{
	const bool bBackwards = Context.GetDirection() == EPlayDirection::Backwards;
	FFrameNumber CurrentFrame = bBackwards ? Context.GetTime().CeilToFrame() : Context.GetTime().FloorToFrame();

	// Don't allow events to fire when playback is in a stopped state. This can occur when stopping 
	// playback and returning the current position to the start of playback. It's not desireable to have 
	// all the events from the last playback position to the start of playback be fired.
	if (!SweptRange.Contains(CurrentFrame) || Context.GetStatus() == EMovieScenePlayerStatus::Stopped || Context.IsSilent())
	{
		return;
	}

	
	if ((!bBackwards && bFireEventsWhenForwards) || (bBackwards && bFireEventsWhenBackwards))
	{
		TArray<FName> Events = { EventToTrigger };
		ExecutionTokens.Add(FEventTriggerExecutionToken(MoveTemp(Events), EventReceivers));
	}
}

#undef LOCTEXT_NAMESPACE