// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "DisplayClusterSceneComponent.h"

#include "Config/DisplayClusterConfigTypes.h"
#include "Game/IPDisplayClusterGameManager.h"
#include "Input/IPDisplayClusterInputManager.h"
#include "Misc/DisplayClusterLog.h"

#include "DisplayClusterGlobals.h"
#include "IPDisplayCluster.h"


UDisplayClusterSceneComponent::UDisplayClusterSceneComponent(const FObjectInitializer& ObjectInitializer) :
	USceneComponent(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UDisplayClusterSceneComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UDisplayClusterSceneComponent::BeginDestroy()
{
	Super::BeginDestroy();
}

void UDisplayClusterSceneComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Update transform if attached to a tracker
	if (!Config.TrackerId.IsEmpty())
	{
		const IPDisplayClusterInputManager* const InputMgr = GDisplayCluster->GetPrivateInputMgr();
		if (InputMgr)
		{
			FVector loc;
			FQuat rot;
			const bool bLocAvail = InputMgr->GetTrackerLocation(Config.TrackerId, Config.TrackerCh, loc);
			const bool bRotAvail = InputMgr->GetTrackerQuat(Config.TrackerId, Config.TrackerCh, rot);

			if (bLocAvail && bRotAvail)
			{
				UE_LOG(LogDisplayClusterGame, Verbose, TEXT("%s[%s] update from tracker %s:%d - {loc %s} {quat %s}"),
					*GetName(), *GetId(), *Config.TrackerId, Config.TrackerCh, *loc.ToString(), *rot.ToString());

				// Update transform
				this->SetRelativeLocationAndRotation(loc, rot);
				// Force child transforms update
				UpdateChildTransforms(/*true*/);
			}
		}
	}
}

void UDisplayClusterSceneComponent::SetSettings(const FDisplayClusterConfigSceneNode* pConfig)
{
	check(pConfig);

	Config = *pConfig;

	// Convert m to cm
	Config.Loc *= 100.f;
}

bool UDisplayClusterSceneComponent::ApplySettings()
{
	// Take place in hierarchy
	if (!GetParentId().IsEmpty())
	{
		const IPDisplayClusterGameManager* const GameMgr = GDisplayCluster->GetPrivateGameMgr();
		if (GameMgr)
		{
			UE_LOG(LogDisplayClusterGame, Log, TEXT("Attaching %s to %s"), *GetId(), *GetParentId());
			UDisplayClusterSceneComponent* const pComp = GameMgr->GetNodeById(GetParentId());
			AttachToComponent(pComp, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
			//this->SetRelativeTransform(FTransform::Identity);
		}
	}

	// Set up location and rotation
	this->SetRelativeLocationAndRotation(Config.Loc, Config.Rot);

	return true;
}
