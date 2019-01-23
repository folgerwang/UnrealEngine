// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LevelSequenceActor.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/Texture2D.h"
#include "Components/BillboardComponent.h"
#include "LevelSequenceBurnIn.h"
#include "DefaultLevelSequenceInstanceData.h"
#include "Engine/ActorChannel.h"
#include "Net/UnrealNetwork.h"

#if WITH_EDITOR
	#include "PropertyCustomizationHelpers.h"
	#include "ActorPickerMode.h"
	#include "SceneOutlinerFilters.h"
#endif

ALevelSequenceActor::ALevelSequenceActor(const FObjectInitializer& Init)
	: Super(Init)
{
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComp"));
	RootComponent = SceneComponent;

#if WITH_EDITORONLY_DATA
	UBillboardComponent* SpriteComponent = CreateEditorOnlyDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));

	if (!IsRunningCommandlet())
	{
		// Structure to hold one-time initialization
		struct FConstructorStatics
		{
			ConstructorHelpers::FObjectFinderOptional<UTexture2D> DecalTexture;
			FConstructorStatics() : DecalTexture(TEXT("/Engine/EditorResources/S_LevelSequence")) {}
		};
		static FConstructorStatics ConstructorStatics;

		if (SpriteComponent)
		{
			SpriteComponent->Sprite = ConstructorStatics.DecalTexture.Get();
			SpriteComponent->SetupAttachment(RootComponent);
			SpriteComponent->bIsScreenSizeScaled = true;
			SpriteComponent->bAbsoluteScale = true;
			SpriteComponent->bReceivesDecals = false;
			SpriteComponent->bHiddenInGame = true;
		}
	}
#endif //WITH_EDITORONLY_DATA

	BindingOverrides = Init.CreateDefaultSubobject<UMovieSceneBindingOverrides>(this, "BindingOverrides");
	BurnInOptions = Init.CreateDefaultSubobject<ULevelSequenceBurnInOptions>(this, "BurnInOptions");
	DefaultInstanceData = Init.CreateDefaultSubobject<UDefaultLevelSequenceInstanceData>(this, "InstanceData");

	// SequencePlayer must be a default sub object for it to be replicated correctly
	SequencePlayer = Init.CreateDefaultSubobject<ULevelSequencePlayer>(this, "AnimationPlayer");

	bOverrideInstanceData = false;

	PrimaryActorTick.bCanEverTick = true;
	bAutoPlay_DEPRECATED = false;

	bReplicates = true;
	bReplicatePlayback = false;
}

void ALevelSequenceActor::PostInitProperties()
{
	Super::PostInitProperties();

	// Have to initialize this here as any properties set on default subobjects inside the constructor
	// Get stomped by the CDO's properties when the constructor exits.
	SequencePlayer->SetPlaybackClient(this);
}

bool ALevelSequenceActor::RetrieveBindingOverrides(const FGuid& InBindingId, FMovieSceneSequenceID InSequenceID, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const
{
	return BindingOverrides->LocateBoundObjects(InBindingId, InSequenceID, OutObjects);
}

UObject* ALevelSequenceActor::GetInstanceData() const
{
	return bOverrideInstanceData ? DefaultInstanceData : nullptr;
}

ULevelSequencePlayer* ALevelSequenceActor::GetSequencePlayer() const
{
	return SequencePlayer && SequencePlayer->GetSequence() ? SequencePlayer : nullptr;
}

void ALevelSequenceActor::SetReplicatePlayback(bool bInReplicatePlayback)
{
	bReplicatePlayback = bInReplicatePlayback;
	SetReplicates(bReplicatePlayback);
}

bool ALevelSequenceActor::ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool bWroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	bWroteSomething |= Channel->ReplicateSubobject(SequencePlayer, *Bunch, *RepFlags);

	return bWroteSomething;
}

void ALevelSequenceActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ALevelSequenceActor, SequencePlayer);
}

void ALevelSequenceActor::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	SetReplicates(bReplicatePlayback);
	InitializePlayer();
}

void ALevelSequenceActor::BeginPlay()
{
	Super::BeginPlay();

	RefreshBurnIn();

	if (PlaybackSettings.bAutoPlay)
	{
		SequencePlayer->Play();
	}
}

void ALevelSequenceActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (SequencePlayer)
	{
		SequencePlayer->Update(DeltaSeconds);
	}
}

void ALevelSequenceActor::PostLoad()
{
	Super::PostLoad();

	// If autoplay was previously enabled, initialize the playback settings to autoplay
	if (bAutoPlay_DEPRECATED)
	{
		PlaybackSettings.bAutoPlay = bAutoPlay_DEPRECATED;
		bAutoPlay_DEPRECATED = false;
	}

	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	// We intentionally do not attempt to load any asset in PostLoad other than by way of LoadPackageAsync
	// since under some circumstances it is possible for the sequence to only be partially loaded.
	// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	UWorld* LocalWorld = GetWorld();
	if (LevelSequence.IsValid() && LocalWorld && LocalWorld->IsGameWorld())
	{
		// If we're async loading and we don't have the sequence asset loaded, schedule a load for it
		ULevelSequence* LevelSequenceAsset = GetSequence();
		if (!LevelSequenceAsset && IsAsyncLoading())
		{
			LoadPackageAsync(LevelSequence.GetLongPackageName(), FLoadPackageAsyncDelegate::CreateUObject(this, &ALevelSequenceActor::OnSequenceLoaded));
		}
	}

#if WITH_EDITORONLY_DATA
	// Fix sprite component so that it's attached to the root component. In the past, the sprite component was the root component.
	UBillboardComponent* SpriteComponent = FindComponentByClass<UBillboardComponent>();
	if (SpriteComponent && SpriteComponent->GetAttachParent() != RootComponent)
	{
		SpriteComponent->SetupAttachment(RootComponent);
	}
#endif
}

ULevelSequence* ALevelSequenceActor::GetSequence() const
{
	return Cast<ULevelSequence>(LevelSequence.ResolveObject());
}

ULevelSequence* ALevelSequenceActor::LoadSequence() const
{
	return Cast<ULevelSequence>(LevelSequence.TryLoad());
}

void ALevelSequenceActor::SetSequence(ULevelSequence* InSequence)
{
	if (!SequencePlayer->IsPlaying())
	{
		LevelSequence = InSequence;

		// cbb: should ideally null out the template and player when no sequence is assigned, but that's currently not possible
		if (InSequence)
		{
			SequencePlayer->Initialize(InSequence, GetLevel(), PlaybackSettings);
		}
	}
}

void ALevelSequenceActor::InitializePlayer()
{
	if (LevelSequence.IsValid() && GetWorld()->IsGameWorld())
	{
		// Attempt to reslove the asset without loading it
		ULevelSequence* LevelSequenceAsset = GetSequence();
		if (LevelSequenceAsset)
		{
			// Level sequence is already loaded. Initialize the player if it's not already initialized with this sequence
			if (LevelSequenceAsset != SequencePlayer->GetSequence())
			{
				SequencePlayer->Initialize(LevelSequenceAsset, GetLevel(), PlaybackSettings);
			}
		}
		else if (!IsAsyncLoading())
		{
			LevelSequenceAsset = LoadSequence();
			if (LevelSequenceAsset != SequencePlayer->GetSequence())
			{
				SequencePlayer->Initialize(LevelSequenceAsset, GetLevel(), PlaybackSettings);
			}
		}
		else
		{
			LoadPackageAsync(LevelSequence.GetLongPackageName(), FLoadPackageAsyncDelegate::CreateUObject(this, &ALevelSequenceActor::OnSequenceLoaded));
		}
	}
}

void ALevelSequenceActor::OnSequenceLoaded(const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result)
{
	if (Result == EAsyncLoadingResult::Succeeded)
	{
		ULevelSequence* LevelSequenceAsset = GetSequence();
		if (SequencePlayer->GetSequence() != LevelSequenceAsset)
		{
			SequencePlayer->Initialize(LevelSequenceAsset, GetLevel(), PlaybackSettings);
		}
	}
}

void ALevelSequenceActor::RefreshBurnIn()
{
	if (BurnInInstance)
	{
		BurnInInstance->RemoveFromViewport();
		BurnInInstance = nullptr;
	}
	
	if (BurnInOptions && BurnInOptions->bUseBurnIn)
	{
		// Create the burn-in if necessary
		UClass* Class = BurnInOptions->BurnInClass.TryLoadClass<ULevelSequenceBurnIn>();
		if (Class)
		{
			BurnInInstance = CreateWidget<ULevelSequenceBurnIn>(GetWorld(), Class);
			if (BurnInInstance)
			{
				// Ensure we have a valid settings object if possible
				BurnInOptions->ResetSettings();

				BurnInInstance->SetSettings(BurnInOptions->Settings);
				BurnInInstance->TakeSnapshotsFrom(*this);
				BurnInInstance->AddToViewport();
			}
		}
	}
}



#if WITH_EDITOR

void FBoundActorProxy::Initialize(TSharedPtr<IPropertyHandle> InPropertyHandle)
{
	ReflectedProperty = InPropertyHandle;

	UObject* Object = nullptr;
	ReflectedProperty->GetValue(Object);
	BoundActor = Cast<AActor>(Object);

	ReflectedProperty->SetOnPropertyValueChanged(FSimpleDelegate::CreateRaw(this, &FBoundActorProxy::OnReflectedPropertyChanged));
}

void FBoundActorProxy::OnReflectedPropertyChanged()
{
	UObject* Object = nullptr;
	ReflectedProperty->GetValue(Object);
	BoundActor = Cast<AActor>(Object);
}

TSharedPtr<FStructOnScope> ALevelSequenceActor::GetObjectPickerProxy(TSharedPtr<IPropertyHandle> ObjectPropertyHandle)
{
	TSharedRef<FStructOnScope> Struct = MakeShared<FStructOnScope>(FBoundActorProxy::StaticStruct());
	reinterpret_cast<FBoundActorProxy*>(Struct->GetStructMemory())->Initialize(ObjectPropertyHandle);
	return Struct;
}

void ALevelSequenceActor::UpdateObjectFromProxy(FStructOnScope& Proxy, IPropertyHandle& ObjectPropertyHandle)
{
	UObject* BoundActor = reinterpret_cast<FBoundActorProxy*>(Proxy.GetStructMemory())->BoundActor;
	ObjectPropertyHandle.SetValue(BoundActor);
}

bool ALevelSequenceActor::GetReferencedContentObjects(TArray<UObject*>& Objects) const
{
	ULevelSequence* LevelSequenceAsset = LoadSequence();

	if (LevelSequenceAsset)
	{
		Objects.Add(LevelSequenceAsset);
	}

	Super::GetReferencedContentObjects(Objects);

	return true;
}

#endif



ULevelSequenceBurnInOptions::ULevelSequenceBurnInOptions(const FObjectInitializer& Init)
	: Super(Init)
	, bUseBurnIn(false)
	, BurnInClass(TEXT("/Engine/Sequencer/DefaultBurnIn.DefaultBurnIn_C"))
	, Settings(nullptr)
{
}

void ULevelSequenceBurnInOptions::SetBurnIn(FSoftClassPath InBurnInClass)
{
	BurnInClass = InBurnInClass;
	
	// Attempt to load the settings class from the BurnIn class and assign it to our local Settings object.
	ResetSettings();
}


void ULevelSequenceBurnInOptions::ResetSettings()
{
	UClass* Class = BurnInClass.TryLoadClass<ULevelSequenceBurnIn>();
	if (Class)
	{
		TSubclassOf<ULevelSequenceBurnInInitSettings> SettingsClass = Cast<ULevelSequenceBurnIn>(Class->GetDefaultObject())->GetSettingsClass();
		if (SettingsClass)
		{
			if (!Settings || !Settings->IsA(SettingsClass))
			{
				if (Settings)
				{
					Settings->Rename(*MakeUniqueObjectName(this, ULevelSequenceBurnInInitSettings::StaticClass(), "Settings_EXPIRED").ToString());
				}
				
				Settings = NewObject<ULevelSequenceBurnInInitSettings>(this, SettingsClass, "Settings");
				Settings->SetFlags(GetMaskedFlags(RF_PropagateToSubObjects));
			}
		}
		else
		{
			Settings = nullptr;
		}
	}
	else
	{
		Settings = nullptr;
	}
}

#if WITH_EDITOR

void ULevelSequenceBurnInOptions::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent)
{
	FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ULevelSequenceBurnInOptions, bUseBurnIn) || PropertyName == GET_MEMBER_NAME_CHECKED(ULevelSequenceBurnInOptions, BurnInClass))
	{
		ResetSettings();
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

#endif // WITH_EDITOR
