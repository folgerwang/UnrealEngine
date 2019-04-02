// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sound/SoundConcurrency.h"
#include "Components/AudioComponent.h"
#include "ActiveSound.h"
#include "AudioDevice.h"
#include "Sound/SoundBase.h"

// Forward Declarations
struct FListener;

/************************************************************************/
/* USoundConcurrency													*/
/************************************************************************/

USoundConcurrency::USoundConcurrency(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

/************************************************************************/
/* USoundConcurrency													*/
/************************************************************************/

FConcurrencyHandle::FConcurrencyHandle(const FSoundConcurrencySettings& InSettings)
	: Settings(InSettings)
	, ObjectID(0)
	, bIsOverride(true)
{
}

FConcurrencyHandle::FConcurrencyHandle(const USoundConcurrency& Concurrency)
	: Settings(Concurrency.Concurrency)
	, ObjectID(Concurrency.GetUniqueID())
	, bIsOverride(false)
{
}

EConcurrencyMode FConcurrencyHandle::GetMode(const FActiveSound& ActiveSound) const
{
	if (Settings.bLimitToOwner && ActiveSound.GetOwnerID() != 0)
	{
		return bIsOverride && ActiveSound.GetSound() != nullptr
			? EConcurrencyMode::OwnerPerSound
			: EConcurrencyMode::Owner;
	}

	return ObjectID == 0 ? EConcurrencyMode::Sound : EConcurrencyMode::Group;
}

/************************************************************************/
/* FConcurrencyGroup												*/
/************************************************************************/

FConcurrencyGroup::FConcurrencyGroup(FConcurrencyGroupID InGroupID, const FConcurrencyHandle& ConcurrencyHandle)
	: GroupID(InGroupID)
	, ObjectID(ConcurrencyHandle.ObjectID)
	, Settings(ConcurrencyHandle.Settings)
	, Generation(0)
{
}

FConcurrencyGroupID FConcurrencyGroup::GenerateNewID()
{
	static FConcurrencyGroupID ConcurrencyGroupIDs = 0;
	return ++ConcurrencyGroupIDs;
}

void FConcurrencyGroup::AddActiveSound(FActiveSound* ActiveSound)
{
	check(GroupID != 0);

	const int32 LastIndex = ActiveSound->ConcurrencyGroupIDs.Num();
	if (ActiveSound->ConcurrencyGroupIDs.AddUnique(GroupID) == LastIndex)
	{
		ActiveSounds.Add(ActiveSound);
		ActiveSound->ConcurrencyGeneration = Generation++;
	}
	else
	{
		UE_LOG(LogAudio, Fatal, TEXT("Attempting to add active sound '%s' to concurrency group multiple times."), *ActiveSound->GetOwnerName());
	}
}

void FConcurrencyGroup::RemoveActiveSound(FActiveSound* ActiveSound)
{
	// Cache generation being removed
	const int32 RemovedGeneration = ActiveSound->ConcurrencyGeneration;

	// Remove from array
	const int32 NumRemoved = ActiveSounds.RemoveSwap(ActiveSound);
	if (NumRemoved == 0)
	{
		return;
	}

	check(NumRemoved == 1);

	// Rebase generations due to removal of a member
	for (FActiveSound* OtherSound : ActiveSounds)
	{
		if (OtherSound->ConcurrencyGeneration > RemovedGeneration)
		{
			OtherSound->ConcurrencyGeneration--;
		}
	}

	Generation--;
}

void FConcurrencyGroup::StopQuietSoundsDueToMaxConcurrency()
{
	// Nothing to do if our active sound count is less than or equal to our max active sounds
	if (Settings.ResolutionRule != EMaxConcurrentResolutionRule::StopQuietest || ActiveSounds.Num() <= Settings.MaxCount)
	{
		return;
	}

	// Helper function for sort this concurrency group's active sounds according to their "volume" concurrency
	// Quieter sounds will be at the front of the array
	struct FCompareActiveSounds
	{
		FORCEINLINE bool operator()(const FActiveSound& A, const FActiveSound& B) const
		{
			return A.VolumeConcurrency < B.VolumeConcurrency;
		}
	};

	ActiveSounds.Sort(FCompareActiveSounds());

	const int32 NumSoundsToStop = ActiveSounds.Num() - Settings.MaxCount;
	check(NumSoundsToStop > 0);

	// Need to make a new list when stopping the sounds since the process of stopping an active sound
	// will remove the sound from this concurrency group's ActiveSounds array.
	int32 i = 0;
	for (; i < NumSoundsToStop; ++i)
	{
		// Flag this active sound as needing to be stopped due to volume-based max concurrency.
		// This will actually be stopped in the audio device update function.
		ActiveSounds[i]->bShouldStopDueToMaxConcurrency = true;
	}

	const int32 NumActiveSounds = ActiveSounds.Num();
	for (; i < NumActiveSounds; ++i)
	{
		ActiveSounds[i]->bShouldStopDueToMaxConcurrency = false;
	}

}


/************************************************************************/
/* FSoundConcurrencyManager												*/
/************************************************************************/

FSoundConcurrencyManager::FSoundConcurrencyManager(class FAudioDevice* InAudioDevice)
	: AudioDevice(InAudioDevice)
{
}

FSoundConcurrencyManager::~FSoundConcurrencyManager()
{
}

void FSoundConcurrencyManager::CreateNewGroupsFromHandles(
	const FActiveSound& NewActiveSound,
	const TArray<FConcurrencyHandle>& ConcurrencyHandles,
	TArray<FConcurrencyGroup*>& OutGroupsToApply
)
{
	for (const FConcurrencyHandle& ConcurrencyHandle : ConcurrencyHandles)
	{
		switch (ConcurrencyHandle.GetMode(NewActiveSound))
		{
			case EConcurrencyMode::Group:
			{
				if (!ConcurrencyMap.Contains(ConcurrencyHandle.ObjectID))
				{
					FConcurrencyGroup& ConcurrencyGroup = CreateNewConcurrencyGroup(ConcurrencyHandle);
					ConcurrencyMap.Add(ConcurrencyHandle.ObjectID, ConcurrencyGroup.GetGroupID());
					OutGroupsToApply.Add(&ConcurrencyGroup);
				}
			}
			break;

			case EConcurrencyMode::Owner:
			{
				const FSoundOwnerObjectID OwnerObjectID = NewActiveSound.GetOwnerID();
				if (FOwnerConcurrencyMapEntry* ConcurrencyEntry = OwnerConcurrencyMap.Find(OwnerObjectID))
				{
					if (!ConcurrencyEntry->ConcurrencyObjectToConcurrencyGroup.Contains(ConcurrencyHandle.ObjectID))
					{
						FConcurrencyGroup& ConcurrencyGroup = CreateNewConcurrencyGroup(ConcurrencyHandle);
						ConcurrencyEntry->ConcurrencyObjectToConcurrencyGroup.Add(ConcurrencyHandle.ObjectID, ConcurrencyGroup.GetGroupID());
						OutGroupsToApply.Add(&ConcurrencyGroup);
					}
				}
				else
				{
					FConcurrencyGroup& ConcurrencyGroup = CreateNewConcurrencyGroup(ConcurrencyHandle);
					OwnerConcurrencyMap.Emplace(OwnerObjectID, FOwnerConcurrencyMapEntry(ConcurrencyHandle.ObjectID, ConcurrencyGroup.GetGroupID()));
					OutGroupsToApply.Add(&ConcurrencyGroup);
				}
			}
			break;

			case EConcurrencyMode::OwnerPerSound:
			{
				const uint32 OwnerObjectID = NewActiveSound.GetOwnerID();
				if (FSoundInstanceEntry* InstanceEntry = OwnerPerSoundConcurrencyMap.Find(OwnerObjectID))
				{
					USoundBase* Sound = NewActiveSound.GetSound();
					check(Sound);
					if (!InstanceEntry->SoundInstanceToConcurrencyGroup.Contains(Sound->GetUniqueID()))
					{
						FConcurrencyGroup& ConcurrencyGroup = CreateNewConcurrencyGroup(ConcurrencyHandle);
						InstanceEntry->SoundInstanceToConcurrencyGroup.Add(ConcurrencyHandle.ObjectID, ConcurrencyGroup.GetGroupID());
						OutGroupsToApply.Add(&ConcurrencyGroup);
					}
				}
				else
				{
					FConcurrencyGroup& ConcurrencyGroup = CreateNewConcurrencyGroup(ConcurrencyHandle);
					OwnerPerSoundConcurrencyMap.Emplace(OwnerObjectID, FSoundInstanceEntry(ConcurrencyHandle.ObjectID, ConcurrencyGroup.GetGroupID()));
					OutGroupsToApply.Add(&ConcurrencyGroup);
				}
			}
			break;

			case EConcurrencyMode::Sound:
			{
				const FSoundObjectID SoundObjectID = NewActiveSound.GetSound()->GetUniqueID();
				if (!SoundObjectToConcurrencyGroup.Contains(SoundObjectID))
				{
					FConcurrencyGroup& ConcurrencyGroup = CreateNewConcurrencyGroup(ConcurrencyHandle);
					SoundObjectToConcurrencyGroup.Add(SoundObjectID, ConcurrencyGroup.GetGroupID());
					OutGroupsToApply.Add(&ConcurrencyGroup);
				}
			}
			break;
		}
	}
}

FActiveSound* FSoundConcurrencyManager::CreateNewActiveSound(const FActiveSound& NewActiveSound)
{
	check(NewActiveSound.GetSound());

	// If there are no concurrency settings associated then there is no limit on this sound
	TArray<FConcurrencyHandle> ConcurrencyHandles;
	NewActiveSound.GetConcurrencyHandles(ConcurrencyHandles);

	// If there was no concurrency or the setting was zero, then always play this sound.
	if (!ConcurrencyHandles.Num())
	{
		FActiveSound* ActiveSound = new FActiveSound(NewActiveSound);
		ActiveSound->SetAudioDevice(AudioDevice);
		return ActiveSound;
	}

#if !UE_BUILD_SHIPPING
	for (const FConcurrencyHandle& ConcurrencyHandle : ConcurrencyHandles)
	{
		check(ConcurrencyHandle.Settings.MaxCount > 0);
	}
#endif

	return EvaluateConcurrency(NewActiveSound, ConcurrencyHandles);
}

FConcurrencyGroup& FSoundConcurrencyManager::CreateNewConcurrencyGroup(const FConcurrencyHandle& ConcurrencyHandle)
{
	//Create & add new concurrency group to the map
	FConcurrencyGroupID GroupID = FConcurrencyGroup::GenerateNewID();
	ConcurrencyGroups.Emplace(GroupID, FConcurrencyGroup(GroupID, ConcurrencyHandle));

	return *ConcurrencyGroups.Find(GroupID);
}

FConcurrencyGroup* FSoundConcurrencyManager::CanPlaySound(const FActiveSound& NewActiveSound, const FConcurrencyGroupID GroupID, TArray<FActiveSound*>& OutSoundsToEvict)
{
	check(GroupID != 0);
	FConcurrencyGroup* ConcurrencyGroup = ConcurrencyGroups.Find(GroupID);
	if (!ConcurrencyGroup)
	{
		UE_LOG(LogAudio, Warning, TEXT("Attempting to add active sound '%s' (owner '%s') to invalid concurrency group."),
			NewActiveSound.GetSound() ? *NewActiveSound.GetSound()->GetFullName() : TEXT("Unset"),
			*NewActiveSound.GetOwnerName());
		return nullptr;
	}

	// StopQuietest doesn't evict, it culls once we instantiate the sound.  This
	// is because it is not possible to evaluate sound volumes *before* they play.
	if (ConcurrencyGroup->GetSettings().ResolutionRule == EMaxConcurrentResolutionRule::StopQuietest)
	{
		return ConcurrencyGroup;
	}

	if (ConcurrencyGroup->IsFull())
	{
		// If no room for new sound, early out
		if (FActiveSound* SoundToEvict = GetEvictableSound(NewActiveSound, *ConcurrencyGroup))
		{
			OutSoundsToEvict.Add(SoundToEvict);
		}
		else
		{
			return nullptr;
		}
	}

	return ConcurrencyGroup;
}

FActiveSound* FSoundConcurrencyManager::GetEvictableSound(const FActiveSound& NewActiveSound, const FConcurrencyGroup& ConcurrencyGroup)
{
	// Concurrency group isn't full so of course there's room
	if (!ConcurrencyGroup.IsFull())
	{
		return nullptr;
	}

	check(AudioDevice);
	TArray<FListener>& Listeners = AudioDevice->Listeners;

	const TArray<FActiveSound*>& ActiveSounds = ConcurrencyGroup.GetActiveSounds();
	FActiveSound* EvictableSound = nullptr;

	const EMaxConcurrentResolutionRule::Type Rule = ConcurrencyGroup.GetSettings().ResolutionRule;
	switch (Rule)
	{
		case EMaxConcurrentResolutionRule::PreventNew:
		{
			// Do nothing.  No sound is evictable as we're preventing anything new
		}
		break;

		case EMaxConcurrentResolutionRule::StopOldest:
		{
			for (int32 SoundIndex = 0; SoundIndex < ActiveSounds.Num(); ++SoundIndex)
			{
				FActiveSound* ActiveSound = ActiveSounds[SoundIndex];
				if (EvictableSound == nullptr || ActiveSound->PlaybackTime > EvictableSound->PlaybackTime)
				{
					EvictableSound = ActiveSound;
				}
			}
		}
		break;

		case EMaxConcurrentResolutionRule::StopFarthestThenPreventNew:
		case EMaxConcurrentResolutionRule::StopFarthestThenOldest:
		{
			int32 ClosestListenerIndex = NewActiveSound.FindClosestListener(Listeners);
			float DistanceToStopSoundSq = FVector::DistSquared(Listeners[ClosestListenerIndex].Transform.GetTranslation(), NewActiveSound.Transform.GetTranslation());

			for (FActiveSound* ActiveSound : ActiveSounds)
			{
				ClosestListenerIndex = ActiveSound->FindClosestListener(Listeners);
				const float DistanceToActiveSoundSq = FVector::DistSquared(Listeners[ClosestListenerIndex].Transform.GetTranslation(), ActiveSound->Transform.GetTranslation());

				if (DistanceToActiveSoundSq > DistanceToStopSoundSq)
				{
					DistanceToStopSoundSq = DistanceToActiveSoundSq;
					EvictableSound = ActiveSound;
				}
				else if (Rule == EMaxConcurrentResolutionRule::StopFarthestThenOldest
							&& DistanceToActiveSoundSq == DistanceToStopSoundSq
							&& (EvictableSound == nullptr || ActiveSound->PlaybackTime > EvictableSound->PlaybackTime))
				{
					DistanceToStopSoundSq = DistanceToActiveSoundSq;
					EvictableSound = ActiveSound;
				}
			}
		}
		break;

		case EMaxConcurrentResolutionRule::StopLowestPriority:
		case EMaxConcurrentResolutionRule::StopLowestPriorityThenPreventNew:
		{
			// Find oldest and oldest lowest priority sound in the group
			for (FActiveSound* CurrSound : ActiveSounds)
			{
				if (EvictableSound == nullptr
					|| (CurrSound->GetPriority() < EvictableSound->GetPriority())
					|| (CurrSound->GetPriority() == EvictableSound->GetPriority() && CurrSound->PlaybackTime > EvictableSound->PlaybackTime))
				{
					EvictableSound = CurrSound;
				}
			}

			if (EvictableSound)
			{
				// Drop request as same priority and preventing new
				if (Rule == EMaxConcurrentResolutionRule::StopLowestPriorityThenPreventNew
					&& EvictableSound->GetPriority() == NewActiveSound.GetPriority())
				{
					EvictableSound = nullptr;
				}

				// Drop request as NewActiveSound's priority is lower than the lowest priority sound playing
				else if (EvictableSound->GetPriority() > NewActiveSound.GetPriority())
				{
					EvictableSound = nullptr;
				}
			}
		}
		break;

		// Eviction not supported by StopQuietest due to it requiring the sound to be initialized in order to calculate.
		// Therefore, it is culled later but not evicted.
		case EMaxConcurrentResolutionRule::StopQuietest:
		break;

		default:
		checkf(false, TEXT("Unknown EMaxConcurrentResolutionRule enumeration."));
		break;
	}

	return EvictableSound;
}

FActiveSound* FSoundConcurrencyManager::EvaluateConcurrency(const FActiveSound& NewActiveSound, TArray<FConcurrencyHandle>& ConcurrencyHandles)
{
	check(NewActiveSound.GetSound());

	TArray<FActiveSound*> SoundsToEvict;
	TArray<FConcurrencyGroup*> GroupsToApply;

	for (const FConcurrencyHandle& ConcurrencyHandle : ConcurrencyHandles)
	{
		switch (ConcurrencyHandle.GetMode(NewActiveSound))
		{
			case EConcurrencyMode::Group:
			{
				if (FConcurrencyGroupID* ConcurrencyGroupID = ConcurrencyMap.Find(ConcurrencyHandle.ObjectID))
				{
					FConcurrencyGroup* ConcurrencyGroup = CanPlaySound(NewActiveSound, *ConcurrencyGroupID, SoundsToEvict);
					if (!ConcurrencyGroup)
					{
						return nullptr;
					}
					GroupsToApply.Add(ConcurrencyGroup);
				}
			}
			break;

			case EConcurrencyMode::Owner:
			{
				if (FOwnerConcurrencyMapEntry* ConcurrencyEntry = OwnerConcurrencyMap.Find(NewActiveSound.GetOwnerID()))
				{
					if (FConcurrencyGroupID* ConcurrencyGroupID = ConcurrencyEntry->ConcurrencyObjectToConcurrencyGroup.Find(ConcurrencyHandle.ObjectID))
					{
						FConcurrencyGroup* ConcurrencyGroup = CanPlaySound(NewActiveSound, *ConcurrencyGroupID, SoundsToEvict);
						if (!ConcurrencyGroup)
						{
							return nullptr;
						}
						GroupsToApply.Add(ConcurrencyGroup);
					}
				}
			}
			break;

			case EConcurrencyMode::OwnerPerSound:
			{
				const uint32 OwnerObjectID = NewActiveSound.GetOwnerID();
				if (FSoundInstanceEntry* InstanceEntry = OwnerPerSoundConcurrencyMap.Find(OwnerObjectID))
				{
					USoundBase* Sound = NewActiveSound.GetSound();
					check(Sound);
					if (FConcurrencyGroupID* ConcurrencyGroupID = InstanceEntry->SoundInstanceToConcurrencyGroup.Find(Sound->GetUniqueID()))
					{
						FConcurrencyGroup* ConcurrencyGroup = CanPlaySound(NewActiveSound, *ConcurrencyGroupID, SoundsToEvict);
						if (!ConcurrencyGroup)
						{
							return nullptr;
						}
						GroupsToApply.Add(ConcurrencyGroup);
					}
				}
			}
			break;

			case EConcurrencyMode::Sound:
			{
				const FSoundObjectID SoundObjectID = NewActiveSound.GetSound()->GetUniqueID();
				if (FConcurrencyGroupID* ConcurrencyGroupID = SoundObjectToConcurrencyGroup.Find(SoundObjectID))
				{
					FConcurrencyGroup* ConcurrencyGroup = CanPlaySound(NewActiveSound, *ConcurrencyGroupID, SoundsToEvict);
					if (!ConcurrencyGroup)
					{
						return nullptr;
					}
					GroupsToApply.Add(ConcurrencyGroup);
				}
			}
			break;
		}
	}

	CreateNewGroupsFromHandles(NewActiveSound, ConcurrencyHandles, GroupsToApply);
	return CreateAndEvictActiveSounds(NewActiveSound, GroupsToApply, SoundsToEvict);
}

FActiveSound* FSoundConcurrencyManager::CreateAndEvictActiveSounds(const FActiveSound& NewActiveSound, const TArray<FConcurrencyGroup*>& GroupsToApply, const TArray<FActiveSound*>& SoundsToEvict)
{
	// First make a new active sound
	FActiveSound* ActiveSound = new FActiveSound(NewActiveSound);
	ActiveSound->SetAudioDevice(AudioDevice);
	check(AudioDevice == ActiveSound->AudioDevice);

	bool bTrackConcurrencyVolume = false;
	for (FConcurrencyGroup* ConcurrencyGroup : GroupsToApply)
	{
		check(ConcurrencyGroup);

		const FSoundConcurrencySettings& Settings = ConcurrencyGroup->GetSettings();
		const float Volume = Settings.VolumeScale;
		if (Volume < 1.0f)
		{
			check(Volume >= 0.0f);
			const int32 NextGeneration = ConcurrencyGroup->GetGeneration() + 1;

			// If we're ducking older sounds in the concurrency group, then loop through each sound in the concurrency group
			// and update their duck amount based on each sound's generation and the next generation count. The older the sound, the more ducking.
			const TArray<FActiveSound*>& ActiveSounds = ConcurrencyGroup->GetActiveSounds();
			for (FActiveSound* CurActiveSound : ActiveSounds)
			{
				check(CurActiveSound);

				const float ActiveSoundGeneration = static_cast<float>(CurActiveSound->ConcurrencyGeneration);
				const float GenerationDelta = NextGeneration - ActiveSoundGeneration;
				CurActiveSound->ConcurrencyGroupVolumeScales.FindOrAdd(ConcurrencyGroup->GetGroupID()) = FMath::Pow(Volume, GenerationDelta);
			}
		}

		// Determine if we need to track concurrency volume on this active sound
		if (ConcurrencyGroup->GetSettings().ResolutionRule == EMaxConcurrentResolutionRule::StopQuietest)
		{
			bTrackConcurrencyVolume = true;
		}

		// And add it to to the concurrency group. This automatically updates generation counts.
		ConcurrencyGroup->AddActiveSound(ActiveSound);
	}

	if (!bTrackConcurrencyVolume)
	{
		ActiveSound->VolumeConcurrency = -1.0f;
	}

	// Stop any sounds now if needed
	for (FActiveSound* SoundToEvict : SoundsToEvict)
	{
		check(SoundToEvict);
		check(AudioDevice == SoundToEvict->AudioDevice);

		// Remove the active sound from the concurrency manager immediately so it doesn't count towards
		// subsequent concurrency resolution checks (i.e. if sounds are triggered multiple times in this frame)
		StopActiveSound(SoundToEvict);

		// Add this sound to list of sounds that need to stop but don't stop it immediately
		AudioDevice->AddSoundToStop(SoundToEvict);
	}


	return ActiveSound;
}

void FSoundConcurrencyManager::StopActiveSound(FActiveSound* ActiveSound)
{
	// Remove this sound from it's concurrency list
	for (const FConcurrencyGroupID ConcurrencyGroupID : ActiveSound->ConcurrencyGroupIDs)
	{
		FConcurrencyGroup* ConcurrencyGroup = ConcurrencyGroups.Find(ConcurrencyGroupID);
		if (!ConcurrencyGroup)
		{
			UE_LOG(LogAudio, Fatal, TEXT("Attempting to remove stopped sound '%s' from inactive concurrency group."),
				ActiveSound->GetSound() ? *ActiveSound->GetSound()->GetName(): TEXT("Unset"));
			return;
		}

		check(!ConcurrencyGroup->IsEmpty());
		ConcurrencyGroup->RemoveActiveSound(ActiveSound);

		if (ConcurrencyGroup->IsEmpty())
		{
			// Get the object ID prior to removing from groups collection to avoid reading
			// from the object after its destroyed.
			const FConcurrencyObjectID ConcurrencyObjectID = ConcurrencyGroup->GetObjectID();

			// Remove the object from the map.
			ConcurrencyGroups.Remove(ConcurrencyGroupID);

			// Remove from global group map if present.
			ConcurrencyMap.Remove(ConcurrencyObjectID);

			// Remove from sound object map if present.
			if (USoundBase* Sound = ActiveSound->GetSound())
			{
				const FSoundOwnerObjectID ObjectID = Sound->GetUniqueID();
				SoundObjectToConcurrencyGroup.Remove(ObjectID);
			}

			// Remove from owner map if present.
			const uint32 OwnerID = ActiveSound->GetOwnerID();
			if (FOwnerConcurrencyMapEntry* OwnerEntry = OwnerConcurrencyMap.Find(OwnerID))
			{
				if (OwnerEntry->ConcurrencyObjectToConcurrencyGroup.Remove(ConcurrencyObjectID))
				{
					if (OwnerEntry->ConcurrencyObjectToConcurrencyGroup.Num() == 0)
					{
						OwnerConcurrencyMap.Remove(OwnerID);
					}
				}
			}

			// Remove from owner per sound map if present.
			if (FSoundInstanceEntry* InstanceEntry = OwnerPerSoundConcurrencyMap.Find(OwnerID))
			{
				if (USoundBase* Sound = ActiveSound->GetSound())
				{
					if (InstanceEntry->SoundInstanceToConcurrencyGroup.Remove(Sound->GetUniqueID()))
					{
						if (InstanceEntry->SoundInstanceToConcurrencyGroup.Num() == 0)
						{
							OwnerPerSoundConcurrencyMap.Remove(OwnerID);
						}
					}
				}
			}
		}
	}
	ActiveSound->ConcurrencyGroupIDs.Reset();
}

void FSoundConcurrencyManager::UpdateQuietSoundsToStop()
{
	for (auto& ConcurrenyGroupEntry : ConcurrencyGroups)
	{
		ConcurrenyGroupEntry.Value.StopQuietSoundsDueToMaxConcurrency();
	}
}
