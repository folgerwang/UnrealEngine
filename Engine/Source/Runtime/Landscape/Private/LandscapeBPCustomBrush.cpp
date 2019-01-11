// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LandscapeBPCustomBrush.h"
#include "CoreMinimal.h"
#include "LandscapeProxy.h"
#include "Landscape.h"

#define LOCTEXT_NAMESPACE "Landscape"

ALandscapeBlueprintCustomBrush::ALandscapeBlueprintCustomBrush(const FObjectInitializer& ObjectInitializer)
#if WITH_EDITORONLY_DATA
	: OwningLandscape(nullptr)
	, bIsCommited(false)
	, bIsInitialized(false)
#endif
{
	USceneComponent* SceneComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootComponent"));
	RootComponent = SceneComp;

	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_DuringPhysics;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.SetTickFunctionEnable(true);
	bIsEditorOnlyActor = true;
}

void ALandscapeBlueprintCustomBrush::Tick(float DeltaSeconds)
{
	// Forward the Tick to the instances class of this BP
	if (GetClass()->HasAnyClassFlags(CLASS_CompiledFromBlueprint))
	{
		TGuardValue<bool> AutoRestore(GAllowActorScriptExecutionInEditor, true);
		ReceiveTick(DeltaSeconds);
	}

	Super::Tick(DeltaSeconds);
}

bool ALandscapeBlueprintCustomBrush::ShouldTickIfViewportsOnly() const
{
	return true;
}

#if WITH_EDITOR

void ALandscapeBlueprintCustomBrush::SetCommitState(bool InCommited)
{
#if WITH_EDITORONLY_DATA
	bListedInSceneOutliner = !InCommited;
	bEditable = !InCommited;
	bIsCommited = InCommited;
#endif
}

void ALandscapeBlueprintCustomBrush::SetOwningLandscape(ALandscape* InOwningLandscape)
{
	OwningLandscape = InOwningLandscape;
}

ALandscape* ALandscapeBlueprintCustomBrush::GetOwningLandscape() const
{ 
	return OwningLandscape; 
}

void ALandscapeBlueprintCustomBrush::SetIsInitialized(bool InIsInitialized)
{
	bIsInitialized = InIsInitialized;
}

void ALandscapeBlueprintCustomBrush::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (OwningLandscape != nullptr)
	{
		OwningLandscape->RequestProceduralContentUpdate(bFinished ? EProceduralContentUpdateFlag::All : EProceduralContentUpdateFlag::All_Render);
	}
}

void ALandscapeBlueprintCustomBrush::PreEditChange(UProperty* PropertyThatWillChange)
{
	const FName PropertyName = PropertyThatWillChange != nullptr ? PropertyThatWillChange->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ALandscapeBlueprintCustomBrush, AffectHeightmap)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ALandscapeBlueprintCustomBrush, AffectWeightmap))
	{
		PreviousAffectHeightmap = AffectHeightmap;
		PreviousAffectWeightmap = AffectWeightmap;
	}
}

void ALandscapeBlueprintCustomBrush::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ALandscapeBlueprintCustomBrush, AffectHeightmap)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(ALandscapeBlueprintCustomBrush, AffectWeightmap))
	{
		for (FProceduralLayer& Layer : OwningLandscape->ProceduralLayers)
		{
			for (int32 i = 0; i < Layer.Brushes.Num(); ++i)
			{
				if (Layer.Brushes[i].BPCustomBrush == this)
				{
					if (AffectHeightmap && !PreviousAffectHeightmap) // changed to affect
					{
						Layer.HeightmapBrushOrderIndices.Add(i); // simply add the brush as the last one
					}
					else if (!AffectHeightmap && PreviousAffectHeightmap) // changed to no longer affect
					{
						for (int32 j = 0; j < Layer.HeightmapBrushOrderIndices.Num(); ++j)
						{
							if (Layer.HeightmapBrushOrderIndices[j] == i)
							{
								Layer.HeightmapBrushOrderIndices.RemoveAt(j);
								break;
							}
						}
					}

					if (AffectWeightmap && !PreviousAffectWeightmap) // changed to affect
					{
						Layer.WeightmapBrushOrderIndices.Add(i); // simply add the brush as the last one
					}
					else if (!AffectWeightmap && PreviousAffectWeightmap) // changed to no longer affect
					{
						for (int32 j = 0; j < Layer.WeightmapBrushOrderIndices.Num(); ++j)
						{
							if (Layer.WeightmapBrushOrderIndices[j] == i)
							{
								Layer.WeightmapBrushOrderIndices.RemoveAt(j);
								break;
							}
						}
					}

					PreviousAffectHeightmap = AffectHeightmap;
					PreviousAffectWeightmap = AffectWeightmap;

					break;
				}
			}			
		}		

		// Should trigger a rebuild of the UI so the visual is updated with changes made to actor
		//TODO: find a way to trigger the update of the UI
		//FEdModeLandscape* EdMode = (FEdModeLandscape*)GLevelEditorModeTools().GetActiveMode(FBuiltinEditorModes::EM_Landscape);
		//EdMode->RefreshDetailPanel();
	}

	if (OwningLandscape != nullptr)
	{
		OwningLandscape->RequestProceduralContentUpdate(EProceduralContentUpdateFlag::All);
	}
}
#endif

ALandscapeBlueprintCustomSimulationBrush::ALandscapeBlueprintCustomSimulationBrush(const FObjectInitializer& ObjectInitializer)
{
}

#undef LOCTEXT_NAMESPACE
