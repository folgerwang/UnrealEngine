// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ARSupportInterface.h"
#include "ARTraceResult.h"
#include "ARSystem.h"
#include "Features/IModularFeatures.h"
#include "ARBlueprintLibrary.h"
#include "ARBlueprintProxy.h"
#include "Templates/SharedPointer.h"
#include "Engine/Texture2D.h"

FARSupportInterface ::FARSupportInterface (IARSystemSupport* InARImplementation, IXRTrackingSystem* InXRTrackingSystem)
	: ARImplemention(InARImplementation)
	, XRTrackingSystem(InXRTrackingSystem)
	, AlignmentTransform(FTransform::Identity)
	, ARSettings(NewObject<UARSessionConfig>())
{
}

FARSupportInterface ::~FARSupportInterface ()
{
	IModularFeatures::Get().UnregisterModularFeature(FARSupportInterface ::GetModularFeatureName(), this);
}

void FARSupportInterface ::InitializeARSystem()
{
	// Register our ability to support Unreal AR API.
	IModularFeatures::Get().RegisterModularFeature(FARSupportInterface ::GetModularFeatureName(), this);

	if (ARImplemention)
	{
		UARBlueprintLibrary::RegisterAsARSystem(AsShared());
		UARBaseAsyncTaskBlueprintProxy::RegisterAsARSystem(AsShared());

		ARImplemention->OnARSystemInitialized();
	}
}

IXRTrackingSystem* FARSupportInterface ::GetXRTrackingSystem()
{
	return XRTrackingSystem;
}

const FTransform& FARSupportInterface ::GetAlignmentTransform() const
{
	return AlignmentTransform;
}

const UARSessionConfig& FARSupportInterface ::GetSessionConfig() const
{
	check(ARSettings != nullptr);
	return *ARSettings;
}

UARSessionConfig& FARSupportInterface ::AccessSessionConfig()
{
	check(ARSettings != nullptr);
	return *ARSettings;
}

bool FARSupportInterface ::StartARGameFrame(FWorldContext& WorldContext)
{
	if (ARImplemention)
	{
		return ARImplemention->OnStartARGameFrame(WorldContext);
	}
	return false;
}


EARTrackingQuality FARSupportInterface ::GetTrackingQuality() const
{
	if (ARImplemention)
	{
		return ARImplemention->OnGetTrackingQuality();
	}
	return EARTrackingQuality::NotTracking;
}

void FARSupportInterface ::StartARSession(UARSessionConfig* InSessionConfig)
{
	if (ARImplemention)
	{
		ARSettings = InSessionConfig;
		ARImplemention->OnStartARSession(InSessionConfig);
	}
}

void FARSupportInterface ::PauseARSession()
{
	if (ARImplemention)
	{
		if (GetARSessionStatus().Status == EARSessionStatus::Running)
		{
			ARImplemention->OnPauseARSession();
		}
	}
}

void FARSupportInterface ::StopARSession()
{
	if (ARImplemention)
	{
		if (GetARSessionStatus().Status == EARSessionStatus::Running)
		{
			ARImplemention->OnStopARSession();
		}
	}
}

FARSessionStatus FARSupportInterface ::GetARSessionStatus() const
{
	if (ARImplemention)
	{
		return ARImplemention->OnGetARSessionStatus();
	}
	return EARSessionStatus::NotSupported;
}

bool FARSupportInterface ::IsSessionTypeSupported(EARSessionType SessionType) const
{
	if (ARImplemention)
	{
		return ARImplemention->OnIsTrackingTypeSupported(SessionType);
	}
	return false;
}

void FARSupportInterface ::SetAlignmentTransform(const FTransform& InAlignmentTransform)
{
	if (ARImplemention)
	{
		ARImplemention->OnSetAlignmentTransform(InAlignmentTransform);
	}
	AlignmentTransform = InAlignmentTransform;
	OnAlignmentTransformUpdated.Broadcast(InAlignmentTransform);
}

TArray<FARTraceResult> FARSupportInterface ::LineTraceTrackedObjects(const FVector2D ScreenCoord, EARLineTraceChannels TraceChannels)
{
	if (ARImplemention)
	{
		return ARImplemention->OnLineTraceTrackedObjects(ScreenCoord, TraceChannels);
	}
	return TArray<FARTraceResult>();
}

TArray<FARTraceResult> FARSupportInterface::LineTraceTrackedObjects(const FVector Start, const FVector End, EARLineTraceChannels TraceChannels)
{
	if (ARImplemention)
	{
		return ARImplemention->OnLineTraceTrackedObjects(Start, End, TraceChannels);
	}
	return TArray<FARTraceResult>();

}

TArray<UARTrackedGeometry*> FARSupportInterface ::GetAllTrackedGeometries() const
{
	if (ARImplemention)
	{
		return ARImplemention->OnGetAllTrackedGeometries();
	}
	return TArray<UARTrackedGeometry*>();
}

TArray<UARPin*> FARSupportInterface ::GetAllPins() const
{
	if (ARImplemention)
	{
		return ARImplemention->OnGetAllPins();
	}
	return TArray<UARPin*>();
}

UARTextureCameraImage* FARSupportInterface ::GetCameraImage()
{
	if (ARImplemention)
	{
		return ARImplemention->OnGetCameraImage();
	}
	return nullptr;
}

UARTextureCameraDepth* FARSupportInterface ::GetCameraDepth()
{
	if (ARImplemention)
	{
		return ARImplemention->OnGetCameraDepth();
	}
	return nullptr;
}

bool FARSupportInterface ::AddManualEnvironmentCaptureProbe(FVector Location, FVector Extent)
{
	if (ARImplemention)
	{
		return ARImplemention->OnAddManualEnvironmentCaptureProbe(Location, Extent);
	}
	return false;
}

TSharedPtr<FARGetCandidateObjectAsyncTask, ESPMode::ThreadSafe> FARSupportInterface ::GetCandidateObject(FVector Location, FVector Extent) const
{
	if (ARImplemention)
	{
		return ARImplemention->OnGetCandidateObject(Location, Extent);
	}
	return TSharedPtr<FARGetCandidateObjectAsyncTask, ESPMode::ThreadSafe>();
}

TSharedPtr<FARSaveWorldAsyncTask, ESPMode::ThreadSafe> FARSupportInterface ::SaveWorld() const
{
	if (ARImplemention)
	{
		return ARImplemention->OnSaveWorld();
	}
	return TSharedPtr<FARSaveWorldAsyncTask, ESPMode::ThreadSafe>();
}

EARWorldMappingState FARSupportInterface ::GetWorldMappingStatus() const
{
	if (ARImplemention)
	{
		return ARImplemention->OnGetWorldMappingStatus();
	}
	return EARWorldMappingState::NotAvailable;
}


UARLightEstimate* FARSupportInterface ::GetCurrentLightEstimate() const
{
	if (ARImplemention)
	{
		return ARImplemention->OnGetCurrentLightEstimate();
	}
	return nullptr;
}

UARPin* FARSupportInterface ::PinComponent(USceneComponent* ComponentToPin, const FTransform& PinToWorldTransform, UARTrackedGeometry* TrackedGeometry, const FName DebugName)
{
	if (ARImplemention)
	{
		return ARImplemention->OnPinComponent(ComponentToPin, PinToWorldTransform, TrackedGeometry, DebugName);
	}
	return nullptr;
}

UARPin* FARSupportInterface ::PinComponent(USceneComponent* ComponentToPin, const FARTraceResult& HitResult, const FName DebugName)
{
	if (ARImplemention)
	{
		return ARImplemention->OnPinComponent(ComponentToPin, HitResult.GetLocalToWorldTransform(), HitResult.GetTrackedGeometry(), DebugName);
	}
	return nullptr;
}

void FARSupportInterface ::RemovePin(UARPin* PinToRemove)
{
	if (ARImplemention)
	{
		ARImplemention->OnRemovePin(PinToRemove);
	}
}

TArray<FARVideoFormat> FARSupportInterface ::GetSupportedVideoFormats(EARSessionType SessionType) const
{
	if (ARImplemention)
	{
		return ARImplemention->OnGetSupportedVideoFormats(SessionType);
	}
	return TArray<FARVideoFormat>();
}

/** @return the current point cloud data for the ar scene */
TArray<FVector> FARSupportInterface ::GetPointCloud() const
{
	if (ARImplemention)
	{
		return ARImplemention->OnGetPointCloud();
	}
	return TArray<FVector>();
}

UARCandidateImage* FARSupportInterface::AddRuntimeCandidateImage(UARSessionConfig* SessionConfig, UTexture2D* CandidateTexture, FString FriendlyName, float PhysicalWidth)
{
	if (ARImplemention && ARImplemention->OnAddRuntimeCandidateImage(SessionConfig, CandidateTexture, FriendlyName, PhysicalWidth))
	{
		float PhysicalHeight = PhysicalWidth / CandidateTexture->GetSizeX() * CandidateTexture->GetSizeY();
		UARCandidateImage* NewCandidateImage = UARCandidateImage::CreateNewARCandidateImage(CandidateTexture, FriendlyName, PhysicalWidth, PhysicalHeight, EARCandidateImageOrientation::Landscape);
		SessionConfig->AddCandidateImage(NewCandidateImage);
		return NewCandidateImage;
	}
	else
	{
		return nullptr;
	}
}

void* FARSupportInterface ::GetARSessionRawPointer()
{
	if (ARImplemention)
	{
		return ARImplemention->GetARSessionRawPointer();
	}
	return nullptr;
}

void* FARSupportInterface ::GetGameThreadARFrameRawPointer()
{
	if (ARImplemention)
	{
		return ARImplemention->GetGameThreadARFrameRawPointer();
	}
	return nullptr;
}


void FARSupportInterface ::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (ARSettings != nullptr)
	{
		Collector.AddReferencedObject(ARSettings);
	}
}
