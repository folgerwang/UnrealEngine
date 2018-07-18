// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ARSystem.h"
#include "IXRTrackingSystem.h"
#include "Features/IModularFeatures.h"
#include "ARBlueprintLibrary.h"
#include "ARBlueprintProxy.h"
#include "ARSessionConfig.h"
#include "GeneralProjectSettings.h"
#include "Engine/Engine.h"



FARSystemBase::FARSystemBase()
: AlignmentTransform(FTransform::Identity)
, ARSettings( NewObject<UARSessionConfig>() )
{
	// See Initialize(), as we need access to SharedThis()
}

void FARSystemBase::InitializeARSystem()
{
	// Register our ability to support Unreal AR API.
	IModularFeatures::Get().RegisterModularFeature(FARSystemBase::GetModularFeatureName(), this);
	
	UARBlueprintLibrary::RegisterAsARSystem( SharedThis(this) );
	UARBaseAsyncTaskBlueprintProxy::RegisterAsARSystem( SharedThis(this) );

	OnARSystemInitialized();
}

FARSystemBase::~FARSystemBase()
{
	IModularFeatures::Get().UnregisterModularFeature(FARSystemBase::GetModularFeatureName(), this);
	
	UARBlueprintLibrary::RegisterAsARSystem( nullptr );
	UARBaseAsyncTaskBlueprintProxy::RegisterAsARSystem( nullptr );
}

EARTrackingQuality FARSystemBase::GetTrackingQuality() const
{
	return OnGetTrackingQuality();
}

void FARSystemBase::StartARSession(UARSessionConfig* InSessionConfig)
{
    if (GetARSessionStatus().Status != EARSessionStatus::Running)
    {
		ARSettings = InSessionConfig;
        OnStartARSession(InSessionConfig);
    }
}

void FARSystemBase::PauseARSession()
{
	if (GetARSessionStatus().Status == EARSessionStatus::Running)
	{
		OnPauseARSession();
	}
}

void FARSystemBase::StopARSession()
{
	if (GetARSessionStatus().Status == EARSessionStatus::Running)
	{
		OnStopARSession();
	}
}

FARSessionStatus FARSystemBase::GetARSessionStatus() const
{
	return OnGetARSessionStatus();
}

TArray<FARTraceResult> FARSystemBase::LineTraceTrackedObjects( const FVector2D ScreenCoord, EARLineTraceChannels TraceChannels )
{
	return OnLineTraceTrackedObjects(ScreenCoord, TraceChannels);
}

TArray<UARTrackedGeometry*> FARSystemBase::GetAllTrackedGeometries() const
{
	return OnGetAllTrackedGeometries();
}

TArray<UARPin*> FARSystemBase::GetAllPins() const
{
	return OnGetAllPins();
}

UARTextureCameraImage* FARSystemBase::GetCameraImage()
{
	return OnGetCameraImage();
}

UARTextureCameraDepth* FARSystemBase::GetCameraDepth()
{
	return OnGetCameraDepth();
}

//@joeg -- ARKit 2.0 additions

bool FARSystemBase::AddManualEnvironmentCaptureProbe(FVector Location, FVector Extent)
{
	return OnAddManualEnvironmentCaptureProbe(Location, Extent);
}

TSharedPtr<FARGetCandidateObjectAsyncTask, ESPMode::ThreadSafe> FARSystemBase::GetCandidateObject(FVector Location, FVector Extent) const
{
	return OnGetCandidateObject(Location, Extent);
}

TSharedPtr<FARSaveWorldAsyncTask, ESPMode::ThreadSafe> FARSystemBase::SaveWorld() const
{
	return OnSaveWorld();
}

EARWorldMappingState FARSystemBase::GetWorldMappingStatus() const
{
	return OnGetWorldMappingStatus();
}
//@joeg -- End additions

bool FARSystemBase::IsSessionTypeSupported(EARSessionType SessionType) const
{
	return OnIsTrackingTypeSupported(SessionType);
}

void FARSystemBase::SetAlignmentTransform( const FTransform& InAlignmentTransform )
{
	OnSetAlignmentTransform(InAlignmentTransform);
	OnAlignmentTransformUpdated.Broadcast(InAlignmentTransform);

}


UARLightEstimate* FARSystemBase::GetCurrentLightEstimate() const
{
	return OnGetCurrentLightEstimate();
}

UARPin* FARSystemBase::PinComponent( USceneComponent* ComponentToPin, const FTransform& PinToWorldTransform, UARTrackedGeometry* TrackedGeometry, const FName DebugName )
{
	return OnPinComponent( ComponentToPin, PinToWorldTransform, TrackedGeometry, DebugName );
}

UARPin* FARSystemBase::PinComponent( USceneComponent* ComponentToPin, const FARTraceResult& HitResult, const FName DebugName )
{
	return OnPinComponent( ComponentToPin, HitResult.GetLocalToWorldTransform(), HitResult.GetTrackedGeometry(), DebugName );
}

void FARSystemBase::RemovePin( UARPin* PinToRemove )
{
	OnRemovePin( PinToRemove );
}

const FTransform& FARSystemBase::GetAlignmentTransform() const
{
	return AlignmentTransform;
}

const UARSessionConfig& FARSystemBase::GetSessionConfig() const
{
	check(ARSettings != nullptr);
	return *ARSettings;
}

UARSessionConfig& FARSystemBase::AccessSessionConfig()
{
	check(ARSettings != nullptr);
	return *ARSettings;
}

void FARSystemBase::SetAlignmentTransform_Internal(const FTransform& NewAlignmentTransform)
{
	AlignmentTransform = NewAlignmentTransform;
}

void FARSystemBase::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (ARSettings != nullptr)
	{
		Collector.AddReferencedObject(ARSettings);
	}
}

