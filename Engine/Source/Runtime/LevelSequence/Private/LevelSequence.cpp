// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "LevelSequence.h"
#include "Engine/EngineTypes.h"
#include "HAL/IConsoleManager.h"
#include "Components/ActorComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/Engine.h"
#include "MovieScene.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "Animation/AnimInstance.h"

#if WITH_EDITOR
	#include "UObject/SequencerObjectVersion.h"
	#include "UObject/ObjectRedirector.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogLevelSequence, Log, All);

static TAutoConsoleVariable<int32> CVarDefaultEvaluationType(
	TEXT("LevelSequence.DefaultEvaluationType"),
	0,
	TEXT("0: Playback locked to playback frames\n1: Unlocked playback with sub frame interpolation"),
	ECVF_Default);

static TAutoConsoleVariable<FString> CVarDefaultTickResolution(
	TEXT("LevelSequence.DefaultTickResolution"),
	TEXT("24000fps"),
	TEXT("Specifies default a tick resolution for newly created level sequences. Examples: 30 fps, 120/1 (120 fps), 30000/1001 (29.97), 0.01s (10ms)."),
	ECVF_Default);

static TAutoConsoleVariable<FString> CVarDefaultDisplayRate(
	TEXT("LevelSequence.DefaultDisplayRate"),
	TEXT("30fps"),
	TEXT("Specifies default a display frame rate for newly created level sequences; also defines frame locked frame rate where sequences are set to be frame locked. Examples: 30 fps, 120/1 (120 fps), 30000/1001 (29.97), 0.01s (10ms)."),
	ECVF_Default);

ULevelSequence::ULevelSequence(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, MovieScene(nullptr)
{
	bParentContextsAreSignificant = true;
}

void ULevelSequence::Initialize()
{
	// @todo sequencer: gmp: fix me
	MovieScene = NewObject<UMovieScene>(this, NAME_None, RF_Transactional);

	const bool bFrameLocked = CVarDefaultEvaluationType.GetValueOnGameThread() != 0;

	MovieScene->SetEvaluationType( bFrameLocked ? EMovieSceneEvaluationType::FrameLocked : EMovieSceneEvaluationType::WithSubFrames );

	FFrameRate TickResolution(60000, 1);
	TryParseString(TickResolution, *CVarDefaultTickResolution.GetValueOnGameThread());
	MovieScene->SetTickResolutionDirectly(TickResolution);

	FFrameRate DisplayRate(30, 1);
	TryParseString(DisplayRate, *CVarDefaultDisplayRate.GetValueOnGameThread());
	MovieScene->SetDisplayRate(DisplayRate);
}

UObject* ULevelSequence::MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName)
{
	UObject* NewInstance = NewObject<UObject>(MovieScene, InSourceObject.GetClass(), ObjectName);

	UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
	CopyParams.bNotifyObjectReplacement = false;
	CopyParams.bPreserveRootComponent = false;
	UEngine::CopyPropertiesForUnrelatedObjects(&InSourceObject, NewInstance, CopyParams);

	AActor* Actor = CastChecked<AActor>(NewInstance);
	if (Actor->GetAttachParentActor() != nullptr)
	{
		// We don't support spawnables and attachments right now
		// @todo: map to attach track?
		Actor->DetachFromActor(FDetachmentTransformRules(FAttachmentTransformRules(EAttachmentRule::KeepRelative, false), false));
	}

	return NewInstance;
}

bool ULevelSequence::CanAnimateObject(UObject& InObject) const 
{
	return InObject.IsA<AActor>() || InObject.IsA<UActorComponent>() || InObject.IsA<UAnimInstance>();
}

#if WITH_EDITOR

void PurgeLegacyBlueprints(UObject* InObject, UPackage* Package)
{
	if (UBlueprint* BP = Cast<UBlueprint>(InObject))
	{
		UPackage* TransientPackage = GetTransientPackage();

		{
			FString OldName = BP->GetName();

			BP->ClearFlags(RF_Public);
			BP->SetFlags(RF_Transient);
			BP->RemoveFromRoot();

			FName NewName = MakeUniqueObjectName(TransientPackage, UBlueprint::StaticClass(), *FString::Printf(TEXT("DEAD_SPAWNABLE_BLUEPRINT_%s"), *BP->GetName()));
			BP->Rename(*NewName.ToString(), GetTransientPackage(), (REN_NonTransactional|REN_ForceNoResetLoaders|REN_DoNotDirty));

			UE_LOG(LogLevelSequence, Log, TEXT("Discarding blueprint '%s' from package '%s'."), *OldName, *Package->GetName());
		}

		if (BP->GeneratedClass)
		{
			FName    OldName    = BP->GeneratedClass->GetFName();
			UObject* OldOuter   = BP->GeneratedClass->GetOuter();
			UClass*  SuperClass = BP->GeneratedClass->GetSuperClass();

			if( BP->GeneratedClass->ClassDefaultObject )
			{
				BP->GeneratedClass->ClassDefaultObject->ClearFlags(RF_Public);
				BP->GeneratedClass->ClassDefaultObject->SetFlags(RF_Transient);
				BP->GeneratedClass->ClassDefaultObject->RemoveFromRoot();
			}

			BP->GeneratedClass->ClearFlags(RF_Public);
			BP->GeneratedClass->SetFlags(RF_Transient);
			BP->GeneratedClass->ClassFlags |= CLASS_Deprecated;
			BP->GeneratedClass->RemoveFromRoot();

			FName NewName = MakeUniqueObjectName(TransientPackage, BP->GeneratedClass, *FString::Printf(TEXT("DEAD_SPAWNABLE_BP_CLASS_%s_C"), *BP->GeneratedClass->ClassGeneratedBy->GetName()));
			BP->GeneratedClass->Rename(*NewName.ToString(), GetTransientPackage(), (REN_DoNotDirty|REN_NonTransactional|REN_ForceNoResetLoaders));

			if (SuperClass)
			{
				UObjectRedirector* Redirector = NewObject<UObjectRedirector>(OldOuter, OldName);
				Redirector->DestinationObject = SuperClass;

				UE_LOG(LogLevelSequence, Log, TEXT("Discarding generated class '%s' from package '%s'. Replacing with redirector to '%s'"), *OldName.ToString(), *Package->GetName(), *SuperClass->GetName());
			}
			else
			{
				UE_LOG(LogLevelSequence, Log, TEXT("Discarding generated class '%s' from package '%s'. Unable to create redirector due to no super class."), *OldName.ToString(), *Package->GetName());
			}
		}
	}
}
#endif

void ULevelSequence::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	TSet<FGuid> InvalidSpawnables;

	for (int32 Index = 0; Index < MovieScene->GetSpawnableCount(); ++Index)
	{
		FMovieSceneSpawnable& Spawnable = MovieScene->GetSpawnable(Index);
		if (!Spawnable.GetObjectTemplate())
		{
			if (Spawnable.GeneratedClass_DEPRECATED && Spawnable.GeneratedClass_DEPRECATED->ClassGeneratedBy)
			{
				const FName TemplateName = MakeUniqueObjectName(MovieScene, UObject::StaticClass(), Spawnable.GeneratedClass_DEPRECATED->ClassGeneratedBy->GetFName());

				UObject* NewTemplate = NewObject<UObject>(MovieScene, Spawnable.GeneratedClass_DEPRECATED->GetSuperClass(), TemplateName);
				if (NewTemplate)
				{
					Spawnable.CopyObjectTemplate(*NewTemplate, *this);
				}
			}
		}

		if (!Spawnable.GetObjectTemplate())
		{
			InvalidSpawnables.Add(Spawnable.GetGuid());
			UE_LOG(LogLevelSequence, Warning, TEXT("Discarding spawnable with ID '%s' since its generated class could not produce to a template actor"), *Spawnable.GetGuid().ToString());
		}
	}

	for (FGuid& ID : InvalidSpawnables)
	{
		MovieScene->RemoveSpawnable(ID);
	}

	if (GetLinkerCustomVersion(FSequencerObjectVersion::GUID) < FSequencerObjectVersion::PurgeSpawnableBlueprints)
	{
		// Remove any old generated classes from the package that will have been left behind from when we used blueprints for spawnables
		UPackage* Package = GetOutermost();
		ForEachObjectWithOuter(MovieScene, [=](UObject* InObject) { PurgeLegacyBlueprints(InObject, Package); }, false);

		// Remove any invalid object bindings
		TSet<FGuid> ValidObjectBindings;
		for (int32 Index = 0; Index < MovieScene->GetSpawnableCount(); ++Index)
		{
			ValidObjectBindings.Add(MovieScene->GetSpawnable(Index).GetGuid());
		}
		for (int32 Index = 0; Index < MovieScene->GetPossessableCount(); ++Index)
		{
			ValidObjectBindings.Add(MovieScene->GetPossessable(Index).GetGuid());
		}

		BindingReferences.RemoveInvalidBindings(ValidObjectBindings);
	}

#endif
}

void ULevelSequence::ConvertPersistentBindingsToDefault(UObject* FixupContext)
{
	if (PossessedObjects_DEPRECATED.Num() == 0)
	{
		return;
	}

	MarkPackageDirty();
	for (auto& Pair : PossessedObjects_DEPRECATED)
	{
		UObject* Object = Pair.Value.GetObject();
		if (Object)
		{
			FGuid ObjectId;
			FGuid::Parse(Pair.Key, ObjectId);
			BindingReferences.AddBinding(ObjectId, Object, FixupContext);
		}
	}
	PossessedObjects_DEPRECATED.Empty();
}

void ULevelSequence::BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context)
{
	if (Context)
	{
		BindingReferences.AddBinding(ObjectId, &PossessedObject, Context);
	}
}

bool ULevelSequence::CanPossessObject(UObject& Object, UObject* InPlaybackContext) const
{
	return Object.IsA<AActor>() || Object.IsA<UActorComponent>() || Object.IsA<UAnimInstance>();
}

void ULevelSequence::LocateBoundObjects(const FGuid& ObjectId, UObject* Context, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	// Handle legacy object references
	UObject* Object = Context ? ObjectReferences.ResolveBinding(ObjectId, Context) : nullptr;
	if (Object)
	{
		OutObjects.Add(Object);
	}

	BindingReferences.ResolveBinding(ObjectId, Context, OutObjects);
}

void ULevelSequence::GatherExpiredObjects(const FMovieSceneObjectCache& InObjectCache, TArray<FGuid>& OutInvalidIDs) const
{
	for (const FGuid& ObjectId : BindingReferences.GetBoundAnimInstances())
	{
		for (TWeakObjectPtr<> WeakObject : InObjectCache.IterateBoundObjects(ObjectId))
		{
			UAnimInstance* AnimInstance = Cast<UAnimInstance>(WeakObject.Get());
			if (!AnimInstance || !AnimInstance->GetOwningComponent() || AnimInstance->GetOwningComponent()->GetAnimInstance() != AnimInstance)
			{
				OutInvalidIDs.Add(ObjectId);
			}
		}
	}
}

UMovieScene* ULevelSequence::GetMovieScene() const
{
	return MovieScene;
}

UObject* ULevelSequence::GetParentObject(UObject* Object) const
{
	if (UActorComponent* Component = Cast<UActorComponent>(Object))
	{
		return Component->GetOwner();
	}

	if (UAnimInstance* AnimInstance = Cast<UAnimInstance>(Object))
	{
		return AnimInstance->GetOwningComponent();
	}

	return nullptr;
}

bool ULevelSequence::AllowsSpawnableObjects() const
{
	return true;
}

bool ULevelSequence::CanRebindPossessable(const FMovieScenePossessable& InPossessable) const
{
	return !InPossessable.GetParent().IsValid();
}

void ULevelSequence::UnbindPossessableObjects(const FGuid& ObjectId)
{
	BindingReferences.RemoveBinding(ObjectId);

	// Legacy object references
	ObjectReferences.Map.Remove(ObjectId);
}
