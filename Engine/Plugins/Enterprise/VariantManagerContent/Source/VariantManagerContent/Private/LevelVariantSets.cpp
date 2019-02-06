// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LevelVariantSets.h"

#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/World.h"
#include "VariantSet.h"
#include "Variant.h"
#include "VariantObjectBinding.h"
#include "LevelVariantSetsFunctionDirector.h"
#include "Kismet/GameplayStatics.h"
#include "LevelVariantSetsActor.h"
#if WITH_EDITOR
#include "Editor.h"
#include "GameDelegates.h"
#include "Engine/Blueprint.h"
#endif

#define LOCTEXT_NAMESPACE "LevelVariantSets"

ULevelVariantSets::ULevelVariantSets(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	if (!IsTemplate())
	{
		SubscribeToEditorDelegates();
	}
#endif
}

ULevelVariantSets::~ULevelVariantSets()
{
#if WITH_EDITOR
	UnsubscribeToEditorDelegates();
	UnsubscribeToDirectorCompiled();
#endif
}

void ULevelVariantSets::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	if (Ar.IsLoading())
	{
		SubscribeToDirectorCompiled();
	}
#endif
}

void ULevelVariantSets::AddVariantSets(const TArray<UVariantSet*>& NewVariantSets, int32 Index)
{
	Modify();

	if (Index == INDEX_NONE)
	{
		Index = VariantSets.Num();
	}

	TSet<FString> OldNames;
	for (UVariantSet* VarSet : VariantSets)
	{
		OldNames.Add(VarSet->GetDisplayText().ToString());
	}

	// Inserting first ensures we preserve the target order
	VariantSets.Insert(NewVariantSets, Index);

	bool bIsMoveOperation = false;
	TSet<ULevelVariantSets*> ParentsModified;
	for (UVariantSet* NewVarSet : NewVariantSets)
	{
		ULevelVariantSets* OldParent = NewVarSet->GetParent();

		if (OldParent)
		{
			if (OldParent != this)
			{
				OldParent->RemoveVariantSets({NewVarSet});
			}
			else
			{
				bIsMoveOperation = true;
			}
		}

		NewVarSet->Modify();
		NewVarSet->Rename(nullptr, this, REN_DontCreateRedirectors);  // Change parents

		// Update name if we're from a different parent but our names collide
		FString IncomingName = NewVarSet->GetDisplayText().ToString();
		if (OldParent != this && OldNames.Contains(IncomingName))
		{
			NewVarSet->SetDisplayText(FText::FromString(GetUniqueVariantSetName(IncomingName)));
		}
	}

	// If it's a move operation, we'll have to manually clear the old pointers from the array
	if (bIsMoveOperation)
	{
		TSet<UVariantSet*> SetOfNewVariantSets = TSet<UVariantSet*>(NewVariantSets);

		// Sweep back from insertion point nulling old bindings with the same GUID
		for (int32 SweepIndex = Index-1; SweepIndex >= 0; SweepIndex--)
		{
			if (SetOfNewVariantSets.Contains(VariantSets[SweepIndex]))
			{
				VariantSets[SweepIndex] = nullptr;
			}
		}
		// Sweep forward from the end of the inserted segment nulling old bindings with the same GUID
		for (int32 SweepIndex = Index + NewVariantSets.Num(); SweepIndex < VariantSets.Num(); SweepIndex++)
		{
			if (SetOfNewVariantSets.Contains(VariantSets[SweepIndex]))
			{
				VariantSets[SweepIndex] = nullptr;
			}
		}

		// Finally remove null entries
		for (int32 IterIndex = VariantSets.Num() - 1; IterIndex >= 0; IterIndex--)
		{
			if (VariantSets[IterIndex] == nullptr)
			{
				VariantSets.RemoveAt(IterIndex);
			}
		}
	}
}

int32 ULevelVariantSets::GetVariantSetIndex(UVariantSet* VarSet)
{
	return VariantSets.Find(VarSet);
}

const TArray<UVariantSet*>& ULevelVariantSets::GetVariantSets() const
{
	return VariantSets;
}

void ULevelVariantSets::RemoveVariantSets(const TArray<UVariantSet*> InVariantSets)
{
	Modify();

	for (UVariantSet* VariantSet : InVariantSets)
	{
		VariantSets.Remove(VariantSet);
	}
}

FString ULevelVariantSets::GetUniqueVariantSetName(const FString& InPrefix)
{
	TSet<FString> UniqueNames;
	for (UVariantSet* VariantSet : VariantSets)
	{
		UniqueNames.Add(VariantSet->GetDisplayText().ToString());
	}

	if (!UniqueNames.Contains(InPrefix))
	{
		return InPrefix;
	}

	FString VarSetName = FString(InPrefix);

	// Remove potentially existing suffix numbers
	FString LastChar = VarSetName.Right(1);
	while (LastChar.IsNumeric())
	{
		VarSetName = VarSetName.LeftChop(1);
		LastChar = VarSetName.Right(1);
	}

	// Add a numbered suffix
	if (UniqueNames.Contains(VarSetName) || VarSetName.IsEmpty())
	{
		int32 Suffix = 0;
		while (UniqueNames.Contains(VarSetName + FString::FromInt(Suffix)))
		{
			Suffix += 1;
		}

		VarSetName = VarSetName + FString::FromInt(Suffix);
	}

	return VarSetName;
}

UObject* ULevelVariantSets::GetDirectorInstance(UObject* WorldContext)
{
	if (WorldContext == nullptr || WorldContext->IsPendingKillOrUnreachable())
	{
		return nullptr;
	}

	UWorld* TargetWorld = WorldContext->GetWorld();

	// Check if we already created a director for this world
	UObject** FoundDirectorPtr = WorldToDirectorInstance.Find(TargetWorld);
	if (FoundDirectorPtr)
	{
		UObject* FoundDirector = *FoundDirectorPtr;
		if (FoundDirector != nullptr && FoundDirector->IsValidLowLevel() && !FoundDirector->IsPendingKillOrUnreachable())
		{
			return FoundDirector;
		}
	}

	// If not we'll need to create one. It will need to be parented to a LVSActor in that world
	AActor* DirectorOuter = nullptr;

	// Look for a LVSActor in that world that is referencing us
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(TargetWorld, ALevelVariantSetsActor::StaticClass(), FoundActors);
	for (AActor* Actor : FoundActors)
	{
		ALevelVariantSetsActor* ActorAsLVSActor = Cast<ALevelVariantSetsActor>(Actor);
		if (ActorAsLVSActor && ActorAsLVSActor->GetLevelVariantSets() == this)
		{
			DirectorOuter = ActorAsLVSActor;
			break;
		}
	}

	// If we haven't found one, we need to spawn a new LVSActor
	if (DirectorOuter == nullptr)
	{
		FVector Location(0.0f, 0.0f, 0.0f);
		FRotator Rotation(0.0f, 0.0f, 0.0f);
		FActorSpawnParameters SpawnInfo;
		ALevelVariantSetsActor* NewActor = TargetWorld->SpawnActor<ALevelVariantSetsActor>(Location, Rotation, SpawnInfo);
		NewActor->SetLevelVariantSets(this);
		DirectorOuter = NewActor;
	}

	if (DirectorOuter == nullptr)
	{
		return nullptr;
	}

	// Finally create our new director and return it
	ULevelVariantSetsFunctionDirector* NewDirector = NewObject<ULevelVariantSetsFunctionDirector>(DirectorOuter, DirectorClass, NAME_None, RF_Transient);
	NewDirector->GetOnDestroy().AddLambda([this](ULevelVariantSetsFunctionDirector* Director)
	{
		if (this != nullptr && this->IsValidLowLevel() && !this->IsPendingKillOrUnreachable())
		{
			HandleDirectorDestroyed(Director);
		}
	});

	WorldToDirectorInstance.Add(TargetWorld, NewDirector);
	return NewDirector;
}

int32 ULevelVariantSets::GetNumVariantSets()
{
	return VariantSets.Num();
}

UVariantSet* ULevelVariantSets::GetVariantSet(int32 VariantSetIndex)
{
	if (VariantSets.IsValidIndex(VariantSetIndex))
	{
		return VariantSets[VariantSetIndex];
	}

	return nullptr;
}

UVariantSet* ULevelVariantSets::GetVariantSetByName(FString VariantSetName)
{
	UVariantSet** VarSetPtr = VariantSets.FindByPredicate([VariantSetName](const UVariantSet* VarSet)
	{
		return VarSet->GetDisplayText().ToString() == VariantSetName;
	});

	if (VarSetPtr)
	{
		return *VarSetPtr;
	}
	return nullptr;
}

#if WITH_EDITOR
void ULevelVariantSets::SetDirectorGeneratedBlueprint(UObject* InDirectorBlueprint)
{
	UBlueprint* InBP = Cast<UBlueprint>(InDirectorBlueprint);
	if (!InBP)
	{
		return;
	}

	DirectorBlueprint = InBP;
	DirectorClass = CastChecked<UBlueprintGeneratedClass>(InBP->GeneratedClass);

	SubscribeToDirectorCompiled();
}

UObject* ULevelVariantSets::GetDirectorGeneratedBlueprint()
{
	return DirectorBlueprint;
}

UBlueprintGeneratedClass* ULevelVariantSets::GetDirectorGeneratedClass()
{
	return DirectorClass;
}

void ULevelVariantSets::OnDirectorBlueprintRecompiled(UBlueprint* InBP)
{
	for (UVariantSet* VarSet : VariantSets)
	{
		for (UVariant* Var : VarSet->GetVariants())
		{
			for (UVariantObjectBinding* Binding : Var->GetBindings())
			{
				Binding->UpdateFunctionCallerNames();
			}
		}
	}
}

UWorld* ULevelVariantSets::GetWorldContext(int32& OutPIEInstanceID)
{
	if (CurrentWorld == nullptr)
	{
		CurrentWorld = ComputeCurrentWorld(CurrentPIEInstanceID);
		check(CurrentWorld);
	}

	OutPIEInstanceID = CurrentPIEInstanceID;
	return CurrentWorld;
}

void ULevelVariantSets::ResetWorldContext()
{
	CurrentWorld = nullptr;
}

void ULevelVariantSets::OnPieEvent(bool bIsSimulating)
{
	ResetWorldContext();
}

void ULevelVariantSets::OnMapChange(uint32 MapChangeFlags)
{
	ResetWorldContext();
}

UWorld* ULevelVariantSets::ComputeCurrentWorld(int32& OutPIEInstanceID)
{
	UWorld* EditorWorld = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
	{
		if (Context.WorldType == EWorldType::PIE)
		{
			UWorld* ThisWorld = Context.World();
			if (ThisWorld)
			{
				OutPIEInstanceID = Context.PIEInstance;
				return ThisWorld;
			}
		}
		else if (Context.WorldType == EWorldType::Editor)
		{
			EditorWorld = Context.World();
		}
	}

	check(EditorWorld);
	OutPIEInstanceID = INDEX_NONE;
	return EditorWorld;
}

void ULevelVariantSets::SubscribeToEditorDelegates()
{
	FEditorDelegates::MapChange.AddUObject(this, &ULevelVariantSets::OnMapChange);

	// Invalidate CurrentWorld after PIE starts
	FEditorDelegates::PostPIEStarted.AddUObject(this, &ULevelVariantSets::OnPieEvent);

	// This is used as if it was a PostPIEEnded event
	EndPlayDelegateHandle = FGameDelegates::Get().GetEndPlayMapDelegate().AddUObject(this, &ULevelVariantSets::OnMapChange, (uint32)0);
}

void ULevelVariantSets::UnsubscribeToEditorDelegates()
{
	FEditorDelegates::MapChange.RemoveAll(this);
	FEditorDelegates::PostPIEStarted.RemoveAll(this);
	FGameDelegates::Get().GetEndPlayMapDelegate().Remove(EndPlayDelegateHandle);
}

void ULevelVariantSets::SubscribeToDirectorCompiled()
{
	UBlueprint* DirectorBP = Cast<UBlueprint>(DirectorBlueprint);
	if (DirectorBP && !DirectorBP->IsPendingKillOrUnreachable())
	{
		OnBlueprintCompiledHandle = DirectorBP->OnCompiled().AddUObject(this, &ULevelVariantSets::OnDirectorBlueprintRecompiled);
	}
}

void ULevelVariantSets::UnsubscribeToDirectorCompiled()
{
	UBlueprint* DirectorBP = Cast<UBlueprint>(DirectorBlueprint);
	if (DirectorBP && !DirectorBP->IsPendingKillOrUnreachable())
	{
		DirectorBP->OnCompiled().Remove(OnBlueprintCompiledHandle);
	}
}
#endif

void ULevelVariantSets::HandleDirectorDestroyed(ULevelVariantSetsFunctionDirector* Director)
{
	TArray<UWorld*> KeyOfPairsToRemove;

	for (const auto& Pair : WorldToDirectorInstance)
	{
		if (Pair.Value == Director)
		{
			KeyOfPairsToRemove.Add(Pair.Key);
		}
	}

	for (UWorld* World : KeyOfPairsToRemove)
	{
		WorldToDirectorInstance.Remove(World);
	}
}

#undef LOCTEXT_NAMESPACE
