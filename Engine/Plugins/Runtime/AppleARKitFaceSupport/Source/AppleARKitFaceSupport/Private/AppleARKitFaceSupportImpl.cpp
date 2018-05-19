// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AppleARKitFaceSupportImpl.h"
#include "AppleARKitSettings.h"
#include "AppleARKitFaceMeshConversion.h"
#include "AppleARKitConfiguration.h"
#include "Async/TaskGraphInterfaces.h"
#include "ARSystem.h"
#include "Misc/ConfigCacheIni.h"

DECLARE_STATS_GROUP(TEXT("AppleARKitFaceSupport"), STATGROUP_APPLEARKITFACE, STATCAT_Advanced);

uint32 LastTrackedFaceGeometryID = 0;

TArray<int32> FAppleARKitFaceAnchorData::FaceIndices;

#if SUPPORTS_ARKIT_1_0

static TSharedPtr<FAppleARKitFaceAnchorData> MakeAnchorData(ARAnchor* Anchor)
{
    TSharedPtr<FAppleARKitFaceAnchorData> NewAnchor;
    if ([Anchor isKindOfClass:[ARFaceAnchor class]])
    {
        ARFaceAnchor* FaceAnchor = (ARFaceAnchor*)Anchor;
        NewAnchor = MakeShared<FAppleARKitFaceAnchorData>(
			FAppleARKitConversion::ToFGuid(FaceAnchor.identifier),
			FAppleARKitConversion::ToFTransform(FaceAnchor.transform),
			ToBlendShapeMap(FaceAnchor.blendShapes, FAppleARKitConversion::ToFTransform(FaceAnchor.transform)),
			ToVertexBuffer(FaceAnchor.geometry.vertices, FaceAnchor.geometry.vertexCount)
        );
        // Only convert from 16bit to 32bit once
        if (FAppleARKitFaceAnchorData::FaceIndices.Num() == 0)
        {
            FAppleARKitFaceAnchorData::FaceIndices = To32BitIndexBuffer(FaceAnchor.geometry.triangleIndices, FaceAnchor.geometry.triangleCount * 3);
        }
    }

    return NewAnchor;
}

#endif

FAppleARKitFaceSupport::FAppleARKitFaceSupport(const TSharedRef<FARSystemBase, ESPMode::ThreadSafe>& InTrackingSystem, IAppleARKitFaceSupportCallback* InCallback)
    : Callback(InCallback)
	, ARSystem(InTrackingSystem)
    , FrameNumber(0)
    , Timestamp(0.0)
{
    check(Callback);

    // Create our LiveLink provider if the project setting is enabled
    if (GetDefault<UAppleARKitSettings>()->bEnableLiveLinkForFaceTracking)
    {
        FaceTrackingLiveLinkSubjectName = GetDefault<UAppleARKitSettings>()->DefaultFaceTrackingLiveLinkSubjectName;
#if PLATFORM_IOS
        LiveLinkSource = FAppleARKitLiveLinkSourceFactory::CreateLiveLinkSource(true);
#else
        // This should be started already, but just in case
        FAppleARKitLiveLinkSourceFactory::CreateLiveLinkRemoteListener();
#endif
    }
}

FAppleARKitFaceSupport::~FAppleARKitFaceSupport()
{

}

#if SUPPORTS_ARKIT_1_0

ARConfiguration* FAppleARKitFaceSupport::ToARConfiguration(UARSessionConfig* SessionConfig, FAppleARKitConfiguration& InConfiguration)
{
	ARConfiguration* SessionConfiguration = nullptr;
	if (SessionConfig->GetSessionType() == EARSessionType::Face)
	{
		if (ARFaceTrackingConfiguration.isSupported == FALSE)
		{
			return nullptr;
		}
		SessionConfiguration = [ARFaceTrackingConfiguration new];
	}

	// Copy / convert properties
	SessionConfiguration.lightEstimationEnabled = InConfiguration.bLightEstimationEnabled;
	SessionConfiguration.providesAudioData = InConfiguration.bProvidesAudioData;
	SessionConfiguration.worldAlignment = FAppleARKitConversion::ToARWorldAlignment(InConfiguration.Alignment);

	return SessionConfiguration;
}

void FAppleARKitFaceSupport::ProcessAnchorAdd(NSArray<ARAnchor*>* NewAnchors, const FTransform& InAlignmentTransform, uint32 InFrameNumber, double InTimestamp)
{
	DECLARE_CYCLE_STAT(TEXT("FaceAR::ProcessAnchorAdd"), STAT_FaceAR_ProcessAnchorAdd, STATGROUP_APPLEARKITFACE);

    FrameNumber = InFrameNumber;
    Timestamp = InTimestamp;

	for (ARAnchor* Anchor in NewAnchors)
	{
		TSharedPtr<FAppleARKitFaceAnchorData> AnchorData = MakeAnchorData(Anchor);
		if (AnchorData.IsValid())
		{
			auto AnchorTask = FSimpleDelegateGraphTask::FDelegate::CreateSP(this, &FAppleARKitFaceSupport::ProcessAnchorAdd_Internal, AnchorData.ToSharedRef());
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(AnchorTask, GET_STATID(STAT_FaceAR_ProcessAnchorAdd), nullptr, ENamedThreads::GameThread);
		}
	}
}

void FAppleARKitFaceSupport::ProcessAnchorUpdate(NSArray<ARAnchor*>* UpdatedAnchors, const FTransform& InAlignmentTransform, uint32 InFrameNumber, double InTimestamp)
{
	DECLARE_CYCLE_STAT(TEXT("FaceAR::ProcessAnchorUpdate"), STAT_FaceAR_ProcessAnchorUpdate, STATGROUP_APPLEARKITFACE);

    FrameNumber = InFrameNumber;
    Timestamp = InTimestamp;

	for (ARAnchor* Anchor in UpdatedAnchors)
	{
		TSharedPtr<FAppleARKitFaceAnchorData> AnchorData = MakeAnchorData(Anchor);
		if (AnchorData.IsValid())
		{
			auto AnchorTask = FSimpleDelegateGraphTask::FDelegate::CreateSP(this, &FAppleARKitFaceSupport::ProcessAnchorUpdate_Internal, AnchorData.ToSharedRef());
			FSimpleDelegateGraphTask::CreateAndDispatchWhenReady(AnchorTask, GET_STATID(STAT_FaceAR_ProcessAnchorUpdate), nullptr, ENamedThreads::GameThread);
		}
	}
}

void FAppleARKitFaceSupport::ProcessAnchorAdd_Internal(TSharedRef<FAppleARKitFaceAnchorData> AnchorData)
{
	if (AnchorData->AnchorType == EAppleAnchorType::FaceAnchor)
	{
        // Update LiveLink first, because the other updates use MoveTemp for efficiency
        if (LiveLinkSource.IsValid())
        {
            LiveLinkSource->PublishBlendShapes(FaceTrackingLiveLinkSubjectName, Timestamp, FrameNumber, AnchorData->BlendShapes);
        }

        FString NewAnchorDebugName;
        UARFaceGeometry* NewGeo = NewObject<UARFaceGeometry>();
        NewAnchorDebugName = FString::Printf(TEXT("FACE-%02d"), LastTrackedFaceGeometryID++);
        NewGeo->SetDebugName(FName(*NewAnchorDebugName));

        NewGeo->UpdateTrackedGeometry(ARSystem, FrameNumber, Timestamp, AnchorData->Transform, AlignmentTransform, AnchorData->BlendShapes, AnchorData->FaceVerts, AnchorData->FaceIndices);

        // Add it to the ar system since that's what other APIs query against
        Callback->AddTrackedGeometry(AnchorData->AnchorGUID, NewGeo);
    }
}

void FAppleARKitFaceSupport::ProcessAnchorUpdate_Internal(TSharedRef<FAppleARKitFaceAnchorData> AnchorData)
{
	UARTrackedGeometry* FoundGeometry = Callback->GetTrackedGeometry(AnchorData->AnchorGUID);
	if (ensure(FoundGeometry != nullptr))
	{
		if (UARFaceGeometry* FaceGeo = Cast<UARFaceGeometry>(FoundGeometry))
		{
			// Update LiveLink first, because the other updates use MoveTemp for efficiency
			if (LiveLinkSource.IsValid())
			{
				LiveLinkSource->PublishBlendShapes(FaceTrackingLiveLinkSubjectName, Timestamp, FrameNumber, AnchorData->BlendShapes);
			}

			const TArray<UARPin*>& Pins = ARSystem->GetAllPins();
			TArray<UARPin*> PinsToUpdate = ARKitUtil::PinsFromGeometry(FoundGeometry, Pins);

			// We figure out the delta transform for the Anchor (aka. TrackedGeometry in ARKit) and apply that
			// delta to figure out the new ARPin transform.
			const FTransform Anchor_LocalToTrackingTransform_PreUpdate = FoundGeometry->GetLocalToTrackingTransform_NoAlignment();
			const FTransform& Anchor_LocalToTrackingTransform_PostUpdate = AnchorData->Transform;

			const FTransform AnchorDeltaTransform = Anchor_LocalToTrackingTransform_PreUpdate.GetRelativeTransform(Anchor_LocalToTrackingTransform_PostUpdate);

			FaceGeo->UpdateTrackedGeometry(ARSystem, FrameNumber, Timestamp, AnchorData->Transform, AlignmentTransform, AnchorData->BlendShapes, AnchorData->FaceVerts, AnchorData->FaceIndices);
			for (UARPin* Pin : PinsToUpdate)
			{
				const FTransform Pin_LocalToTrackingTransform_PostUpdate = Pin->GetLocalToTrackingTransform_NoAlignment() * AnchorDeltaTransform;
				Pin->OnTransformUpdated(Pin_LocalToTrackingTransform_PostUpdate);
			}
		}
	}
}

#endif
