// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SequencerKeyActor.h"
#include "Components/StaticMeshComponent.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "SequencerEdMode.h"
#include "Materials/MaterialInstance.h" 
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Selection.h"
#include "EditorModeTools.h"
#include "EditorModeManager.h"
#include "Modules/ModuleManager.h"
#include "EditorViewportClient.h"
#include "UnrealClient.h"
#include "MovieScene.h"
#include "Channels/MovieSceneChannelProxy.h"

ASequencerKeyActor::ASequencerKeyActor()
	: Super()
{
	UStaticMesh* KeyEditorMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/VREditor/TransformGizmo/SM_Sequencer_Key"));
	check(KeyEditorMesh != nullptr);
	UMaterial* KeyEditorMaterial = LoadObject<UMaterial>(nullptr, TEXT("/Engine/VREditor/TransformGizmo/Main"));
	check(KeyEditorMaterial != nullptr);

	const bool bTransient = true;
	USceneComponent* SceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("SceneComponent"), bTransient);
	check(SceneComponent != nullptr);
	this->RootComponent = SceneComponent;

	KeyMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("KeyMesh"));
	KeyMeshComponent->SetMobility(EComponentMobility::Movable);
	KeyMeshComponent->SetupAttachment(RootComponent);
	KeyMeshComponent->SetStaticMesh(KeyEditorMesh);
	KeyMeshComponent->CreateAndSetMaterialInstanceDynamicFromMaterial(0, KeyEditorMaterial);

	KeyMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	KeyMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	KeyMeshComponent->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	
	KeyMeshComponent->SetGenerateOverlapEvents(false);
	KeyMeshComponent->SetCanEverAffectNavigation(false);
	KeyMeshComponent->bCastDynamicShadow = false;
	KeyMeshComponent->bCastStaticShadow = false;
	KeyMeshComponent->bAffectDistanceFieldLighting = false;
	KeyMeshComponent->bAffectDynamicIndirectLighting = false;
}


void ASequencerKeyActor::PostEditMove(bool bFinished)
{
	// Push the key's transform to the Sequencer track
	PropagateKeyChange();
	Super::PostEditMove(bFinished);
}

void ASequencerKeyActor::SetKeyData(class UMovieScene3DTransformSection* NewTrackSection, float NewKeyTime)
{
	TrackSection = NewTrackSection;
	KeyTime = NewKeyTime;
	// Associate the currently selected actor with this key
	AssociatedActor = GEditor->GetSelectedActors()->GetTop<AActor>();
	// Draw a single transform track based on the data from this key
	FEditorViewportClient* ViewportClient = StaticCast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
	if (ViewportClient != nullptr)
	{
		FSequencerEdMode* SequencerEdMode = (FSequencerEdMode*)(ViewportClient->GetModeTools()->GetActiveMode(FSequencerEdMode::EM_SequencerMode));
		SequencerEdMode->DrawMeshTransformTrailFromKey(this);
	}
}


void ASequencerKeyActor::PropagateKeyChange()
{
	if (TrackSection != nullptr)
	{
		// Mark the track section as dirty
		TrackSection->Modify();

		FFrameRate   TickResolution  = TrackSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
		FFrameNumber FrameNumber     = (KeyTime * TickResolution).RoundToFrame();

		TArrayView<FMovieSceneFloatChannel*> FloatChannels = TrackSection->GetChannelProxy().GetChannels<FMovieSceneFloatChannel>();

		const FVector Translation = GetActorTransform().GetLocation();
		FloatChannels[0]->GetData().UpdateOrAddKey(FrameNumber, FMovieSceneFloatValue(Translation.X));
		FloatChannels[1]->GetData().UpdateOrAddKey(FrameNumber, FMovieSceneFloatValue(Translation.Y));
		FloatChannels[2]->GetData().UpdateOrAddKey(FrameNumber, FMovieSceneFloatValue(Translation.Z));

		// Draw a single transform track based on the data from this key
		FEditorViewportClient* ViewportClient = StaticCast<FEditorViewportClient*>(GEditor->GetActiveViewport()->GetClient());
		if (ViewportClient != nullptr)
		{
			FSequencerEdMode* SequencerEdMode = (FSequencerEdMode*)(ViewportClient->GetModeTools()->GetActiveMode(FSequencerEdMode::EM_SequencerMode));
			SequencerEdMode->DrawMeshTransformTrailFromKey(this);
		}
	}
}

