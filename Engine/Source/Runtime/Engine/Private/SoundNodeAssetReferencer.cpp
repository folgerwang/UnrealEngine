// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sound/SoundNodeAssetReferencer.h"
#include "Sound/SoundNodeQualityLevel.h"
#include "Sound/SoundNodeRandom.h"
#include "Sound/SoundCue.h"
#include "UObject/FrameworkObjectVersion.h"

#define ASYNC_LOAD_RANDOMIZED_SOUNDS 1

bool USoundNodeAssetReferencer::ShouldHardReferenceAsset() const
{
	bool bShouldHardReference = true;

	if (USoundCue* Cue = Cast<USoundCue>(GetOuter()))
	{
		TArray<USoundNodeQualityLevel*> QualityNodes;
		TArray<USoundNodeAssetReferencer*> WavePlayers;
		Cue->RecursiveFindNode(Cue->FirstNode, QualityNodes);

		for (USoundNodeQualityLevel* QualityNode : QualityNodes)
		{
			WavePlayers.Reset();
			Cue->RecursiveFindNode(QualityNode, WavePlayers);
			if (WavePlayers.Contains(this))
			{
				bShouldHardReference = false;
				break;
			}
		}

#if ASYNC_LOAD_RANDOMIZED_SOUNDS
		if (bShouldHardReference)
		{
			//Check for randomized sounds as well:
			TArray<USoundNodeRandom*> RandomNodes;
			Cue->RecursiveFindNode(Cue->FirstNode, RandomNodes);

			for (USoundNodeRandom* RandomNode : RandomNodes)
			{
				WavePlayers.Reset();
				Cue->RecursiveFindNode(RandomNode, WavePlayers);
				if (WavePlayers.Contains(this))
				{
					bShouldHardReference = false;
					break;
				}
			}
		}
#endif // ASYNC_LOAD_RANDOMIZED_SOUNDS

		
	}

	return bShouldHardReference;
}

#if WITH_EDITOR
void USoundNodeAssetReferencer::PostEditImport()
{
	Super::PostEditImport();

	LoadAsset();
}
#endif
