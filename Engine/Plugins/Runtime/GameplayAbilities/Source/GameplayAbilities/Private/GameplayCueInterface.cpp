// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "GameplayCueInterface.h"
#include "AbilitySystemStats.h"
#include "GameplayTagsModule.h"
#include "AbilitySystemComponent.h"
#include "GameplayCueSet.h"


namespace GameplayCueInterfacePrivate
{
	struct FCueNameAndUFunction
	{
		FGameplayTag Tag;
		UFunction* Func;
	};
	typedef TMap<FGameplayTag, TArray<FCueNameAndUFunction> > FGameplayCueTagFunctionList;
	static TMap<FObjectKey, FGameplayCueTagFunctionList > PerClassGameplayTagToFunctionMap;
}


UGameplayCueInterface::UGameplayCueInterface(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
}

void IGameplayCueInterface::DispatchBlueprintCustomHandler(AActor* Actor, UFunction* Func, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	GameplayCueInterface_eventBlueprintCustomHandler_Parms Parms;
	Parms.EventType = EventType;
	Parms.Parameters = Parameters;

	Actor->ProcessEvent(Func, &Parms);
}

void IGameplayCueInterface::ClearTagToFunctionMap()
{
	GameplayCueInterfacePrivate::PerClassGameplayTagToFunctionMap.Empty();
}

void IGameplayCueInterface::HandleGameplayCues(AActor *Self, const FGameplayTagContainer& GameplayCueTags, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	for (auto TagIt = GameplayCueTags.CreateConstIterator(); TagIt; ++TagIt)
	{
		HandleGameplayCue(Self, *TagIt, EventType, Parameters);
	}
}

bool IGameplayCueInterface::ShouldAcceptGameplayCue(AActor *Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	return true;
}

void IGameplayCueInterface::HandleGameplayCue(AActor *Self, FGameplayTag GameplayCueTag, EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	SCOPE_CYCLE_COUNTER(STAT_GameplayCueInterface_HandleGameplayCue);

	// Look up a custom function for this gameplay tag. 
	UClass* Class = Self->GetClass();
	FGameplayTagContainer TagAndParentsContainer = GameplayCueTag.GetGameplayTagParents();

	Parameters.OriginalTag = GameplayCueTag;

	//Find entry for the class
	FObjectKey ClassObjectKey(Class);
	GameplayCueInterfacePrivate::FGameplayCueTagFunctionList& GameplayTagFunctionList = GameplayCueInterfacePrivate::PerClassGameplayTagToFunctionMap.FindOrAdd(ClassObjectKey);
	TArray<GameplayCueInterfacePrivate::FCueNameAndUFunction>* FunctionList = GameplayTagFunctionList.Find(GameplayCueTag);
	if (FunctionList == NULL)
	{
		//generate new function list
		FunctionList = &GameplayTagFunctionList.Add(GameplayCueTag);

		for (auto InnerTagIt = TagAndParentsContainer.CreateConstIterator(); InnerTagIt; ++InnerTagIt)
		{
			UFunction* Func = NULL;
			FName CueName = InnerTagIt->GetTagName();

			Func = Class->FindFunctionByName(CueName, EIncludeSuperFlag::IncludeSuper);
			// If the handler calls ForwardGameplayCueToParent, keep calling functions until one consumes the cue and doesn't forward it
			while (Func)
			{
				GameplayCueInterfacePrivate::FCueNameAndUFunction NewCueFunctionPair;
				NewCueFunctionPair.Tag = *InnerTagIt;
				NewCueFunctionPair.Func = Func;
				FunctionList->Add(NewCueFunctionPair);

				Func = Func->GetSuperFunction();
			}

			// Native functions cant be named with ".", so look for them with _. 
			FName NativeCueFuncName = *CueName.ToString().Replace(TEXT("."), TEXT("_"));
			Func = Class->FindFunctionByName(NativeCueFuncName, EIncludeSuperFlag::IncludeSuper);

			while (Func)
			{
				GameplayCueInterfacePrivate::FCueNameAndUFunction NewCueFunctionPair;
				NewCueFunctionPair.Tag = *InnerTagIt;
				NewCueFunctionPair.Func = Func;
				FunctionList->Add(NewCueFunctionPair);

				Func = Func->GetSuperFunction();
			}
		}
	}

	//Iterate through all functions in the list until we should no longer continue
	check(FunctionList);
		
	bool bShouldContinue = true;
	for (int32 FunctionIndex = 0; bShouldContinue && (FunctionIndex < FunctionList->Num()); ++FunctionIndex)
	{
		GameplayCueInterfacePrivate::FCueNameAndUFunction& CueFunctionPair = FunctionList->GetData()[FunctionIndex];
		UFunction* Func = CueFunctionPair.Func;
		Parameters.MatchedTagName = CueFunctionPair.Tag;

		// Reset the forward parameter now, so we can check it after function
		bForwardToParent = false;
		IGameplayCueInterface::DispatchBlueprintCustomHandler(Self, Func, EventType, Parameters);

		bShouldContinue = bForwardToParent;
	}

	if (bShouldContinue)
	{
		TArray<UGameplayCueSet*> Sets;
		GetGameplayCueSets(Sets);
		for (UGameplayCueSet* Set : Sets)
		{
			bShouldContinue = Set->HandleGameplayCue(Self, GameplayCueTag, EventType, Parameters);
			if (!bShouldContinue)
			{
				break;
			}
		}
	}

	if (bShouldContinue)
	{
		Parameters.MatchedTagName = GameplayCueTag;
		GameplayCueDefaultHandler(EventType, Parameters);
	}
}

void IGameplayCueInterface::GameplayCueDefaultHandler(EGameplayCueEvent::Type EventType, FGameplayCueParameters Parameters)
{
	// No default handler, subclasses can implement
}

void IGameplayCueInterface::ForwardGameplayCueToParent()
{
	// Consumed by HandleGameplayCue
	bForwardToParent = true;
}

void FActiveGameplayCue::PreReplicatedRemove(const struct FActiveGameplayCueContainer &InArray)
{
	if (!InArray.Owner)
	{
		return;
	}

	// We don't check the PredictionKey here like we do in PostReplicatedAdd. PredictionKey tells us
	// if we were predictely created, but this doesn't mean we will predictively remove ourselves.
	if (bPredictivelyRemoved == false)
	{
		// If predicted ignore the add/remove
		InArray.Owner->UpdateTagMap(GameplayCueTag, -1);
		InArray.Owner->InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Removed, Parameters);
	}
}

void FActiveGameplayCue::PostReplicatedAdd(const struct FActiveGameplayCueContainer &InArray)
{
	if (!InArray.Owner)
	{
		return;
	}

	InArray.Owner->UpdateTagMap(GameplayCueTag, 1);

	if (PredictionKey.IsLocalClientKey() == false)
	{
		// If predicted ignore the add/remove
		InArray.Owner->InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::WhileActive, Parameters);
	}
}

FString FActiveGameplayCue::GetDebugString()
{
	return FString::Printf(TEXT("(%s / %s"), *GameplayCueTag.ToString(), *PredictionKey.ToString());
}

void FActiveGameplayCueContainer::AddCue(const FGameplayTag& Tag, const FPredictionKey& PredictionKey, const FGameplayCueParameters& Parameters)
{
	if (!Owner)
	{
		return;
	}

	UWorld* World = Owner->GetWorld();

	// Store the prediction key so the client can investigate it
	FActiveGameplayCue&	NewCue = GameplayCues[GameplayCues.AddDefaulted()];
	NewCue.GameplayCueTag = Tag;
	NewCue.PredictionKey = PredictionKey;
	NewCue.Parameters = Parameters;
	MarkItemDirty(NewCue);
	
	Owner->UpdateTagMap(Tag, 1);
}

void FActiveGameplayCueContainer::RemoveCue(const FGameplayTag& Tag)
{
	if (!Owner)
	{
		return;
	}

	for (int32 idx=0; idx < GameplayCues.Num(); ++idx)
	{
		FActiveGameplayCue& Cue = GameplayCues[idx];

		if (Cue.GameplayCueTag == Tag)
		{
			GameplayCues.RemoveAt(idx);
			MarkArrayDirty();
			Owner->UpdateTagMap(Tag, -1);
			return;
		}
	}

}

void FActiveGameplayCueContainer::RemoveAllCues()
{
	if (!Owner)
	{
		return;
	}

	for (int32 idx=0; idx < GameplayCues.Num(); ++idx)
	{
		FActiveGameplayCue& Cue = GameplayCues[idx];
		Owner->UpdateTagMap(Cue.GameplayCueTag, -1);
		Owner->InvokeGameplayCueEvent(Cue.GameplayCueTag, EGameplayCueEvent::Removed, Cue.Parameters);
	}
}

void FActiveGameplayCueContainer::PredictiveRemove(const FGameplayTag& Tag)
{
	if (!Owner)
	{
		return;
	}
	

	// Predictive remove: we are predicting the removal of a replicated cue
	// (We are not predicting the removal of a predictive cue. The predictive cue will be implicitly removed when the prediction key catched up)
	for (int32 idx=0; idx < GameplayCues.Num(); ++idx)
	{
		// "Which" cue we predictively remove is only based on the tag and not already being predictively removed.
		// Since there are no handles/identifies for the items in this container, we just go with the first.
		FActiveGameplayCue& Cue = GameplayCues[idx];
		if (Cue.GameplayCueTag == Tag && !Cue.bPredictivelyRemoved)
		{
			Cue.bPredictivelyRemoved = true;
			Owner->UpdateTagMap(Tag, -1);
			Owner->InvokeGameplayCueEvent(Tag, EGameplayCueEvent::Removed, Cue.Parameters);	
			return;
		}
	}
}

void FActiveGameplayCueContainer::PredictiveAdd(const FGameplayTag& Tag, FPredictionKey& PredictionKey)
{
	if (!Owner)
	{
		return;
	}

	Owner->UpdateTagMap(Tag, 1);	
	PredictionKey.NewRejectOrCaughtUpDelegate(FPredictionKeyEvent::CreateUObject(Owner, &UAbilitySystemComponent::OnPredictiveGameplayCueCatchup, Tag));
}

bool FActiveGameplayCueContainer::HasCue(const FGameplayTag& Tag) const
{
	for (int32 idx=0; idx < GameplayCues.Num(); ++idx)
	{
		const FActiveGameplayCue& Cue = GameplayCues[idx];
		if (Cue.GameplayCueTag == Tag)
		{
			return true;
		}
	}

	return false;
}

bool FActiveGameplayCueContainer::NetDeltaSerialize(FNetDeltaSerializeInfo & DeltaParms)
{
	if (bMinimalReplication && (Owner && Owner->ReplicationMode == EGameplayEffectReplicationMode::Full))
	{
		return false;
	}

	return FastArrayDeltaSerialize<FActiveGameplayCue>(GameplayCues, DeltaParms, *this);
}

void FActiveGameplayCueContainer::SetOwner(UAbilitySystemComponent* InOwner)
{
	Owner = InOwner;
	
	// If we already have cues, pretend they were just added
	for (FActiveGameplayCue& Cue : GameplayCues)
	{
		Cue.PostReplicatedAdd(*this);
	}
}

// ----------------------------------------------------------------------------------------

FMinimalGameplayCueReplicationProxy::FMinimalGameplayCueReplicationProxy()
{
	InitGameplayCueParametersFunc = [](FGameplayCueParameters& GameplayCueParameters, UAbilitySystemComponent* InOwner)
	{
		if (InOwner)
		{
			InOwner->InitDefaultGameplayCueParameters(GameplayCueParameters);
		}
	};
}

void FMinimalGameplayCueReplicationProxy::SetOwner(UAbilitySystemComponent* ASC)
{
	Owner = ASC;
	if (Owner && ReplicatedTags.Num() > 0)
	{
		// Invoke events in case we skipped them during ::NetSerialize
		FGameplayCueParameters Parameters;
		InitGameplayCueParametersFunc(Parameters, Owner);

		for (FGameplayTag& Tag : ReplicatedTags)
		{
			Owner->SetTagMapCount(Tag, 1);
			Owner->InvokeGameplayCueEvent(Tag, EGameplayCueEvent::WhileActive, Parameters);
		}
	}
}

void FMinimalGameplayCueReplicationProxy::PreReplication(const FActiveGameplayCueContainer& SourceContainer)
{
	if (LastSourceArrayReplicationKey != SourceContainer.ArrayReplicationKey)
	{
		LastSourceArrayReplicationKey = SourceContainer.ArrayReplicationKey;
		ReplicatedTags.SetNum(SourceContainer.GameplayCues.Num(), false);
		for (int32 idx=0; idx < SourceContainer.GameplayCues.Num(); ++idx)
		{
			ReplicatedTags[idx] = SourceContainer.GameplayCues[idx].GameplayCueTag;
		}
	}
}

bool FMinimalGameplayCueReplicationProxy::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	enum { NumBits = 5 }; // Number of bits to use for number of array
	enum { MaxNum = (1 << NumBits) -1 }; // Number of bits to use for number of array

	uint8 NumElements;

	if (Ar.IsSaving())
	{
		NumElements = ReplicatedTags.Num();
		if (NumElements > MaxNum)
		{
			FString Str;
			for (const FGameplayTag& Tag : ReplicatedTags)
			{
				Str += Tag.ToString() + TEXT(" ");
			}
			ABILITY_LOG(Warning, TEXT("Too many tags in ReplicatedTags on %s. %d total: %s. Dropping"), *GetPathNameSafe(Owner), NumElements, *Str);
			NumElements = MaxNum;
			ReplicatedTags.SetNum(NumElements);
		}

		Ar.SerializeBits(&NumElements, NumBits);

		for (uint8 i=0; i < NumElements; ++i)
		{
			ReplicatedTags[i].NetSerialize(Ar, Map, bOutSuccess);
		}
	}
	else
	{
		NumElements = 0;
		Ar.SerializeBits(&NumElements, NumBits);

		LocalTags = MoveTemp(ReplicatedTags);
		LocalBitMask.Init(true, LocalTags.Num());
		
		ReplicatedTags.SetNumUninitialized(NumElements, false);

		// This struct does not serialize GC parameters but will synthesize them on the receiving side.
		FGameplayCueParameters Parameters;
		InitGameplayCueParametersFunc(Parameters, Owner);

		for (uint8 i=0; i < NumElements; ++i)
		{
			FGameplayTag& ReplicatedTag = ReplicatedTags[i];

			ReplicatedTag.NetSerialize(Ar, Map, bOutSuccess);

			int32 LocalIdx = LocalTags.IndexOfByKey(ReplicatedTag);
			if (LocalIdx != INDEX_NONE)
			{
				// This tag already existed and is accounted for
				LocalBitMask[LocalIdx] = false;
			}
			else if (Owner)
			{
				// This is a new tag, we need to invoke the WhileActive gameplaycue event
				Owner->SetTagMapCount(ReplicatedTag, 1);
				Owner->InvokeGameplayCueEvent(ReplicatedTag, EGameplayCueEvent::WhileActive, Parameters);
			}
		}

		if (Owner)
		{
			for (TConstSetBitIterator<TInlineAllocator<NumInlineTags>> It(LocalBitMask); It; ++It)
			{
				FGameplayTag& RemovedTag = LocalTags[It.GetIndex()];
				Owner->SetTagMapCount(RemovedTag, 0);
				Owner->InvokeGameplayCueEvent(RemovedTag, EGameplayCueEvent::Removed, Parameters);
			}
		}
	}


	bOutSuccess = true;
	return true;
}

void FMinimalGameplayCueReplicationProxy::RemoveAllCues()
{
	if (!Owner)
	{
		return;
	}

	FGameplayCueParameters Parameters;
	InitGameplayCueParametersFunc(Parameters, Owner);

	for (int32 idx=0; idx < ReplicatedTags.Num(); ++idx)
	{
		const FGameplayTag& GameplayCueTag = ReplicatedTags[idx];
		Owner->SetTagMapCount(GameplayCueTag, 0);
		Owner->InvokeGameplayCueEvent(GameplayCueTag, EGameplayCueEvent::Removed, Parameters);
	}
}