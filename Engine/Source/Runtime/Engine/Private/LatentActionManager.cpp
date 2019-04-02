// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Engine/LatentActionManager.h"
#include "UObject/Class.h"
#include "LatentActions.h"

FOnLatentActionsChanged FLatentActionManager::LatentActionsChangedDelegate;


/////////////////////////////////////////////////////
// FPendingLatentAction

#if WITH_EDITOR
FString FPendingLatentAction::GetDescription() const
{
	return TEXT("Not implemented");
}
#endif

/////////////////////////////////////////////////////
// FLatentActionManager

void FLatentActionManager::AddNewAction(UObject* InActionObject, int32 UUID, FPendingLatentAction* NewAction)
{
	TSharedPtr<FObjectActions>& ObjectActions = ObjectToActionListMap.FindOrAdd(InActionObject);
	if (!ObjectActions.Get())
	{
		ObjectActions = MakeShareable(new FObjectActions());
	}
	ObjectActions->ActionList.Add(UUID, NewAction);

	LatentActionsChangedDelegate.Broadcast(InActionObject, ELatentActionChangeType::ActionsAdded);
}

void FLatentActionManager::RemoveActionsForObject(TWeakObjectPtr<UObject> InObject)
{
	FObjectActions* ObjectActions = GetActionsForObject(InObject);
	if (ObjectActions)
	{
		FWeakObjectAndActions* FoundEntry = ActionsToRemoveMap.FindByPredicate([InObject](const FWeakObjectAndActions& Entry) { return Entry.Key == InObject; });

		TSharedPtr<TArray<FUuidAndAction>> ActionToRemoveListPtr;
		if (FoundEntry)
		{
			ActionToRemoveListPtr = FoundEntry->Value;
		}
		else
		{
			ActionToRemoveListPtr = MakeShareable(new TArray<FUuidAndAction>());
			ActionsToRemoveMap.Emplace(FWeakObjectAndActions(InObject, ActionToRemoveListPtr));
		}
		ActionToRemoveListPtr->Reserve(ActionToRemoveListPtr->Num() + ObjectActions->ActionList.Num());
		for (FActionList::TConstIterator It(ObjectActions->ActionList); It; ++It)
		{
			ActionToRemoveListPtr->Add(*It);
		}
	}
}

int32 FLatentActionManager::GetNumActionsForObject(TWeakObjectPtr<UObject> InObject)
{
	FObjectActions* ObjectActions = GetActionsForObject(InObject);
	if (ObjectActions)
	{
		return ObjectActions->ActionList.Num();
	}

	return 0;
}


DECLARE_CYCLE_STAT(TEXT("Blueprint Latent Actions"), STAT_TickLatentActions, STATGROUP_Game);

void FLatentActionManager::BeginFrame()
{
	for (FObjectToActionListMap::TIterator ObjIt(ObjectToActionListMap); ObjIt; ++ObjIt)
	{
		FObjectActions* ObjectActions = ObjIt.Value().Get();
		check(ObjectActions);
		ObjectActions->bProcessedThisFrame = false;
	}
}

void FLatentActionManager::ProcessLatentActions(UObject* InObject, float DeltaTime)
{
	SCOPE_CYCLE_COUNTER(STAT_TickLatentActions);

	if (InObject && !InObject->GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		return;
	}

	for (FActionsForObject::TIterator It(ActionsToRemoveMap); It; ++It)
	{
		FObjectActions* ObjectActions = GetActionsForObject(It->Key);
		TSharedPtr<TArray<FUuidAndAction>> ActionToRemoveListPtr = It->Value;
		if (ActionToRemoveListPtr.IsValid() && ObjectActions)
		{
			for (const FUuidAndAction& PendingActionToKill : *ActionToRemoveListPtr)
			{
				FPendingLatentAction* Action = PendingActionToKill.Value;
				const int32 RemovedNum = ObjectActions->ActionList.RemoveSingle(PendingActionToKill.Key, Action);
				if (RemovedNum && Action)
				{
					Action->NotifyActionAborted();
					delete Action;
				}
			}

			// Notify listeners that latent actions for this object were removed
			LatentActionsChangedDelegate.Broadcast(It->Key.Get(), ELatentActionChangeType::ActionsRemoved);
		}

	}
	ActionsToRemoveMap.Reset();

	if (InObject)
	{
		if (FObjectActions* ObjectActions = GetActionsForObject(InObject))
		{
			if (!ObjectActions->bProcessedThisFrame)
			{
				TickLatentActionForObject(DeltaTime, ObjectActions->ActionList, InObject);
				ObjectActions->bProcessedThisFrame = true;
			}
		}
	}
	else 
	{
		for (FObjectToActionListMap::TIterator ObjIt(ObjectToActionListMap); ObjIt; ++ObjIt)
		{	
			TWeakObjectPtr<UObject> WeakPtr = ObjIt.Key();
			UObject* Object = WeakPtr.Get();
			FObjectActions* ObjectActions = ObjIt.Value().Get();
			check(ObjectActions);
			FActionList& ObjectActionList = ObjectActions->ActionList;

			if (Object)
			{
				// Tick all outstanding actions for this object
				if (!ObjectActions->bProcessedThisFrame && ObjectActionList.Num() > 0)
				{
					TickLatentActionForObject(DeltaTime, ObjectActionList, Object);
					ensure(ObjectActions == ObjIt.Value().Get());
					ObjectActions->bProcessedThisFrame = true;
				}
			}
			else
			{
				// Terminate all outstanding actions for this object, which has been GCed
				for (TMultiMap<int32, FPendingLatentAction*>::TConstIterator It(ObjectActionList); It; ++It)
				{
					if (FPendingLatentAction* Action = It.Value())
					{
						Action->NotifyObjectDestroyed();
						delete Action;
					}
				}
				ObjectActionList.Reset();
			}

			// Remove the entry if there are no pending actions remaining for this object (or if the object was NULLed and cleaned up)
			if (ObjectActionList.Num() == 0)
			{
				ObjIt.RemoveCurrent();
			}
		}
	}
}

void FLatentActionManager::TickLatentActionForObject(float DeltaTime, FActionList& ObjectActionList, UObject* InObject)
{
	typedef TPair<int32, FPendingLatentAction*> FActionListPair;
	TArray<FActionListPair, TInlineAllocator<4>> ItemsToRemove;
	
	FLatentResponse Response(DeltaTime);
	for (TMultiMap<int32, FPendingLatentAction*>::TConstIterator It(ObjectActionList); It; ++It)
	{
		FPendingLatentAction* Action = It.Value();

		Response.bRemoveAction = false;

		Action->UpdateOperation(Response);

		if (Response.bRemoveAction)
		{
			ItemsToRemove.Emplace(It.Key(), Action);
		}
	}

	// Remove any items that were deleted
	for (const FActionListPair& ItemPair : ItemsToRemove)
	{
		const int32 ItemIndex = ItemPair.Key;
		FPendingLatentAction* DyingAction = ItemPair.Value;
		ObjectActionList.Remove(ItemIndex, DyingAction);
		delete DyingAction;
	}

	if (ItemsToRemove.Num() > 0)
	{
		LatentActionsChangedDelegate.Broadcast(InObject, ELatentActionChangeType::ActionsRemoved);
	}

	// Trigger any pending execution links
	for (FLatentResponse::FExecutionInfo& LinkInfo : Response.LinksToExecute)
	{
		if (LinkInfo.LinkID != INDEX_NONE)
		{
			if (UObject* CallbackTarget = LinkInfo.CallbackTarget.Get())
			{
				check(CallbackTarget == InObject);

				if (UFunction* ExecutionFunction = CallbackTarget->FindFunction(LinkInfo.ExecutionFunction))
				{
					CallbackTarget->ProcessEvent(ExecutionFunction, &(LinkInfo.LinkID));
				}
				else
				{
					UE_LOG(LogScript, Warning, TEXT("FLatentActionManager::ProcessLatentActions: Could not find latent action resume point named '%s' on '%s' called by '%s'"),
						*LinkInfo.ExecutionFunction.ToString(), *(CallbackTarget->GetPathName()), *(InObject->GetPathName()));
				}
			}
			else
			{
				UE_LOG(LogScript, Warning, TEXT("FLatentActionManager::ProcessLatentActions: CallbackTarget is None."));
			}
		}
	}
}

#if WITH_EDITOR


FString FLatentActionManager::GetDescription(UObject* InObject, int32 UUID) const
{
	check(InObject);

	FString Description = *NSLOCTEXT("LatentActionManager", "NoPendingActions", "No Pending Actions").ToString();

	const FObjectActions* ObjectActions = GetActionsForObject(InObject);
	if (ObjectActions && ObjectActions->ActionList.Num() > 0)
	{	
		TArray<FPendingLatentAction*> Actions;
		ObjectActions->ActionList.MultiFind(UUID, Actions);

		const int32 PendingActions = Actions.Num();

		// See if there are pending actions
		if (PendingActions > 0)
		{
			FPendingLatentAction* PrimaryAction = Actions[0];
			FString ActionDesc = PrimaryAction->GetDescription();

			Description = (PendingActions > 1)
				? FText::Format(NSLOCTEXT("LatentActionManager", "NumPendingActionsFwd", "{0} Pending Actions: {1}"), PendingActions, FText::FromString(ActionDesc)).ToString()
				: ActionDesc;
		}
	}
	return Description;
}

void FLatentActionManager::GetActiveUUIDs(UObject* InObject, TSet<int32>& UUIDList) const
{
	check(InObject);

	const FObjectActions* ObjectActions = GetActionsForObject(InObject);
	if (ObjectActions && ObjectActions->ActionList.Num() > 0)
	{
		for (TMultiMap<int32, FPendingLatentAction*>::TConstIterator It(ObjectActions->ActionList); It; ++It)
		{
			UUIDList.Add(It.Key());
		}
	}
}

#endif

FLatentActionManager::~FLatentActionManager()
{
	for (auto& ObjectActionListIterator : ObjectToActionListMap)
	{
		TSharedPtr<FObjectActions>& ObjectActions = ObjectActionListIterator.Value;
		if (ObjectActions.IsValid())
		{
			for (auto& ActionIterator : ObjectActions->ActionList)
			{
				FPendingLatentAction* Action = ActionIterator.Value;
				ActionIterator.Value = nullptr;
				delete Action;
			}
			ObjectActions->ActionList.Reset();
		}
	}
}
