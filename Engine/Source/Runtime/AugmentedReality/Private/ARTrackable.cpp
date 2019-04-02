// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "ARTrackable.h"
#include "ARSystem.h"
#include "ARDebugDrawHelpers.h"
#include "DrawDebugHelpers.h"

//
//
//
UARTrackedGeometry::UARTrackedGeometry()
:TrackingState(EARTrackingState::Tracking)
,NativeResource(nullptr)
{
	
}

void UARTrackedGeometry::InitializeNativeResource(IARRef* InNativeResource)
{
	NativeResource.Reset(InNativeResource);
}

void UARTrackedGeometry::DebugDraw( UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds ) const
{
	const FTransform WorldTrans = GetLocalToWorldTransform();
	const FVector Location = WorldTrans.GetLocation();
	const FRotator Rotation = FRotator(WorldTrans.GetRotation());
	const FVector Scale3D = WorldTrans.GetScale3D();
	DrawDebugCoordinateSystem(World, Location, Rotation, Scale3D.X, true, PersistForSeconds, 0, OutlineThickness);
}

TSharedPtr<FARSupportInterface , ESPMode::ThreadSafe> UARTrackedGeometry::GetARSystem() const
{
	auto MyARSystem = ARSystem.Pin();
	return MyARSystem;
}

FTransform UARTrackedGeometry::GetLocalToTrackingTransform() const
{
	return LocalToAlignedTrackingTransform;
}

FTransform UARTrackedGeometry::GetLocalToTrackingTransform_NoAlignment() const
{
	return LocalToTrackingTransform;
}

EARTrackingState UARTrackedGeometry::GetTrackingState() const
{
	return TrackingState;
}

bool UARTrackedGeometry::IsTracked() const
{
	return TrackingState == EARTrackingState::Tracking;
}

void UARTrackedGeometry::SetTrackingState(EARTrackingState NewState)
{
	TrackingState = NewState;
}

FTransform UARTrackedGeometry::GetLocalToWorldTransform() const
{
	return GetLocalToTrackingTransform() * GetARSystem()->GetXRTrackingSystem()->GetTrackingToWorldTransform();
}

int32 UARTrackedGeometry::GetLastUpdateFrameNumber() const
{
	return LastUpdateFrameNumber;
}

FName UARTrackedGeometry::GetDebugName() const
{
	return DebugName;
}

float UARTrackedGeometry::GetLastUpdateTimestamp() const
{
	return LastUpdateTimestamp;
}

void UARTrackedGeometry::UpdateTrackedGeometry(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform )
{
	ARSystem = InTrackingSystem;
	LocalToTrackingTransform = InLocalToTrackingTransform;
	LastUpdateFrameNumber = FrameNumber;
	LastUpdateTimestamp = Timestamp;
	UpdateAlignmentTransform(InAlignmentTransform);
}

void UARTrackedGeometry::UpdateTrackingState( EARTrackingState NewTrackingState )
{
	TrackingState = NewTrackingState;

	if (TrackingState == EARTrackingState::StoppedTracking && NativeResource)
	{
		// Remove reference to the native resource since the tracked geometry is stopped tracking.
		NativeResource->RemoveRef();
	}
}

void UARTrackedGeometry::UpdateAlignmentTransform( const FTransform& NewAlignmentTransform )
{
	LocalToAlignedTrackingTransform = LocalToTrackingTransform * NewAlignmentTransform;
}

void UARTrackedGeometry::SetDebugName( FName InDebugName )
{
	DebugName = InDebugName;
}

IARRef* UARTrackedGeometry::GetNativeResource()
{
	return NativeResource.Get();
}

//
//
//
void UARPlaneGeometry::UpdateTrackedGeometry(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform, const FVector InCenter, const FVector InExtent )
{
	Super::UpdateTrackedGeometry(InTrackingSystem, FrameNumber, Timestamp, InLocalToTrackingTransform, InAlignmentTransform);
	Center = InCenter;
	Extent = InExtent;
	
	BoundaryPolygon.Empty(4);
	BoundaryPolygon.Add(FVector(-Extent.X, -Extent.Y, 0.0f));
	BoundaryPolygon.Add(FVector(Extent.X, -Extent.Y, 0.0f));
	BoundaryPolygon.Add(FVector(Extent.X, Extent.Y, 0.0f));
	BoundaryPolygon.Add(FVector(-Extent.X, Extent.Y, 0.0f));

	SubsumedBy = nullptr;
}

void UARPlaneGeometry::UpdateTrackedGeometry(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform, const FVector InCenter, const FVector InExtent, const TArray<FVector>& InBoundingPoly, UARPlaneGeometry* InSubsumedBy)
{
	Super::UpdateTrackedGeometry(InTrackingSystem, FrameNumber, Timestamp, InLocalToTrackingTransform, InAlignmentTransform);
	Center = InCenter;
	Extent = InExtent;

	BoundaryPolygon = InBoundingPoly;

	SubsumedBy = InSubsumedBy;
}

void UARPlaneGeometry::DebugDraw( UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds ) const
{
	const FTransform LocalToWorldTransform = GetLocalToWorldTransform();
	const FColor OutlineRGB = OutlineColor.ToFColor(false);
	if (BoundaryPolygon.Num() > 2)
	{
		FVector LastVert = LocalToWorldTransform.TransformPosition(BoundaryPolygon[0]);
		for (int32 i=1; i<BoundaryPolygon.Num(); ++i)
		{
			const FVector NewVert = LocalToWorldTransform.TransformPosition(BoundaryPolygon[i]);
			DrawDebugLine(World, LastVert, NewVert, OutlineRGB);
			LastVert = NewVert;
		}
		DrawDebugLine(World, LastVert, LocalToWorldTransform.TransformPosition(BoundaryPolygon[0]), OutlineRGB);
	}

	const FVector WorldSpaceCenter = LocalToWorldTransform.TransformPosition(Center);
	DrawDebugBox( World, WorldSpaceCenter, Extent, LocalToWorldTransform.GetRotation(), OutlineRGB, false, PersistForSeconds, 0, 0.1f*OutlineThickness );
	
	const FString CurAnchorDebugName = GetDebugName().ToString();
	ARDebugHelpers::DrawDebugString( World, WorldSpaceCenter, CurAnchorDebugName, 0.25f*OutlineThickness, OutlineRGB, PersistForSeconds, true);
}

void UARTrackedImage::DebugDraw(UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds /*= 0.0f*/) const
{
	const FTransform LocalToWorldTransform = GetLocalToWorldTransform();
	const FString CurAnchorDebugName = FString::Printf(TEXT("%s - %s"), *GetDebugName().ToString(), *DetectedImage->GetFriendlyName());
	const FColor OutlineRGB = OutlineColor.ToFColor(false);

	FVector Extent(DetectedImage->GetPhysicalHeight() / 2.f, DetectedImage->GetPhysicalWidth() / 2.f, 0.f);
	
	const FVector WorldSpaceCenter = LocalToWorldTransform.GetLocation();
	DrawDebugBox(World, WorldSpaceCenter, Extent, LocalToWorldTransform.GetRotation(), OutlineRGB, false, PersistForSeconds, 0, 0.1f * OutlineThickness);

	ARDebugHelpers::DrawDebugString(World, WorldSpaceCenter, CurAnchorDebugName, 0.25f * OutlineThickness, OutlineRGB, PersistForSeconds, true);
}

void UARTrackedImage::UpdateTrackedGeometry(const TSharedRef<FARSupportInterface, ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform, FVector2D InEstimatedSize, UARCandidateImage* InDetectedImage)
{
	Super::UpdateTrackedGeometry(InTrackingSystem, FrameNumber, Timestamp, InLocalToTrackingTransform, InAlignmentTransform);
	EstimatedSize = InEstimatedSize;
	DetectedImage = InDetectedImage;
}

FVector2D UARTrackedImage::GetEstimateSize()
{
	return EstimatedSize;
}

void UARFaceGeometry::UpdateFaceGeometry(const TSharedRef<FARSupportInterface, ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform, FARBlendShapeMap& InBlendShapes, TArray<FVector>& InVertices, const TArray<int32>& Indices, const FTransform& InLeftEyeTransform, const FTransform& InRightEyeTransform, const FVector& InLookAtTarget)
{
	Super::UpdateTrackedGeometry(InTrackingSystem, FrameNumber, Timestamp, InLocalToTrackingTransform, InAlignmentTransform);
	BlendShapes = MoveTemp(InBlendShapes);
	VertexBuffer = MoveTemp(InVertices);
	// This won't change ever so only copy first time
	if (IndexBuffer.Num() == 0)
	{
		IndexBuffer = Indices;
	}
	
	LeftEyeTransform = InLeftEyeTransform;
	RightEyeTransform = InRightEyeTransform;
	LookAtTarget = InLookAtTarget;
}

void UARTrackedPoint::DebugDraw(UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds /*= 0.0f*/) const
{
	const FTransform LocalToWorldTransform = GetLocalToWorldTransform();
	const FString CurAnchorDebugName = GetDebugName().ToString();
	const FColor OutlineRGB =OutlineColor.ToFColor(false);
	ARDebugHelpers::DrawDebugString( World, LocalToWorldTransform.GetLocation(), CurAnchorDebugName, 0.25f*OutlineThickness, OutlineRGB, PersistForSeconds, true);
	
	DrawDebugPoint(World, LocalToWorldTransform.GetLocation(), 0.5f, OutlineRGB, false, PersistForSeconds, 0);
}

void UARTrackedPoint::UpdateTrackedGeometry(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform)
{
	Super::UpdateTrackedGeometry(InTrackingSystem, FrameNumber, Timestamp, InLocalToTrackingTransform, InAlignmentTransform);
}

void UARFaceGeometry::DebugDraw( UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds ) const
{
	Super::DebugDraw(World, OutlineColor, OutlineThickness, PersistForSeconds);
}

float UARFaceGeometry::GetBlendShapeValue(EARFaceBlendShape BlendShape) const
{
	float Value = 0.f;
	if (BlendShapes.Contains(BlendShape))
	{
		Value = BlendShapes[BlendShape];
	}
	return Value;
}

const TMap<EARFaceBlendShape, float> UARFaceGeometry::GetBlendShapes() const
{
	return BlendShapes;
}

const FTransform& UARFaceGeometry::GetLocalSpaceEyeTransform(EAREye Eye) const
{
	if (Eye == EAREye::LeftEye)
	{
		return LeftEyeTransform;
	}
	return RightEyeTransform;
}

FTransform UARFaceGeometry::GetWorldSpaceEyeTransform(EAREye Eye) const
{
	if (Eye == EAREye::LeftEye)
	{
		return GetLocalToWorldTransform() * LeftEyeTransform;
	}
	return GetLocalToWorldTransform() * RightEyeTransform;
}

UAREnvironmentCaptureProbe::UAREnvironmentCaptureProbe()
	: Super()
	, EnvironmentCaptureTexture(nullptr)
{
}

void UAREnvironmentCaptureProbe::DebugDraw(UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds) const
{
	const FTransform LocalToWorldTransform = GetLocalToWorldTransform();
	const FString CurAnchorDebugName = GetDebugName().ToString();
	const FColor OutlineRGB =OutlineColor.ToFColor(false);

	ARDebugHelpers::DrawDebugString( World, LocalToWorldTransform.GetLocation(), CurAnchorDebugName, 0.25f * OutlineThickness, OutlineRGB, PersistForSeconds, true );
	
	DrawDebugBox( World, LocalToWorldTransform.GetLocation(), Extent, LocalToWorldTransform.GetRotation(), OutlineRGB, false, PersistForSeconds, 0, 0.1f * OutlineThickness );
}

void UAREnvironmentCaptureProbe::UpdateEnvironmentCapture(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& InTrackingSystem, uint32 InFrameNumber, double InTimestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform, FVector InExtent)
{
	Super::UpdateTrackedGeometry(InTrackingSystem, InFrameNumber, InTimestamp, InLocalToTrackingTransform, InAlignmentTransform);

	Extent = InExtent;
}

FVector UAREnvironmentCaptureProbe::GetExtent() const
{
	return Extent;
}

UAREnvironmentCaptureProbeTexture* UAREnvironmentCaptureProbe::GetEnvironmentCaptureTexture()
{
	return EnvironmentCaptureTexture;
}

void UARTrackedObject::DebugDraw(UWorld* World, const FLinearColor& OutlineColor, float OutlineThickness, float PersistForSeconds /*= 0.0f*/) const
{
	const FTransform LocalToWorldTransform = GetLocalToWorldTransform();
	const FString CurAnchorDebugName = GetDebugName().ToString();
	const FColor OutlineRGB =OutlineColor.ToFColor(false);
	ARDebugHelpers::DrawDebugString( World, LocalToWorldTransform.GetLocation(), CurAnchorDebugName, 0.25f*OutlineThickness, OutlineRGB, PersistForSeconds, true);
	
	DrawDebugPoint(World, LocalToWorldTransform.GetLocation(), 0.5f, OutlineRGB, false, PersistForSeconds, 0);
}

void UARTrackedObject::UpdateTrackedGeometry(const TSharedRef<FARSupportInterface , ESPMode::ThreadSafe>& InTrackingSystem, uint32 FrameNumber, double Timestamp, const FTransform& InLocalToTrackingTransform, const FTransform& InAlignmentTransform, UARCandidateObject* InDetectedObject)
{
	Super::UpdateTrackedGeometry(InTrackingSystem, FrameNumber, Timestamp, InLocalToTrackingTransform, InAlignmentTransform);
	DetectedObject = InDetectedObject;
}

