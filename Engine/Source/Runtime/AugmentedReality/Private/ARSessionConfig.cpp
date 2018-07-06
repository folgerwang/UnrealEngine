// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ARSessionConfig.h"
#include "UObject/VRObjectVersion.h"

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

const TArray<UARCandidateImage*>& UARSessionConfig::GetCandidateImageList() const
{
	return CandidateImages;
}

int32 UARSessionConfig::GetMaxNumSimultaneousImagesTracked() const
{
    return MaxNumSimultaneousImagesTracked;
}

/** @see EnvironmentCaptureProbeType */
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

void UARSessionConfig::AddCandidateObject(UARCandidateObject* CandidateObject)
{
	if (CandidateObject != nullptr)
	{
		CandidateObjects.Add(CandidateObject);
	}
}

FARVideoFormat UARSessionConfig::GetDesiredVideoFormat() const
{
	return DesiredVideoFormat;
}

void UARSessionConfig::SetDesiredVideoFormat(FARVideoFormat NewFormat)
{
	DesiredVideoFormat = NewFormat;
}

void UARSessionConfig::Serialize(FArchive& Ar)
{
	Ar.UsingCustomVersion(FVRObjectVersion::GUID);

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
