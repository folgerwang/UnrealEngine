// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AppleARKitFaceSupport.h"
#include "AppleARKitLiveLinkSourceFactory.h"
#include "AppleARKitConversion.h"


struct FAppleARKitFaceAnchorData :
	public FAppleARKitAnchorData
{
	FAppleARKitFaceAnchorData(FGuid InAnchorGuid, FTransform InTransform, FARBlendShapeMap InBlendShapes, TArray<FVector> InFaceVerts)
		: FAppleARKitAnchorData(InAnchorGuid, InTransform)
		, BlendShapes( MoveTemp(InBlendShapes) )
		, FaceVerts( MoveTemp(InFaceVerts) )
	{
	}

	FARBlendShapeMap BlendShapes;
	TArray<FVector> FaceVerts;
	// Note: the index buffer never changes so can be safely read once
	static TArray<int32> FaceIndices;
};

class FAppleARKitFaceSupport :
	public FAppleARKitFaceSupportBase
{
public:
	FAppleARKitFaceSupport(const TSharedRef<FARSystemBase, ESPMode::ThreadSafe>& InTrackingSystem, IAppleARKitFaceSupportCallback* Callback);
	virtual ~FAppleARKitFaceSupport();

private:
#if SUPPORTS_ARKIT_1_0
	// ~IAppleARKitFaceSupport
	virtual void ProcessAnchorAdd(NSArray<ARAnchor*>* NewAnchors, const FTransform& AlignmentTransform, uint32 FrameNumber, double Timestamp) override;
	virtual void ProcessAnchorUpdate(NSArray<ARAnchor*>* UpdatedAnchors, const FTransform& AlignmentTransform, uint32 FrameNumber, double Timestamp) override;
	virtual ARConfiguration* ToARConfiguration(UARSessionConfig* InSessionConfig, FAppleARKitConfiguration& InConfiguration) override;
	// ~IAppleARKitFaceSupport

	void ProcessAnchorAdd_Internal(TSharedRef<FAppleARKitFaceAnchorData> AnchorData);
	void ProcessAnchorUpdate_Internal(TSharedRef<FAppleARKitFaceAnchorData> AnchorData);
#endif
    
    IAppleARKitFaceSupportCallback* Callback;
	const TSharedRef<FARSystemBase, ESPMode::ThreadSafe> ARSystem;

	uint32 FrameNumber;
	double Timestamp;
	FTransform AlignmentTransform;

	/** If requested, publishes face ar updates to LiveLink for the animation system to use */
	TSharedPtr<ILiveLinkSourceARKit> LiveLinkSource;
	/** Copied from the UARSessionConfig project settings object */
	FName FaceTrackingLiveLinkSubjectName;
};

