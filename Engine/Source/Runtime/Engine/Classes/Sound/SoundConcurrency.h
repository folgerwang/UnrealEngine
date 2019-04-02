// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SoundConcurrency.generated.h"

class FAudioDevice;
class FSoundConcurrencyManager;

struct FActiveSound;

/** Sound concurrency group ID. */
using FConcurrencyGroupID = uint32;

/** Sound concurrency unique object IDs. */
using FConcurrencyObjectID = uint32;

/** Sound owner object IDs */
using FSoundOwnerObjectID = uint32;

/** Sound instance (USoundBase) object ID. */
using FSoundObjectID = uint32;


UENUM()
namespace EMaxConcurrentResolutionRule
{
	enum Type
	{
		/** When Max Concurrent sounds are active do not start a new sound. */
		PreventNew,

		/** When Max Concurrent sounds are active stop the oldest and start a new one. */
		StopOldest,

		/** When Max Concurrent sounds are active stop the furthest sound.  If all sounds are the same distance then do not start a new sound. */
		StopFarthestThenPreventNew,

		/** When Max Concurrent sounds are active stop the furthest sound.  If all sounds are the same distance then stop the oldest. */
		StopFarthestThenOldest,

		/** Stop the lowest priority sound in the group. If all sounds are the same priority, then it will stop the oldest sound in the group. */
		StopLowestPriority,

		/** Stop the sound that is quietest in the group. */
		StopQuietest,

		/** Stop the lowest priority sound in the group. If all sounds are the same priority, then it won't play a new sound. */
		StopLowestPriorityThenPreventNew,
	};
}


USTRUCT(BlueprintType)
struct ENGINE_API FSoundConcurrencySettings
{
	GENERATED_USTRUCT_BODY()

	/** The max number of allowable concurrent active voices for voices playing in this concurrency group. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Concurrency, meta = (UIMin = "1", ClampMin = "1"))
	int32 MaxCount;

	/* Whether or not to limit the concurrency to per sound owner (i.e. the actor that plays the sound). If the sound doesn't have an owner, it falls back to global concurrency. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Concurrency)
	uint32 bLimitToOwner:1;

	/** Which concurrency resolution policy to use if max voice count is reached. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Concurrency)
	TEnumAsByte<EMaxConcurrentResolutionRule::Type> ResolutionRule;

	/**
	 * The amount of attenuation to apply to older voice instances in this concurrency group. This reduces volume of older voices in a concurrency group as new voices play.
	 *
	 * AppliedVolumeScale = Math.Pow(DuckingScale, VoiceGeneration)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Concurrency, meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float VolumeScale;

	FSoundConcurrencySettings()
		: MaxCount(16)
		, bLimitToOwner(false)
		, ResolutionRule(EMaxConcurrentResolutionRule::StopFarthestThenOldest)
		, VolumeScale(1.0f)
	{}
};

UCLASS(BlueprintType, hidecategories=Object, editinlinenew, MinimalAPI)
class USoundConcurrency : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Settings, meta = (ShowOnlyInnerProperties))
	FSoundConcurrencySettings Concurrency;
};

/** How the concurrency request is handled by the concurrency manager */
enum class EConcurrencyMode : uint8
{
	Group,
	Owner,
	OwnerPerSound,
	Sound,
};

/** Handle to all required data to create and catalog a concurrency group */
struct FConcurrencyHandle
{
	const FSoundConcurrencySettings& Settings;
	const FConcurrencyObjectID ObjectID;
	const bool bIsOverride;

	/** Constructs a handle from concurrency override settings */
	FConcurrencyHandle(const FSoundConcurrencySettings& InSettings);

	/** Constructs a handle to a concurrency asset */
	FConcurrencyHandle(const USoundConcurrency& Concurrency);

	EConcurrencyMode GetMode(const FActiveSound& ActiveSound) const;
};


/** Class which tracks array of active sound pointers for concurrency management */
class FConcurrencyGroup
{
	/** Array of active sounds for this concurrency group. */
	TArray<FActiveSound*> ActiveSounds;

	FConcurrencyGroupID GroupID;
	FConcurrencyObjectID ObjectID;
	FSoundConcurrencySettings Settings;
	int32 Generation;

public:
	/** Constructor for the max concurrency active sound entry. */
	FConcurrencyGroup(FConcurrencyGroupID GroupID, const FConcurrencyHandle& ConcurrencyHandle);

	static FConcurrencyGroupID GenerateNewID();

	/** Returns the active sounds array. */
	const TArray<FActiveSound*>& GetActiveSounds() const { return ActiveSounds; }

	/** Returns the id of the concurrency group */
	FConcurrencyGroupID GetGroupID() const { return GroupID; }

	/** Returns the current generation */
	const int32 GetGeneration() const { return Generation; }

	/** Returns the current generation */
	const FSoundConcurrencySettings& GetSettings() const { return Settings; }

	/** Returns the parent object ID */
	FConcurrencyObjectID GetObjectID() const { return ObjectID; }

	/** Determines if the group is full. */
	bool IsEmpty() const { return ActiveSounds.Num() == 0; }

	/** Determines if the group is full. */
	bool IsFull() const { return Settings.MaxCount <= ActiveSounds.Num(); }

	/** Adds an active sound to the active sound array. */
	void AddActiveSound(struct FActiveSound* ActiveSound);

	/** Removes an active sound from the active sound array. */
	void RemoveActiveSound(FActiveSound* ActiveSound);

	/** Sorts the active sound array by volume */
	void StopQuietSoundsDueToMaxConcurrency();
};

typedef TMap<FConcurrencyGroupID, FConcurrencyGroup> FConcurrencyGroups;

struct FSoundInstanceEntry
{
	TMap<FSoundObjectID, FConcurrencyGroupID> SoundInstanceToConcurrencyGroup;

	FSoundInstanceEntry(FSoundObjectID SoundObjectID, FConcurrencyGroupID GroupID)
	{
		SoundInstanceToConcurrencyGroup.Add(SoundObjectID, GroupID);
	}
};

/** Type for mapping an object id to a concurrency entry. */
typedef TMap<FConcurrencyObjectID, FConcurrencyGroupID> FConcurrencyMap;

struct FOwnerConcurrencyMapEntry
{
	FConcurrencyMap ConcurrencyObjectToConcurrencyGroup;

	FOwnerConcurrencyMapEntry(FConcurrencyObjectID ConcurrencyObjectID, FConcurrencyGroupID GroupID)
	{
		ConcurrencyObjectToConcurrencyGroup.Add(ConcurrencyObjectID, GroupID);
	}
};

/** Maps owners to concurrency maps */
typedef TMap<FSoundOwnerObjectID, FOwnerConcurrencyMapEntry> FOwnerConcurrencyMap;

/** Maps owners to sound instances */
typedef TMap<FSoundOwnerObjectID, FSoundInstanceEntry> FOwnerPerSoundConcurrencyMap;

/** Maps sound object ids to active sound array for global concurrency limiting */
typedef TMap<FSoundObjectID, FConcurrencyGroupID> FPerSoundToActiveSoundsMap;


class FSoundConcurrencyManager
{
public:
	FSoundConcurrencyManager(class FAudioDevice* InAudioDevice);
	ENGINE_API ~FSoundConcurrencyManager();

	void CreateNewGroupsFromHandles(
		const FActiveSound& NewActiveSound,
		const TArray<FConcurrencyHandle>& ConcurrencyHandles,
		TArray<FConcurrencyGroup*>& OutGroupsToApply
	);

	/** Returns a newly allocated active sound given the input active sound struct. Will return nullptr if the active sound concurrency evaluation doesn't allow for it. */
	FActiveSound* CreateNewActiveSound(const FActiveSound& NewActiveSound);

	/** Removes the active sound from concurrency tracking when active sound is stopped. */
	void StopActiveSound(FActiveSound* ActiveSound);

	/** Stops any active sounds due to max concurrency quietest sound resolution rule */
	void UpdateQuietSoundsToStop();

private: // Methods
	/** Evaluates whether or not the sound can play given the concurrency group's rules. Appends permissible
	sounds to evict in order for sound to play (if required) and returns the desired concurrency group. */
	FConcurrencyGroup* CanPlaySound(const FActiveSound& NewActiveSound, const FConcurrencyGroupID GroupID, TArray<FActiveSound*>& OutSoundsToEvict);

	/** Creates a new concurrency group and returns pointer to said group */
	FConcurrencyGroup& CreateNewConcurrencyGroup(const FConcurrencyHandle& ConcurrencyHandle);

	/**  Creates an active sound to play, assigning it to the provided concurrency groups, and evicting required sounds */
	FActiveSound* CreateAndEvictActiveSounds(const FActiveSound& NewActiveSound, const TArray<FConcurrencyGroup*>& GroupsToApply, const TArray<FActiveSound*>& SoundsToEvict);

	/** Finds an active sound able to be evicted based on the provided concurrency settings. */
	FActiveSound* GetEvictableSound(const FActiveSound& NewActiveSound, const FConcurrencyGroup& ConcurrencyGroup);

	/** Handles concurrency evaluation that happens per USoundConcurrencyObject */
	FActiveSound* EvaluateConcurrency(const FActiveSound& NewActiveSound, TArray<FConcurrencyHandle>& ConcurrencyHandles);

private: // Data
	/** Owning audio device ptr for the concurrency manager. */
	FAudioDevice* AudioDevice;

	/** Global concurrency map that maps individual sounds instances to shared USoundConcurrency UObjects. */
	FConcurrencyMap ConcurrencyMap;

	FOwnerConcurrencyMap OwnerConcurrencyMap;

	/** A map of owners to concurrency maps for sounds which are concurrency-limited per sound owner. */
	FOwnerPerSoundConcurrencyMap OwnerPerSoundConcurrencyMap;

	/** Map of sound objects concurrency-limited globally */
	FPerSoundToActiveSoundsMap SoundObjectToConcurrencyGroup;

	/** A map of concurrency active sound ID to concurrency active sounds */
	FConcurrencyGroups ConcurrencyGroups;
};
