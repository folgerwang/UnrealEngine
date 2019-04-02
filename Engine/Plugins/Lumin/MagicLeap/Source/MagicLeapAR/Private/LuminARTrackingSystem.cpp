// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LuminARTrackingSystem.h"
#include "Engine/Engine.h"
#include "RHIDefinitions.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/WorldSettings.h"
#include "LuminARDevice.h"
#include "ARSessionConfig.h"


FLuminARImplementation::FLuminARImplementation()
	: LuminARDeviceInstance(nullptr)
	, bHasValidPose(false)
	, DeltaControlRotation(FRotator::ZeroRotator)
	, DeltaControlOrientation(FQuat::Identity)
	, LightEstimate(nullptr)
{
	LuminARDeviceInstance = FLuminARDevice::GetInstance();
	check(LuminARDeviceInstance);
}

FLuminARImplementation::~FLuminARImplementation()
{
}

/////////////////////////////////////////////////////////////////////////////////
// Begin FLuminARImplementation IHeadMountedDisplay Virtual Interface   //
////////////////////////////////////////////////////////////////////////////////


void* FLuminARImplementation::GetARSessionRawPointer()
{
#if PLATFORM_LUMIN
	return static_cast<void*>(FLuminARDevice::GetInstance()->GetARSessionRawPointer());
#endif
	ensureAlwaysMsgf(false, TEXT("FLuminARImplementation::GetARSessionRawPointer is unimplemented on current platform."));
	return nullptr;
}

void* FLuminARImplementation::GetGameThreadARFrameRawPointer()
{
#if PLATFORM_LUMIN
	return static_cast<void*>(FLuminARDevice::GetInstance()->GetGameThreadARFrameRawPointer());
#endif
	ensureAlwaysMsgf(false, TEXT("FLuminARImplementation::GetARSessionRawPointer is unimplemented on current platform."));
	return nullptr;
}

void FLuminARImplementation::OnARSystemInitialized()
{
}

bool FLuminARImplementation::OnStartARGameFrame(FWorldContext& WorldContext)
{
	if (LuminARDeviceInstance->GetIsLuminARSessionRunning())
	{
		bHasValidPose = (LuminARDeviceInstance->GetTrackingState() == ELuminARTrackingState::Tracking);

		if (LightEstimate == nullptr)
		{
			LightEstimate = NewObject<UARBasicLightEstimate>();
		}
		FLuminARLightEstimate LuminARLightEstimate = FLuminARDevice::GetInstance()->GetLatestLightEstimate();
		if (LuminARLightEstimate.bIsValid)
		{
			LightEstimate->SetLightEstimate(LuminARLightEstimate.RGBScaleFactor, LuminARLightEstimate.PixelIntensity);
		}
		else
		{
			LightEstimate = nullptr;
		}
	}
	return true;
}


EARTrackingQuality FLuminARImplementation::OnGetTrackingQuality() const
{
	if (!bHasValidPose)
	{
		return EARTrackingQuality::NotTracking;
	}

	return EARTrackingQuality::OrientationAndPosition;
}

void FLuminARImplementation::OnStartARSession(UARSessionConfig* SessionConfig)
{
	FLuminARDevice::GetInstance()->StartLuminARSessionRequest(SessionConfig);
}

void FLuminARImplementation::OnPauseARSession()
{
	FLuminARDevice::GetInstance()->PauseLuminARSession();
}

void FLuminARImplementation::OnStopARSession()
{
	FLuminARDevice::GetInstance()->PauseLuminARSession();
	FLuminARDevice::GetInstance()->ResetLuminARSession();
}

FARSessionStatus FLuminARImplementation::OnGetARSessionStatus() const
{
	return FLuminARDevice::GetInstance()->GetSessionStatus();
}

void FLuminARImplementation::OnSetAlignmentTransform(const FTransform& InAlignmentTransform)
{
	const FTransform& NewAlignmentTransform = InAlignmentTransform;

	TArray<UARTrackedGeometry*> AllTrackedGeometries = OnGetAllTrackedGeometries();
	for (UARTrackedGeometry* TrackedGeometry : AllTrackedGeometries)
	{
		TrackedGeometry->UpdateAlignmentTransform(NewAlignmentTransform);
	}

	TArray<UARPin*> AllARPins = OnGetAllPins();
	for (UARPin* SomePin : AllARPins)
	{
		SomePin->UpdateAlignmentTransform(NewAlignmentTransform);
	}
}

static ELuminARLineTraceChannel ConvertToLuminTraceChannels(EARLineTraceChannels TraceChannels)
{
	ELuminARLineTraceChannel LuminARTraceChannels = ELuminARLineTraceChannel::None;
	if (!!(TraceChannels & EARLineTraceChannels::FeaturePoint))
	{
		LuminARTraceChannels = LuminARTraceChannels | ELuminARLineTraceChannel::FeaturePoint;
	}
	if (!!(TraceChannels & EARLineTraceChannels::GroundPlane))
	{
		LuminARTraceChannels = LuminARTraceChannels | ELuminARLineTraceChannel::InfinitePlane;
	}
	if (!!(TraceChannels & EARLineTraceChannels::PlaneUsingBoundaryPolygon))
	{
		LuminARTraceChannels = LuminARTraceChannels | ELuminARLineTraceChannel::PlaneUsingBoundaryPolygon;
	}
	if (!!(TraceChannels & EARLineTraceChannels::PlaneUsingExtent))
	{
		LuminARTraceChannels = LuminARTraceChannels | ELuminARLineTraceChannel::PlaneUsingExtent;
	}
	return LuminARTraceChannels;
}

TArray<FARTraceResult> FLuminARImplementation::OnLineTraceTrackedObjects(const FVector2D ScreenCoord, EARLineTraceChannels TraceChannels)
{
	TArray<FARTraceResult> OutHitResults;
	FLuminARDevice::GetInstance()->ARLineTrace(ScreenCoord, ConvertToLuminTraceChannels(TraceChannels), OutHitResults);
	return OutHitResults;
}

TArray<FARTraceResult> FLuminARImplementation::OnLineTraceTrackedObjects(const FVector Start, const FVector End, EARLineTraceChannels TraceChannels)
{
	TArray<FARTraceResult> OutHitResults;
	FLuminARDevice::GetInstance()->ARLineTrace(Start, End, ConvertToLuminTraceChannels(TraceChannels), OutHitResults);
	return OutHitResults;
}

TArray<UARTrackedGeometry*> FLuminARImplementation::OnGetAllTrackedGeometries() const
{
	TArray<UARTrackedGeometry*> AllTrackedGeometry;
	FLuminARDevice::GetInstance()->GetAllTrackables<UARTrackedGeometry>(AllTrackedGeometry);
	return AllTrackedGeometry;
}

TArray<UARPin*> FLuminARImplementation::OnGetAllPins() const
{
	TArray<UARPin*> AllARPins;
	FLuminARDevice::GetInstance()->GetAllARPins(AllARPins);
	return AllARPins;
}

bool FLuminARImplementation::OnIsTrackingTypeSupported(EARSessionType SessionType) const
{
	return FLuminARDevice::GetInstance()->GetIsTrackingTypeSupported(SessionType);
}

UARLightEstimate* FLuminARImplementation::OnGetCurrentLightEstimate() const
{
	return LightEstimate;
}

UARPin* FLuminARImplementation::OnPinComponent(USceneComponent* ComponentToPin, const FTransform& PinToWorldTransform, UARTrackedGeometry* TrackedGeometry /*= nullptr*/, const FName DebugName /*= NAME_None*/)
{
	UARPin* NewARPin = nullptr;
	ELuminARFunctionStatus Status = FLuminARDevice::GetInstance()->CreateARPin(PinToWorldTransform, TrackedGeometry, ComponentToPin, DebugName, NewARPin);
	if (Status != ELuminARFunctionStatus::Success)
	{
		UE_LOG(LogLuminARImplementation, Warning, TEXT("OnPinComponent CreateARPin did not return success.  Status=%i"), static_cast<int32>(Status));
	}
	return NewARPin;
}

void FLuminARImplementation::OnRemovePin(UARPin* PinToRemove)
{
	FLuminARDevice::GetInstance()->RemoveARPin(PinToRemove);
}

TArray<FVector> FLuminARImplementation::OnGetPointCloud() const
{
	//TODO NEEDED
	return TArray<FVector>();
}


void FLuminARImplementation::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (LightEstimate != nullptr)
	{
		Collector.AddReferencedObject(LightEstimate);
	}
}
