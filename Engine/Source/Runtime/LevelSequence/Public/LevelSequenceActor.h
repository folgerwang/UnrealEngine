// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneBindingOwnerInterface.h"
#include "MovieSceneBindingOverrides.h"
#include "LevelSequenceActor.generated.h"

class ULevelSequenceBurnIn;

UCLASS(Blueprintable, DefaultToInstanced)
class LEVELSEQUENCE_API ULevelSequenceBurnInInitSettings : public UObject
{
	GENERATED_BODY()
};

UCLASS(config=EditorPerProjectUserSettings, PerObjectConfig, DefaultToInstanced, BlueprintType)
class LEVELSEQUENCE_API ULevelSequenceBurnInOptions : public UObject
{
public:

	GENERATED_BODY()
	ULevelSequenceBurnInOptions(const FObjectInitializer& Init);

	/** Loads the specified class path and initializes an instance, then stores it in Settings. */
	UFUNCTION(BlueprintCallable, Category = "General")
	void SetBurnIn(FSoftClassPath InBurnInClass);

	/** Ensure the settings object is up-to-date */
	void ResetSettings();

public:

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category="General")
	bool bUseBurnIn;

	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category="General", meta=(EditCondition=bUseBurnIn, MetaClass="LevelSequenceBurnIn"))
	FSoftClassPath BurnInClass;

	UPROPERTY(Instanced, EditAnywhere, BlueprintReadWrite, Category="General", meta=(EditCondition=bUseBurnIn))
	ULevelSequenceBurnInInitSettings* Settings;

protected:

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR
};

/**
 * Actor responsible for controlling a specific level sequence in the world.
 */
UCLASS(hideCategories=(Rendering, Physics, LOD, Activation, Input))
class LEVELSEQUENCE_API ALevelSequenceActor
	: public AActor
	, public IMovieScenePlaybackClient
	, public IMovieSceneBindingOwnerInterface
{
public:

	DECLARE_DYNAMIC_DELEGATE(FOnLevelSequenceLoaded);

	GENERATED_BODY()

	/** Create and initialize a new instance. */
	ALevelSequenceActor(const FObjectInitializer& Init);

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playback", meta=(ShowOnlyInnerProperties))
	FMovieSceneSequencePlaybackSettings PlaybackSettings;

	UPROPERTY(Instanced, transient, replicated, BlueprintReadOnly, BlueprintGetter=GetSequencePlayer, Category="Playback", meta=(ExposeFunctionCategories="Game|Cinematic"))
	ULevelSequencePlayer* SequencePlayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="General", meta=(AllowedClasses="LevelSequence"))
	FSoftObjectPath LevelSequence;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="General")
	TArray<AActor*> AdditionalEventReceivers;

	UPROPERTY(Instanced, BlueprintReadOnly, Category="General")
	ULevelSequenceBurnInOptions* BurnInOptions;

	/** Mapping of actors to override the sequence bindings with */
	UPROPERTY(Instanced, BlueprintReadOnly, Category="General")
	UMovieSceneBindingOverrides* BindingOverrides;

	UPROPERTY()
	uint8 bAutoPlay_DEPRECATED : 1;

	/** Enable specification of dynamic instance data to be supplied to the sequence during playback */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="General")
	uint8 bOverrideInstanceData : 1;

	/** If true, playback of this level sequence on the server will be synchronized across other clients */
	UPROPERTY(EditAnywhere, DisplayName="Replicate Playback", BlueprintReadWrite, BlueprintSetter=SetReplicatePlayback, Category=Replication)
	uint8 bReplicatePlayback:1;

	/** Instance data that can be used to dynamically control sequence evaluation at runtime */
	UPROPERTY(Instanced, BlueprintReadWrite, Category="General")
	UObject* DefaultInstanceData;

public:

	/**
	 * Get the level sequence being played by this actor.
	 *
	 * @return Level sequence, or nullptr if not assigned or if it cannot be loaded.
	 * @see SetSequence
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	ULevelSequence* GetSequence() const;

	/**
	 * Get the level sequence being played by this actor.
	 *
	 * @return Level sequence, or nullptr if not assigned or if it cannot be loaded.
	 * @see SetSequence
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	ULevelSequence* LoadSequence() const;

	/**
	 * Set the level sequence being played by this actor.
	 *
	 * @param InSequence The sequence object to set.
	 * @see GetSequence
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetSequence(ULevelSequence* InSequence);

	/**
	 * Set an array of additional actors that will receive events triggerd from this sequence actor
	 *
	 * @param AdditionalReceivers An array of actors to receive events
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetEventReceivers(TArray<AActor*> AdditionalReceivers) { AdditionalEventReceivers = AdditionalReceivers; }

	/**
	 * Set whether or not to replicate playback for this actor
	 */
	UFUNCTION(BlueprintSetter)
	void SetReplicatePlayback(bool ReplicatePlayback);

	/**
	 * Access this actor's sequence player, or None if it is not yet initialized
	 */
	UFUNCTION(BlueprintGetter)
	ULevelSequencePlayer* GetSequencePlayer() const;

	/** Refresh this actor's burn in */
	void RefreshBurnIn();

public:

	/** Overrides the specified binding with the specified actors, optionally still allowing the bindings defined in the Level Sequence asset
	 *
	 * @param Binding Binding to modify
	 * @param Actors Actors to bind
	 * @param bAllowBindingsFromAsset Allow bindings from the level sequence asset
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic|Bindings")
	void SetBinding(FMovieSceneObjectBindingID Binding, const TArray<AActor*>& Actors, bool bAllowBindingsFromAsset = false)
	{
		BindingOverrides->SetBinding(Binding, TArray<UObject*>(Actors), bAllowBindingsFromAsset);
		if (SequencePlayer)
		{
			SequencePlayer->State.Invalidate(Binding.GetGuid(), Binding.GetSequenceID());
		}
	}

	/** Adds the specified actor to the overridden bindings for the specified binding ID, optionally still allowing the bindings defined in the Level Sequence asset
	 *
	 * @param Binding Binding to modify
	 * @param Actor Actor to bind
	 * @param bAllowBindingsFromAsset Allow bindings from the level sequence asset
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic|Bindings")
	void AddBinding(FMovieSceneObjectBindingID Binding, AActor* Actor, bool bAllowBindingsFromAsset = false)
	{
		BindingOverrides->AddBinding(Binding, Actor, bAllowBindingsFromAsset);
		if (SequencePlayer)
		{
			SequencePlayer->State.Invalidate(Binding.GetGuid(), Binding.GetSequenceID());
		}
	}

	/** Removes the specified actor from the specified binding's actor array */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic|Bindings")
	void RemoveBinding(FMovieSceneObjectBindingID Binding, AActor* Actor)
	{
		BindingOverrides->RemoveBinding(Binding, Actor);
		if (SequencePlayer)
		{
			SequencePlayer->State.Invalidate(Binding.GetGuid(), Binding.GetSequenceID());
		}
	}

	/** Resets the specified binding back to the defaults defined by the Level Sequence asset */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic|Bindings")
	void ResetBinding(FMovieSceneObjectBindingID Binding)
	{
		BindingOverrides->ResetBinding(Binding);
		if (SequencePlayer)
		{
			SequencePlayer->State.Invalidate(Binding.GetGuid(), Binding.GetSequenceID());
		}
	}

	/** Resets all overridden bindings back to the defaults defined by the Level Sequence asset */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic|Bindings")
	void ResetBindings()
	{
		BindingOverrides->ResetBindings();
		if (SequencePlayer)
		{
			SequencePlayer->State.ClearObjectCaches(*SequencePlayer);
		}
	}

protected:

	//~ Begin IMovieScenePlaybackClient interface
	virtual bool RetrieveBindingOverrides(const FGuid& InBindingId, FMovieSceneSequenceID InSequenceID, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;
	virtual UObject* GetInstanceData() const override;
	//~ End IMovieScenePlaybackClient interface

	//~ Begin UObject interface
	virtual bool ReplicateSubobjects(UActorChannel* Channel, FOutBunch* Bunch, FReplicationFlags *RepFlags) override;
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;
	//~ End UObject interface

	//~ Begin AActor interface
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostInitializeComponents() override;
	virtual void BeginPlay() override;
	//~ End AActor interface

public:

#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif //WITH_EDITOR

	/** Initialize the player object by loading the asset, using async loading when necessary */
	void InitializePlayer();

	/** Initialize the player object with the specified asset */
	void InitializePlayerWithSequence(ULevelSequence* LevelSequenceAsset);
	void OnSequenceLoaded(const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result);

#if WITH_EDITOR
	virtual TSharedPtr<FStructOnScope> GetObjectPickerProxy(TSharedPtr<IPropertyHandle> PropertyHandle) override;
	virtual void UpdateObjectFromProxy(FStructOnScope& Proxy, IPropertyHandle& ObjectPropertyHandle) override;
	virtual UMovieSceneSequence* RetrieveOwnedSequence() const override
	{
		return LoadSequence();
	}
#endif

private:
	/** Burn-in widget */
	UPROPERTY()
	ULevelSequenceBurnIn* BurnInInstance;
};


USTRUCT()
struct FBoundActorProxy
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA

	/** Specifies the actor to override the binding with */
	UPROPERTY(EditInstanceOnly, AdvancedDisplay, Category="General")
	AActor* BoundActor;

	void Initialize(TSharedPtr<IPropertyHandle> InPropertyHandle);

	void OnReflectedPropertyChanged();

	TSharedPtr<IPropertyHandle> ReflectedProperty;

#endif
};