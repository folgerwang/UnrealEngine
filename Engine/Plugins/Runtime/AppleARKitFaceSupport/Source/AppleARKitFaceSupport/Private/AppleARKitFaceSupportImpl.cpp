// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AppleARKitFaceSupportImpl.h"
#include "AppleARKitSettings.h"
#include "AppleARKitFaceMeshConversion.h"
#include "AppleARKitConversion.h"
#include "Async/TaskGraphInterfaces.h"
#include "ARSystem.h"
#include "Misc/ConfigCacheIni.h"
#include "AppleARKitFaceSupportModule.h"

DECLARE_CYCLE_STAT(TEXT("Conversion"), STAT_FaceAR_Conversion, STATGROUP_FaceAR);

#if SUPPORTS_ARKIT_1_0

static TSharedPtr<FAppleARKitAnchorData> MakeAnchorData(ARAnchor* Anchor, const FRotator& AdjustBy, EARFaceTrackingUpdate UpdateSetting)
{
	SCOPE_CYCLE_COUNTER(STAT_FaceAR_Conversion);
	
    TSharedPtr<FAppleARKitAnchorData> NewAnchor;
    if ([Anchor isKindOfClass:[ARFaceAnchor class]])
    {
        ARFaceAnchor* FaceAnchor = (ARFaceAnchor*)Anchor;
		FTransform LeftEyeTransform;
		FTransform RightEyeTransform;
		FVector LookAtTarget;
#if SUPPORTS_ARKIT_2_0
		if (FAppleARKitAvailability::SupportsARKit20())
		{
			LeftEyeTransform = FAppleARKitConversion::ToFTransform(FaceAnchor.leftEyeTransform, AdjustBy);
			RightEyeTransform = FAppleARKitConversion::ToFTransform(FaceAnchor.rightEyeTransform, AdjustBy);
			LookAtTarget = FAppleARKitConversion::ToFVector(FaceAnchor.lookAtPoint);
		}
#endif
		NewAnchor = MakeShared<FAppleARKitAnchorData>(
			FAppleARKitConversion::ToFGuid(FaceAnchor.identifier),
			FAppleARKitConversion::ToFTransform(FaceAnchor.transform, AdjustBy),
			ToBlendShapeMap(FaceAnchor.blendShapes, FAppleARKitConversion::ToFTransform(FaceAnchor.transform, AdjustBy), LeftEyeTransform, RightEyeTransform),
			UpdateSetting == EARFaceTrackingUpdate::CurvesAndGeo ? ToVertexBuffer(FaceAnchor.geometry.vertices, FaceAnchor.geometry.vertexCount) : TArray<FVector>(),
			LeftEyeTransform,
			RightEyeTransform,
			LookAtTarget
		);
        // Only convert from 16bit to 32bit once
        if (UpdateSetting == EARFaceTrackingUpdate::CurvesAndGeo && FAppleARKitAnchorData::FaceIndices.Num() == 0)
        {
            FAppleARKitAnchorData::FaceIndices = To32BitIndexBuffer(FaceAnchor.geometry.triangleIndices, FaceAnchor.geometry.triangleCount * 3);
        }
		NewAnchor->bIsTracked = FaceAnchor.isTracked;
    }

    return NewAnchor;
}

#endif

FAppleARKitFaceSupport::FAppleARKitFaceSupport()
{
}

FAppleARKitFaceSupport::~FAppleARKitFaceSupport()
{
	// Should only be called durirng shutdown
	check(GIsRequestingExit);
}

void FAppleARKitFaceSupport::Init()
{
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
}

void FAppleARKitFaceSupport::Shutdown()
{
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
}

#if SUPPORTS_ARKIT_1_0

ARConfiguration* FAppleARKitFaceSupport::ToARConfiguration(UARSessionConfig* SessionConfig)
{
	ARFaceTrackingConfiguration* SessionConfiguration = nullptr;
	if (SessionConfig->GetSessionType() == EARSessionType::Face)
	{
		if (ARFaceTrackingConfiguration.isSupported == FALSE)
		{
			return nullptr;
		}
		SessionConfiguration = [ARFaceTrackingConfiguration new];
	}

	// Copy / convert properties
	SessionConfiguration.lightEstimationEnabled = SessionConfig->GetLightEstimationMode() != EARLightEstimationMode::None;
	SessionConfiguration.providesAudioData = NO;
	SessionConfiguration.worldAlignment = FAppleARKitConversion::ToARWorldAlignment(SessionConfig->GetWorldAlignment());

#if SUPPORTS_ARKIT_1_5
	if (FAppleARKitAvailability::SupportsARKit15())
	{
		ARVideoFormat* Format = FAppleARKitConversion::ToARVideoFormat(SessionConfig->GetDesiredVideoFormat(), ARFaceTrackingConfiguration.supportedVideoFormats);
		if (Format != nullptr)
		{
			SessionConfiguration.videoFormat = Format;
		}
	}
#endif
	return SessionConfiguration;
}

TArray<TSharedPtr<FAppleARKitAnchorData>> FAppleARKitFaceSupport::MakeAnchorData(NSArray<ARAnchor*>* Anchors, double Timestamp, uint32 FrameNumber, const FRotator& AdjustBy, EARFaceTrackingUpdate UpdateSetting)
{
	TArray<TSharedPtr<FAppleARKitAnchorData>> AnchorList;

	for (ARAnchor* Anchor in Anchors)
	{
		TSharedPtr<FAppleARKitAnchorData> AnchorData = ::MakeAnchorData(Anchor, AdjustBy, UpdateSetting);
		if (AnchorData.IsValid())
		{
			AnchorList.Add(AnchorData);
		}
	}

	return AnchorList;
}

void FAppleARKitFaceSupport::PublishLiveLinkData(TSharedPtr<FAppleARKitAnchorData> Anchor, double Timestamp, uint32 FrameNumber)
{
	static bool bNeedsInit = true;
	if (bNeedsInit)
	{
		bNeedsInit = false;
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

	if (LiveLinkSource.IsValid())
	{
        LiveLinkSource->PublishBlendShapes(FaceTrackingLiveLinkSubjectName, Timestamp, FrameNumber, Anchor->BlendShapes);
	}
}

bool FAppleARKitFaceSupport::DoesSupportFaceAR()
{
	return ARFaceTrackingConfiguration.isSupported == TRUE;
}
#endif
#if SUPPORTS_ARKIT_1_5
TArray<FARVideoFormat> FAppleARKitFaceSupport::ToARConfiguration()
{
	return FAppleARKitConversion::FromARVideoFormatArray(ARFaceTrackingConfiguration.supportedVideoFormats);
}
#endif

