// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Sound/SoundCue.h"
#include "Misc/App.h"
#include "EngineDefines.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "Misc/CoreDelegates.h"
#include "Components/AudioComponent.h"
#include "UObject/UObjectIterator.h"
#include "EngineUtils.h"
#include "Sound/SoundNode.h"
#include "Sound/SoundNodeMixer.h"
#include "Sound/SoundNodeAssetReferencer.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundNodeAttenuation.h"
#include "Sound/SoundNodeQualityLevel.h"
#include "Sound/SoundNodeSoundClass.h"
#include "Sound/SoundNodeRandom.h"
#include "GameFramework/GameUserSettings.h"
#include "AudioCompressionSettingsUtils.h"
#include "AudioThread.h"
#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#include "SoundCueGraph/SoundCueGraphNode.h"
#include "SoundCueGraph/SoundCueGraph.h"
#include "SoundCueGraph/SoundCueGraphNode_Root.h"
#include "SoundCueGraph/SoundCueGraphSchema.h"
#endif

/*-----------------------------------------------------------------------------
	USoundCue implementation.
-----------------------------------------------------------------------------*/

int32 USoundCue::CachedQualityLevel = -1;

#if WITH_EDITOR
TSharedPtr<ISoundCueAudioEditor> USoundCue::SoundCueAudioEditor = nullptr;
#endif

USoundCue::USoundCue(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	VolumeMultiplier = 0.75f;
	PitchMultiplier = 1.0f;
	SubtitlePriority = DEFAULT_SUBTITLE_PRIORITY;
}

#if WITH_EDITOR
void USoundCue::PostInitProperties()
{
	Super::PostInitProperties();
	if (!HasAnyFlags(RF_ClassDefaultObject | RF_NeedLoad))
	{
		CreateGraph();
	}

	CacheAggregateValues();
}


void USoundCue::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	USoundCue* This = CastChecked<USoundCue>(InThis);

	Collector.AddReferencedObject(This->SoundCueGraph, This);

	Super::AddReferencedObjects(InThis, Collector);
}
#endif // WITH_EDITOR

void USoundCue::CacheAggregateValues()
{
	if (FirstNode)
	{
		FirstNode->ConditionalPostLoad();

		Duration = FirstNode->GetDuration();

		MaxDistance = FirstNode->GetMaxDistance();
		// If no sound cue nodes overrode the max distance, we need to check the base attenuation
		if (MaxDistance == 0.0f)
		{
			MaxDistance = USoundBase::GetMaxDistance();
		}

		bHasDelayNode = FirstNode->HasDelayNode();
		bHasConcatenatorNode = FirstNode->HasConcatenatorNode();
		bHasVirtualizeWhenSilent = FirstNode->IsVirtualizeWhenSilent();
	}
}

void USoundCue::Serialize(FStructuredArchive::FRecord Record)
{
	FArchive& UnderlyingArchive = Record.GetUnderlyingArchive();

	// Always force the duration to be updated when we are saving or cooking
	if (UnderlyingArchive.IsSaving() || UnderlyingArchive.IsCooking())
	{
		Duration = (FirstNode ? FirstNode->GetDuration() : 0.f);
		CacheAggregateValues();
	}

	Super::Serialize(Record);

	if (UnderlyingArchive.UE4Ver() >= VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT)
	{
		FStripDataFlags StripFlags(Record.EnterField(FIELD_NAME_TEXT("SoundCueStripFlags")));
#if WITH_EDITORONLY_DATA
		if (!StripFlags.IsEditorDataStripped())
		{
			Record << NAMED_FIELD(SoundCueGraph);
		}
#endif
	}
#if WITH_EDITOR
	else
	{
		Record << NAMED_FIELD(SoundCueGraph);
	}
#endif
}

void USoundCue::PostLoad()
{
	Super::PostLoad();

	// Game doesn't care if there are NULL graph nodes
#if WITH_EDITOR
	if (GIsEditor && !GetOutermost()->HasAnyPackageFlags(PKG_FilterEditorOnly))
	{
		// we should have a soundcuegraph unless we are contained in a package which is missing editor only data
		if (ensure(SoundCueGraph))
		{
			USoundCue::GetSoundCueAudioEditor()->RemoveNullNodes(this);
		}

		// Always load all sound waves in the editor
		for (USoundNode* SoundNode : AllNodes)
		{
			if (USoundNodeAssetReferencer* AssetReferencerNode = Cast<USoundNodeAssetReferencer>(SoundNode))
			{
				AssetReferencerNode->LoadAsset();
			}
		}
	}
	else
#endif
	if (GEngine && *GEngine->GameUserSettingsClass)
	{
		EvaluateNodes(false);
	}
	else
	{
		OnPostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddUObject(this, &USoundCue::OnPostEngineInit);
	}

	CacheAggregateValues();
}

bool USoundCue::CanBeClusterRoot() const
{
	return false;
}

bool USoundCue::CanBeInCluster() const
{
	return false;
}

void USoundCue::OnPostEngineInit()
{
	FCoreDelegates::OnPostEngineInit.Remove(OnPostEngineInitHandle);
	OnPostEngineInitHandle.Reset();

	EvaluateNodes(true);
}

void USoundCue::EvaluateNodes(bool bAddToRoot)
{
	if (CachedQualityLevel == -1)
	{
		// Use per-platform quality index override if one exists, otherwise use the quality level from the game settings.
		CachedQualityLevel = FPlatformCompressionUtilities::GetQualityIndexOverrideForCurrentPlatform();
		if (CachedQualityLevel < 0)
		{
			CachedQualityLevel = GEngine->GetGameUserSettings()->GetAudioQualityLevel();
		}
	}

	TArray<USoundNode*> NodesToEvaluate;
	NodesToEvaluate.Push(FirstNode);

	while (NodesToEvaluate.Num() > 0)
	{
		if (USoundNode* SoundNode = NodesToEvaluate.Pop(false))
		{
			if (USoundNodeAssetReferencer* AssetReferencerNode = Cast<USoundNodeAssetReferencer>(SoundNode))
			{
				AssetReferencerNode->ConditionalPostLoad();
				AssetReferencerNode->LoadAsset(bAddToRoot);
			}
			else if (USoundNodeQualityLevel* QualityLevelNode = Cast<USoundNodeQualityLevel>(SoundNode))
			{
				if (CachedQualityLevel < QualityLevelNode->ChildNodes.Num())
				{
					NodesToEvaluate.Add(QualityLevelNode->ChildNodes[CachedQualityLevel]);
				}
			}
			else
			{
				NodesToEvaluate.Append(SoundNode->ChildNodes);
			}
		}
	}
}


#if WITH_EDITOR

void USoundCue::RecursivelySetExcludeBranchCulling(USoundNode* CurrentNode)
{
	if (CurrentNode)
	{
		USoundNodeRandom* RandomNode = Cast<USoundNodeRandom>(CurrentNode);
		if (RandomNode)
		{
			RandomNode->bSoundCueExcludedFromBranchCulling = bExcludeFromRandomNodeBranchCulling;
			RandomNode->MarkPackageDirty();
		}
		for (USoundNode* ChildNode : CurrentNode->ChildNodes)
		{
			RecursivelySetExcludeBranchCulling(ChildNode);
		}
	}
}

void USoundCue::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property)
	{
		for (TObjectIterator<UAudioComponent> It; It; ++It)
		{
			if (It->Sound == this && It->bIsActive)
			{
				It->Stop();
				It->Play();
			}
		}

		// Propagate branch exclusion to child nodes which care (sound node random)
		RecursivelySetExcludeBranchCulling(FirstNode);
	}
}
#endif

void USoundCue::RecursiveFindAttenuation( USoundNode* Node, TArray<class USoundNodeAttenuation*> &OutNodes )
{
	RecursiveFindNode<USoundNodeAttenuation>( Node, OutNodes );
}

void USoundCue::RecursiveFindAllNodes( USoundNode* Node, TArray<class USoundNode*> &OutNodes )
{
	if( Node )
	{
		OutNodes.AddUnique( Node );

		// Recurse.
		const int32 MaxChildNodes = Node->GetMaxChildNodes();
		for( int32 ChildIndex = 0 ; ChildIndex < Node->ChildNodes.Num() && ChildIndex < MaxChildNodes ; ++ChildIndex )
		{
			RecursiveFindAllNodes( Node->ChildNodes[ ChildIndex ], OutNodes );
		}
	}
}

bool USoundCue::RecursiveFindPathToNode(USoundNode* CurrentNode, const UPTRINT CurrentHash, const UPTRINT NodeHashToFind, TArray<USoundNode*>& OutPath) const
{
	OutPath.Push(CurrentNode);
	if (CurrentHash == NodeHashToFind)
	{
		return true;
	}

	for (int32 ChildIndex = 0; ChildIndex < CurrentNode->ChildNodes.Num(); ++ChildIndex)
	{
		USoundNode* ChildNode = CurrentNode->ChildNodes[ChildIndex];
		if (ChildNode)
		{
			if (RecursiveFindPathToNode(ChildNode, USoundNode::GetNodeWaveInstanceHash(CurrentHash, ChildNode, ChildIndex), NodeHashToFind, OutPath))
			{
				return true;
			}
		}
	}

	OutPath.Pop();
	return false;
}

bool USoundCue::FindPathToNode(const UPTRINT NodeHashToFind, TArray<USoundNode*>& OutPath) const
{
	return RecursiveFindPathToNode(FirstNode, (UPTRINT)FirstNode, NodeHashToFind, OutPath);
}

void USoundCue::StaticAudioQualityChanged(int32 NewQualityLevel)
{
	if (CachedQualityLevel != NewQualityLevel)
	{
		FAudioCommandFence AudioFence;
		AudioFence.BeginFence();
		AudioFence.Wait();

		CachedQualityLevel = NewQualityLevel;

		if (GEngine)
		{
			for (TObjectIterator<USoundCue> SoundCueIt; SoundCueIt; ++SoundCueIt)
			{
				SoundCueIt->AudioQualityChanged();
			}
		}
		else
		{
			// PostLoad should have set up the delegate to fire EvaluateNodes once GEngine is initialized
		}
	}
}

void USoundCue::AudioQualityChanged()
{
	// First clear any references to assets that were loaded in the old child nodes
	TArray<USoundNode*> NodesToClearReferences;
	NodesToClearReferences.Push(FirstNode);

	while (NodesToClearReferences.Num() > 0)
	{
		if (USoundNode* SoundNode = NodesToClearReferences.Pop(false))
		{
			if (USoundNodeAssetReferencer* AssetReferencerNode = Cast<USoundNodeAssetReferencer>(SoundNode))
			{
				AssetReferencerNode->ClearAssetReferences();
			}
			else
			{
				NodesToClearReferences.Append(SoundNode->ChildNodes);
			}
		}
	}

	// Now re-evaluate the nodes to reassign the references to any objects that are still legitimately
	// referenced and load any new assets that are now referenced that were not previously
	EvaluateNodes(false);
}

FString USoundCue::GetDesc()
{
	FString Description = TEXT( "" );

	// Display duration
	const float CueDuration = GetDuration();
	if( CueDuration < INDEFINITELY_LOOPING_DURATION )
	{
		Description = FString::Printf( TEXT( "%3.2fs" ), CueDuration );
	}
	else
	{
		Description = TEXT( "Forever" );
	}

	// Display group
	Description += TEXT( " [" );
	Description += *GetSoundClass()->GetName();
	Description += TEXT( "]" );

	return Description;
}

int32 USoundCue::GetResourceSizeForFormat(FName Format)
{
	TArray<USoundNodeWavePlayer*> WavePlayers;
	RecursiveFindNode<USoundNodeWavePlayer>(FirstNode, WavePlayers);

	int32 ResourceSize = 0;
	for (int32 WaveIndex = 0; WaveIndex < WavePlayers.Num(); ++WaveIndex)
	{
		USoundWave* SoundWave = WavePlayers[WaveIndex]->GetSoundWave();
		if (SoundWave)
		{
			ResourceSize += SoundWave->GetResourceSizeForFormat(Format);
		}
	}

	return ResourceSize;
}

float USoundCue::GetMaxDistance() const
{
	return MaxDistance;
}

float USoundCue::GetDuration()
{
	// Always recalc the duration when in the editor as it could change
	if (GIsEditor || (Duration < SMALL_NUMBER) || HasDelayNode())
	{
		if (FirstNode)
		{
			Duration = FirstNode->GetDuration();
		}
	}

	return Duration;
}


bool USoundCue::ShouldApplyInteriorVolumes()
{
	// Only evaluate the sound class graph if we've not cached the result or if we're in editor
	if (GIsEditor || !bShouldApplyInteriorVolumesCached)
	{
		// After this, we'll have cached the value
		bShouldApplyInteriorVolumesCached = true;

		bShouldApplyInteriorVolumes = Super::ShouldApplyInteriorVolumes();

		// Only need to evaluate the sound cue graph if our super doesn't have apply interior volumes enabled
		if (!bShouldApplyInteriorVolumes)
		{
			TArray<UObject*> Children;
			GetObjectsWithOuter(this, Children);

			for (UObject* Child : Children)
			{
				if (USoundNodeSoundClass* SoundClassNode = Cast<USoundNodeSoundClass>(Child))
				{
					if (SoundClassNode->SoundClassOverride && SoundClassNode->SoundClassOverride->Properties.bApplyAmbientVolumes)
					{
						bShouldApplyInteriorVolumes = true;
						break;
					}
				}
			}
		}
	}

	return bShouldApplyInteriorVolumes;
}

bool USoundCue::IsPlayable() const
{
	return FirstNode != nullptr;
}

void USoundCue::Parse( FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances )
{
	if (FirstNode)
	{
		FirstNode->ParseNodes(AudioDevice,(UPTRINT)FirstNode,ActiveSound,ParseParams,WaveInstances);
	}
}

float USoundCue::GetVolumeMultiplier()
{
	return VolumeMultiplier;
}

float USoundCue::GetPitchMultiplier()
{
	return PitchMultiplier;
}

const FSoundAttenuationSettings* USoundCue::GetAttenuationSettingsToApply() const
{
	if (bOverrideAttenuation)
	{
		return &AttenuationOverrides;
	}
	return Super::GetAttenuationSettingsToApply();
}

float USoundCue::GetSubtitlePriority() const
{
	return SubtitlePriority;
}

bool USoundCue::GetSoundWavesWithCookedAnalysisData(TArray<USoundWave*>& OutSoundWaves)
{
	// Check this sound cue's wave players to see if any of their soundwaves have cooked analysis data
	TArray<USoundNodeWavePlayer*> WavePlayers;
	RecursiveFindNode<USoundNodeWavePlayer>(FirstNode, WavePlayers);

	bool bHasAnalysisData = false;
	for (USoundNodeWavePlayer* Player : WavePlayers)
	{
		USoundWave* SoundWave = Player->GetSoundWave();
		if (SoundWave && SoundWave->GetSoundWavesWithCookedAnalysisData(OutSoundWaves))
		{
			bHasAnalysisData = true;
		}
	}
	return bHasAnalysisData;
}

bool USoundCue::HasCookedFFTData() const
{
	// Check this sound cue's wave players to see if any of their soundwaves have cooked analysis data
	TArray<const USoundNodeWavePlayer*> WavePlayers;
	RecursiveFindNode<USoundNodeWavePlayer>(FirstNode, WavePlayers);

	for (const USoundNodeWavePlayer* Player : WavePlayers)
	{
		const USoundWave* SoundWave = Player->GetSoundWave();
		if (SoundWave && SoundWave->HasCookedFFTData())
		{
			return true;
		}
	}
	return false;
}

bool USoundCue::HasCookedAmplitudeEnvelopeData() const
{
	// Check this sound cue's wave players to see if any of their soundwaves have cooked analysis data
	TArray<const USoundNodeWavePlayer*> WavePlayers;
	RecursiveFindNode<USoundNodeWavePlayer>(FirstNode, WavePlayers);

	for (const USoundNodeWavePlayer* Player : WavePlayers)
	{
		const USoundWave* SoundWave = Player->GetSoundWave();
		if (SoundWave && SoundWave->HasCookedAmplitudeEnvelopeData())
		{
			return true;
		}
	}
	return false;
}

#if WITH_EDITOR
UEdGraph* USoundCue::GetGraph()
{ 
	return SoundCueGraph;
}

void USoundCue::CreateGraph()
{
	if (SoundCueGraph == nullptr)
	{
		SoundCueGraph = USoundCue::GetSoundCueAudioEditor()->CreateNewSoundCueGraph(this);
		SoundCueGraph->bAllowDeletion = false;

		// Give the schema a chance to fill out any required nodes (like the results node)
		const UEdGraphSchema* Schema = SoundCueGraph->GetSchema();
		Schema->CreateDefaultNodesForGraph(*SoundCueGraph);
	}
}

void USoundCue::ClearGraph()
{
	if (SoundCueGraph)
	{
		SoundCueGraph->Nodes.Empty();
		// Give the schema a chance to fill out any required nodes (like the results node)
		const UEdGraphSchema* Schema = SoundCueGraph->GetSchema();
		Schema->CreateDefaultNodesForGraph(*SoundCueGraph);
	}
}

void USoundCue::SetupSoundNode(USoundNode* InSoundNode, bool bSelectNewNode/* = true*/)
{
	// Create the graph node
	check(InSoundNode->GraphNode == NULL);

	USoundCue::GetSoundCueAudioEditor()->SetupSoundNode(SoundCueGraph, InSoundNode, bSelectNewNode);
}

void USoundCue::LinkGraphNodesFromSoundNodes()
{
	USoundCue::GetSoundCueAudioEditor()->LinkGraphNodesFromSoundNodes(this);
	CacheAggregateValues();
}

void USoundCue::CompileSoundNodesFromGraphNodes()
{
	USoundCue::GetSoundCueAudioEditor()->CompileSoundNodesFromGraphNodes(this);
}

void USoundCue::SetSoundCueAudioEditor(TSharedPtr<ISoundCueAudioEditor> InSoundCueAudioEditor)
{
	check(!SoundCueAudioEditor.IsValid());
	SoundCueAudioEditor = InSoundCueAudioEditor;
}

/** Gets the sound cue graph editor implementation. */
TSharedPtr<ISoundCueAudioEditor> USoundCue::GetSoundCueAudioEditor()
	{
	return SoundCueAudioEditor;
}
			

#endif
