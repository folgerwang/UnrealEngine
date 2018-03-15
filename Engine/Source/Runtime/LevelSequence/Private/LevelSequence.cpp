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

DEFINE_LOG_CATEGORY_STATIC(LogLevelSequence, Log, All);

static TAutoConsoleVariable<int32> CVarDefaultEvaluationType(
	TEXT("LevelSequence.DefaultEvaluationType"),
	0,
	TEXT("0: Playback locked to playback frames\n1: Unlocked playback with sub frame interpolation"),
	ECVF_Default);

static TAutoConsoleVariable<FString> CVarDefaultFrameResolution(
	TEXT("LevelSequence.DefaultFrameResolution"),
	TEXT("24000fps"),
	TEXT("Specifies default a frame resolution for newly created level sequences. Examples: 30 fps, 120/1 (120 fps), 30000/1001 (29.97), 0.01s (10ms)."),
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

	FFrameRate FrameResolution(60000, 1);
	TryParseString(FrameResolution, *CVarDefaultFrameResolution.GetValueOnGameThread());
	MovieScene->SetFrameResolutionDirectly(FrameResolution);

	FFrameRate PlaybackFrameRate(30, 1);
	TryParseString(PlaybackFrameRate, *CVarDefaultDisplayRate.GetValueOnGameThread());
	MovieScene->SetPlaybackFrameRate(PlaybackFrameRate);
}

UObject* ULevelSequence::MakeSpawnableTemplateFromInstance(UObject& InSourceObject, FName ObjectName)
{
	UObject* NewInstance = NewObject<UObject>(MovieScene, InSourceObject.GetClass(), ObjectName);

	UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
	CopyParams.bNotifyObjectReplacement = false;
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
	return InObject.IsA<AActor>() || InObject.IsA<UActorComponent>();
}

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

				UObject* NewTemplate = NewObject<UObject>(MovieScene, Spawnable.GeneratedClass_DEPRECATED, TemplateName);
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
	return Object.IsA<AActor>() || Object.IsA<UActorComponent>();
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

UMovieScene* ULevelSequence::GetMovieScene() const
{
	return MovieScene;
}

UObject* ULevelSequence::GetParentObject(UObject* Object) const
{
	UActorComponent* Component = Cast<UActorComponent>(Object);

	if (Component != nullptr)
	{
		return Component->GetOwner();
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
