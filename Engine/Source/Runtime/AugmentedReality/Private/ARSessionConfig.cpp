// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ARSessionConfig.h"
#include "UObject/VRObjectVersion.h"
#include "Containers/StringConv.h"
#include "Misc/CoreMisc.h"
#include "ARSessionConfigCookSupport.h"

UARSessionConfig::UARSessionConfig()
: WorldAlignment(EARWorldAlignment::Gravity)
, SessionType(EARSessionType::World)
, PlaneDetectionMode_DEPRECATED(EARPlaneDetectionMode::HorizontalPlaneDetection)
, bHorizontalPlaneDetection(true)
, bVerticalPlaneDetection(true)
, bEnableAutoFocus(true)
, LightEstimationMode(EARLightEstimationMode::AmbientLightEstimate)
, FrameSyncMode(EARFrameSyncMode::SyncTickWithoutCameraImage)
, bEnableAutomaticCameraOverlay(true)
, bEnableAutomaticCameraTracking(true)
, bResetCameraTracking(true)
, bResetTrackedObjects(true)
, MaxNumSimultaneousImagesTracked(1)
{
}

EARWorldAlignment UARSessionConfig::GetWorldAlignment() const
{
	return WorldAlignment;
}

EARSessionType UARSessionConfig::GetSessionType() const
{
	return SessionType;
}

EARPlaneDetectionMode UARSessionConfig::GetPlaneDetectionMode() const
{
	return static_cast<EARPlaneDetectionMode>(
	(bHorizontalPlaneDetection ? static_cast<int32>(EARPlaneDetectionMode::HorizontalPlaneDetection) : 0) |
	(bVerticalPlaneDetection ? static_cast<int32>(EARPlaneDetectionMode::VerticalPlaneDetection) : 0));
}

EARLightEstimationMode UARSessionConfig::GetLightEstimationMode() const
{
	return LightEstimationMode;
}

EARFrameSyncMode UARSessionConfig::GetFrameSyncMode() const
{
	return FrameSyncMode;
}

bool UARSessionConfig::ShouldRenderCameraOverlay() const
{
	return bEnableAutomaticCameraOverlay;
}

bool UARSessionConfig::ShouldEnableCameraTracking() const
{
	return bEnableAutomaticCameraTracking;
}

bool UARSessionConfig::ShouldEnableAutoFocus() const
{
	return bEnableAutoFocus;
}

void UARSessionConfig::SetEnableAutoFocus(bool bNewValue)
{
	bEnableAutoFocus = bNewValue;
}

bool UARSessionConfig::ShouldResetCameraTracking() const
{
	return bResetCameraTracking;
}

void UARSessionConfig::SetResetCameraTracking(bool bNewValue)
{
	bResetCameraTracking = bNewValue;
}

bool UARSessionConfig::ShouldResetTrackedObjects() const
{
	return bResetTrackedObjects;
}

void UARSessionConfig::SetResetTrackedObjects(bool bNewValue)
{
	bResetTrackedObjects = bNewValue;
}

const TArray<UARCandidateImage*>& UARSessionConfig::GetCandidateImageList() const
{
	return CandidateImages;
}

void UARSessionConfig::AddCandidateImage(UARCandidateImage* NewCandidateImage)
{
	CandidateImages.Add(NewCandidateImage);
}

int32 UARSessionConfig::GetMaxNumSimultaneousImagesTracked() const
{
    return MaxNumSimultaneousImagesTracked;
}

EAREnvironmentCaptureProbeType UARSessionConfig::GetEnvironmentCaptureProbeType() const
{
	return EnvironmentCaptureProbeType;
}

const TArray<uint8>& UARSessionConfig::GetWorldMapData() const
{
	return WorldMapData;
}

void UARSessionConfig::SetWorldMapData(TArray<uint8> InWorldMapData)
{
	WorldMapData = MoveTemp(InWorldMapData);
}

const TArray<UARCandidateObject*>& UARSessionConfig::GetCandidateObjectList() const
{
	return CandidateObjects;
}

void UARSessionConfig::SetCandidateObjectList(const TArray<UARCandidateObject*>& InCandidateObjects)
{
	CandidateObjects = InCandidateObjects;
}

void UARSessionConfig::AddCandidateObject(UARCandidateObject* CandidateObject)
{
	if (CandidateObject != nullptr)
	{
		CandidateObjects.Add(CandidateObject);
	}
}

const TArray<uint8>& UARSessionConfig::GetSerializedARCandidateImageDatabase() const
{
	return SerializedARCandidateImageDatabase;
}

FARVideoFormat UARSessionConfig::GetDesiredVideoFormat() const
{
	return DesiredVideoFormat;
}

void UARSessionConfig::SetDesiredVideoFormat(FARVideoFormat NewFormat)
{
	DesiredVideoFormat = NewFormat;
}

EARFaceTrackingDirection UARSessionConfig::GetFaceTrackingDirection() const
{
	return FaceTrackingDirection;
}

void UARSessionConfig::SetFaceTrackingDirection(EARFaceTrackingDirection InDirection)
{
	FaceTrackingDirection = InDirection;
}

EARFaceTrackingUpdate UARSessionConfig::GetFaceTrackingUpdate() const
{
	return FaceTrackingUpdate;
}

void UARSessionConfig::SetFaceTrackingUpdate(EARFaceTrackingUpdate InUpdate)
{
	FaceTrackingUpdate = InUpdate;
}

void UARSessionConfig::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FVRObjectVersion::GUID);

#if WITH_EDITORONLY_DATA
	if (!Ar.IsLoading() && Ar.IsCooking())
	{
		TArray<IARSessionConfigCookSupport*> CookSupportModules = IModularFeatures::Get().GetModularFeatureImplementations<IARSessionConfigCookSupport>(IARSessionConfigCookSupport::GetModularFeatureName());
		for (IARSessionConfigCookSupport* CookSupportModule : CookSupportModules)
		{
			CookSupportModule->OnSerializeSessionConfig(this, Ar, SerializedARCandidateImageDatabase);
		}
	}
#endif

	Super::Serialize(Ar);

	if (Ar.CustomVer(FVRObjectVersion::GUID) < FVRObjectVersion::UseBoolsForARSessionConfigPlaneDetectionConfiguration)
	{
		if (PlaneDetectionMode_DEPRECATED == EARPlaneDetectionMode::None)
		{
			bHorizontalPlaneDetection = false;
			bVerticalPlaneDetection = false;
		}
	}
}
