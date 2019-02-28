// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ARBlueprintLibrary.h"
#include "Features/IModularFeature.h"
#include "Features/IModularFeatures.h"
#include "Engine/Engine.h"
#include "ARSystem.h"
#include "ARPin.h"
#include "ARTrackable.h"

TWeakPtr<FARSupportInterface , ESPMode::ThreadSafe> UARBlueprintLibrary::RegisteredARSystem = nullptr;

EARTrackingQuality UARBlueprintLibrary::GetTrackingQuality()
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->GetTrackingQuality();
	}
	else
	{
		return EARTrackingQuality::NotTracking;
	}
}

void UARBlueprintLibrary::StartARSession(UARSessionConfig* SessionConfig)
{
	if (SessionConfig == nullptr)
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		static const TCHAR NoSession_Warning[] = TEXT("Attempting to start an AR session witout a session config object");
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 3600.0f, FColor(255,48,16), NoSession_Warning);
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		return;
	}

	static const TCHAR NotARApp_Warning[] = TEXT("Attempting to start an AR session but there is no AR plugin configured. To use AR, enable the proper AR plugin in the Plugin Settings.");
	
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		ARSystem.Pin()->StartARSession(SessionConfig);
	}
	else
	{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 3600.0f, FColor(255,48,16), NotARApp_Warning);
#endif //!(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	}
}

void UARBlueprintLibrary::PauseARSession()
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		ARSystem.Pin()->PauseARSession();
	}
}

void UARBlueprintLibrary::StopARSession()
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		ARSystem.Pin()->StopARSession();
	}
}

FARSessionStatus UARBlueprintLibrary::GetARSessionStatus()
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->GetARSessionStatus();
	}
	else
	{
		return FARSessionStatus(EARSessionStatus::NotStarted);
	}
}
UARSessionConfig* UARBlueprintLibrary::GetSessionConfig()
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return &ARSystem.Pin()->AccessSessionConfig();
	}
	else
	{
		return nullptr;
	}
}


void UARBlueprintLibrary::SetAlignmentTransform( const FTransform& InAlignmentTransform )
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->SetAlignmentTransform( InAlignmentTransform );
	}
}


TArray<FARTraceResult> UARBlueprintLibrary::LineTraceTrackedObjects( const FVector2D ScreenCoord, bool bTestFeaturePoints, bool bTestGroundPlane, bool bTestPlaneExtents, bool bTestPlaneBoundaryPolygon )
{
	TArray<FARTraceResult> Result;
	
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		EARLineTraceChannels ActiveTraceChannels =
		(bTestFeaturePoints ? EARLineTraceChannels::FeaturePoint : EARLineTraceChannels::None) |
			(bTestGroundPlane ? EARLineTraceChannels::GroundPlane : EARLineTraceChannels::None) |
			(bTestPlaneExtents ? EARLineTraceChannels::PlaneUsingExtent : EARLineTraceChannels::None ) |
			(bTestPlaneBoundaryPolygon ? EARLineTraceChannels::PlaneUsingBoundaryPolygon : EARLineTraceChannels::None);
		
		Result = ARSystem.Pin()->LineTraceTrackedObjects(ScreenCoord, ActiveTraceChannels);
	}
	
	return Result;
}

TArray<FARTraceResult> UARBlueprintLibrary::LineTraceTrackedObjects3D(const FVector Start, const FVector End, bool bTestFeaturePoints, bool bTestGroundPlane, bool bTestPlaneExtents, bool bTestPlaneBoundaryPolygon)
{
	TArray<FARTraceResult> Result;

	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		EARLineTraceChannels ActiveTraceChannels =
			(bTestFeaturePoints ? EARLineTraceChannels::FeaturePoint : EARLineTraceChannels::None) |
			(bTestGroundPlane ? EARLineTraceChannels::GroundPlane : EARLineTraceChannels::None) |
			(bTestPlaneExtents ? EARLineTraceChannels::PlaneUsingExtent : EARLineTraceChannels::None) |
			(bTestPlaneBoundaryPolygon ? EARLineTraceChannels::PlaneUsingBoundaryPolygon : EARLineTraceChannels::None);

		Result = ARSystem.Pin()->LineTraceTrackedObjects(Start, End, ActiveTraceChannels);
	}

	return Result;
}

TArray<UARTrackedGeometry*> UARBlueprintLibrary::GetAllGeometries()
{
	TArray<UARTrackedGeometry*> Geometries;
	
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		Geometries = ARSystem.Pin()->GetAllTrackedGeometries();
	}
	return Geometries;
}

TArray<UARPin*> UARBlueprintLibrary::GetAllPins()
{
	TArray<UARPin*> Pins;
	
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		Pins = ARSystem.Pin()->GetAllPins();
	}
	return Pins;
}

bool UARBlueprintLibrary::IsSessionTypeSupported(EARSessionType SessionType)
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->IsSessionTypeSupported(SessionType);
	}
	return false;
}


void UARBlueprintLibrary::DebugDrawTrackedGeometry( UARTrackedGeometry* TrackedGeometry, UObject* WorldContextObject, FLinearColor Color, float OutlineThickness, float PersistForSeconds )
{
	UWorld* MyWorld = WorldContextObject->GetWorld();
	if (TrackedGeometry != nullptr && MyWorld != nullptr)
	{
		TrackedGeometry->DebugDraw(MyWorld, Color, OutlineThickness, PersistForSeconds);
	}
}

void UARBlueprintLibrary::DebugDrawPin( UARPin* ARPin, UObject* WorldContextObject, FLinearColor Color, float Scale, float PersistForSeconds )
{
	UWorld* MyWorld = WorldContextObject->GetWorld();
	if (ARPin != nullptr && MyWorld != nullptr)
	{
		ARPin->DebugDraw(MyWorld, Color, Scale, PersistForSeconds);
	}
}


UARLightEstimate* UARBlueprintLibrary::GetCurrentLightEstimate()
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->GetCurrentLightEstimate();
	}
	return nullptr;
}

UARPin* UARBlueprintLibrary::PinComponent( USceneComponent* ComponentToPin, const FTransform& PinToWorldTransform, UARTrackedGeometry* TrackedGeometry, const FName DebugName )
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->PinComponent( ComponentToPin, PinToWorldTransform, TrackedGeometry, DebugName );
	}
	return nullptr;
}

UARPin* UARBlueprintLibrary::PinComponentToTraceResult( USceneComponent* ComponentToPin, const FARTraceResult& TraceResult, const FName DebugName )
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->PinComponent( ComponentToPin, TraceResult, DebugName );
	}
	return nullptr;
}

void UARBlueprintLibrary::UnpinComponent( USceneComponent* ComponentToUnpin )
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		auto PinnedARSystem = ARSystem.Pin();
		TArray<UARPin*> AllPins = PinnedARSystem->GetAllPins();
		const int32 AllPinsCount = AllPins.Num();
		for (int32 i=0; i<AllPinsCount; ++i)
		{
			if (AllPins[i]->GetPinnedComponent() == ComponentToUnpin)
			{
				PinnedARSystem->RemovePin( AllPins[i] );
				return;
			}
		}
	}
}

void UARBlueprintLibrary::RemovePin( UARPin* PinToRemove )
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->RemovePin( PinToRemove );
	}
}



void UARBlueprintLibrary::RegisterAsARSystem(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& NewARSystem)
{
	RegisteredARSystem = NewARSystem;
}


const TWeakPtr<FARSupportInterface , ESPMode::ThreadSafe>& UARBlueprintLibrary::GetARSystem()
{
	return RegisteredARSystem;
}



float UARTraceResultLibrary::GetDistanceFromCamera( const FARTraceResult& TraceResult )
{
	return TraceResult.GetDistanceFromCamera();
}

FTransform UARTraceResultLibrary::GetLocalToTrackingTransform( const FARTraceResult& TraceResult )
{
	return TraceResult.GetLocalToTrackingTransform();
}

FTransform UARTraceResultLibrary::GetLocalToWorldTransform( const FARTraceResult& TraceResult )
{
	return TraceResult.GetLocalToWorldTransform();
}

UARTrackedGeometry* UARTraceResultLibrary::GetTrackedGeometry( const FARTraceResult& TraceResult )
{
	return TraceResult.GetTrackedGeometry();
}

EARLineTraceChannels UARTraceResultLibrary::GetTraceChannel( const FARTraceResult& TraceResult )
{
	return TraceResult.GetTraceChannel();
}

UARTextureCameraImage* UARBlueprintLibrary::GetCameraImage()
{
	UARTextureCameraImage* Image = nullptr;

	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		Image = ARSystem.Pin()->GetCameraImage();
	}
	return Image;
}

UARTextureCameraDepth* UARBlueprintLibrary::GetCameraDepth()
{
	UARTextureCameraDepth* Depth = nullptr;

	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		Depth = ARSystem.Pin()->GetCameraDepth();
	}
	return Depth;
}

TArray<UARPlaneGeometry*> UARBlueprintLibrary::GetAllTrackedPlanes()
{
	TArray<UARPlaneGeometry*> ResultList;
	
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		auto PinnedARSystem = ARSystem.Pin();
		for (UARTrackedGeometry* Geo : PinnedARSystem->GetAllTrackedGeometries())
		{
			if (UARPlaneGeometry* Item = Cast<UARPlaneGeometry>(Geo))
			{
				ResultList.Add(Item);
			}
		}
	}
	return ResultList;
}

TArray<UARTrackedPoint*> UARBlueprintLibrary::GetAllTrackedPoints()
{
	TArray<UARTrackedPoint*> ResultList;
	
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		auto PinnedARSystem = ARSystem.Pin();
		for (UARTrackedGeometry* Geo : PinnedARSystem->GetAllTrackedGeometries())
		{
			if (UARTrackedPoint* Item = Cast<UARTrackedPoint>(Geo))
			{
				ResultList.Add(Item);
			}
		}
	}
	return ResultList;
}

TArray<UARTrackedImage*> UARBlueprintLibrary::GetAllTrackedImages()
{
	TArray<UARTrackedImage*> ResultList;
	
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		auto PinnedARSystem = ARSystem.Pin();
		for (UARTrackedGeometry* Geo : PinnedARSystem->GetAllTrackedGeometries())
		{
			if (UARTrackedImage* Item = Cast<UARTrackedImage>(Geo))
			{
				ResultList.Add(Item);
			}
		}
	}
	return ResultList;
}

TArray<UAREnvironmentCaptureProbe*> UARBlueprintLibrary::GetAllTrackedEnvironmentCaptureProbes()
{
	TArray<UAREnvironmentCaptureProbe*> ResultList;
	
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		auto PinnedARSystem = ARSystem.Pin();
		for (UARTrackedGeometry* Geo : PinnedARSystem->GetAllTrackedGeometries())
		{
			if (UAREnvironmentCaptureProbe* Item = Cast<UAREnvironmentCaptureProbe>(Geo))
			{
				ResultList.Add(Item);
			}
		}
	}
	return ResultList;
}

bool UARBlueprintLibrary::AddManualEnvironmentCaptureProbe(FVector Location, FVector Extent)
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->AddManualEnvironmentCaptureProbe(Location, Extent);
	}
	return false;
}

EARWorldMappingState UARBlueprintLibrary::GetWorldMappingStatus()
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->GetWorldMappingStatus();
	}
	return EARWorldMappingState::NotAvailable;
}

TSharedPtr<FARSaveWorldAsyncTask, ESPMode::ThreadSafe> UARBlueprintLibrary::SaveWorld()
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->SaveWorld();
		
	}
	return TSharedPtr<FARSaveWorldAsyncTask, ESPMode::ThreadSafe>();
}

TSharedPtr<FARGetCandidateObjectAsyncTask, ESPMode::ThreadSafe> UARBlueprintLibrary::GetCandidateObject(FVector Location, FVector Extent)
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->GetCandidateObject(Location, Extent);
		
	}
	return TSharedPtr<FARGetCandidateObjectAsyncTask, ESPMode::ThreadSafe>();
}

TArray<FVector> UARBlueprintLibrary::GetPointCloud()
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->GetPointCloud();
	}
	return TArray<FVector>();
}

TArray<FARVideoFormat> UARBlueprintLibrary::GetSupportedVideoFormats(EARSessionType SessionType)
{
	auto ARSystem = GetARSystem();
	if (ARSystem.IsValid())
	{
		return ARSystem.Pin()->GetSupportedVideoFormats(SessionType);
	}
	return TArray<FARVideoFormat>();
}

UARCandidateImage* UARBlueprintLibrary::AddRuntimeCandidateImage(UARSessionConfig* SessionConfig, UTexture2D* CandidateTexture, FString FriendlyName, float PhysicalWidth)
{
	auto ARSystem = GetARSystem();
	if (ensure(ARSystem.IsValid()))
	{
		return ARSystem.Pin()->AddRuntimeCandidateImage(SessionConfig, CandidateTexture, FriendlyName, PhysicalWidth);
	}
	else
	{
		return nullptr;
	}
}

