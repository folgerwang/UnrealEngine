// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PlanesComponent.h"
#include "Components/BoxComponent.h"
#include "MagicLeapHMD.h"
#include "AppFramework.h"
#include "MagicLeapMath.h"
#include "Engine/Engine.h"
#include "Kismet/KismetMathLibrary.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#if WITH_EDITOR
#include "Editor.h"
#endif


class FPlanesTrackerImpl
{
public:
	FPlanesTrackerImpl()
#if WITH_MLSDK
		: Tracker(ML_INVALID_HANDLE)
#endif //WITH_MLSDK
	{};

public:
#if WITH_MLSDK
	MLHandle Tracker;
#endif //WITH_MLSDK

public:
	bool Create()
	{
#if WITH_MLSDK
		if (!MLHandleIsValid(Tracker))
		{
			MLResult CreateResult = MLPlanesCreate(&Tracker);

			if (CreateResult != MLResult_Ok || !MLHandleIsValid(Tracker))
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Could not create planes tracker."));
				return false;
			}
		}
#endif //WITH_MLSDK
		return true;
	}

	void Destroy()
	{
#if WITH_MLSDK
		if (MLHandleIsValid(Tracker))
		{
			MLResult DestroyResult = MLPlanesDestroy(Tracker);
			if (DestroyResult != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Error destroying planes tracker."));
			}
			Tracker = ML_INVALID_HANDLE;
		}
#endif //WITH_MLSDK
	}
};

#if WITH_MLSDK
MLPlanesQueryFlags UnrealToMLPlanesQueryFlagMap(EPlaneQueryFlags QueryFlag)
{
	switch (QueryFlag)
	{
	case EPlaneQueryFlags::Vertical:
		return MLPlanesQueryFlag_Vertical;
	case EPlaneQueryFlags::Horizontal:
		return MLPlanesQueryFlag_Horizontal;
	case EPlaneQueryFlags::Arbitrary:
		return MLPlanesQueryFlag_Arbitrary;
	case EPlaneQueryFlags::OrientToGravity:
		return MLPlanesQueryFlag_OrientToGravity;
	case EPlaneQueryFlags::PreferInner:
		return MLPlanesQueryFlag_Inner;
	case EPlaneQueryFlags::IgnoreHoles:
		return MLPlanesQueryFlag_IgnoreHoles;
	case EPlaneQueryFlags::Ceiling:
		return MLPlanesQueryFlag_Semantic_Ceiling;
	case EPlaneQueryFlags::Floor:
		return MLPlanesQueryFlag_Semantic_Floor;
	case EPlaneQueryFlags::Wall:
		return MLPlanesQueryFlag_Semantic_Wall;
	}
	return static_cast<MLPlanesQueryFlags>(0);
}

EPlaneQueryFlags MLToUnrealPlanesQueryFlagMap(MLPlanesQueryFlags QueryFlag)
{
	switch (QueryFlag)
	{
	case MLPlanesQueryFlag_Vertical:
		return EPlaneQueryFlags::Vertical;
	case MLPlanesQueryFlag_Horizontal:
		return EPlaneQueryFlags::Horizontal;
	case MLPlanesQueryFlag_Arbitrary:
		return EPlaneQueryFlags::Arbitrary;
	case MLPlanesQueryFlag_OrientToGravity:
		return EPlaneQueryFlags::OrientToGravity;
	case MLPlanesQueryFlag_Inner:
		return EPlaneQueryFlags::PreferInner;
	case MLPlanesQueryFlag_IgnoreHoles:
		return EPlaneQueryFlags::IgnoreHoles;
	case MLPlanesQueryFlag_Semantic_Ceiling:
		return EPlaneQueryFlags::Ceiling;
	case MLPlanesQueryFlag_Semantic_Floor:
		return EPlaneQueryFlags::Floor;
	case MLPlanesQueryFlag_Semantic_Wall:
		return EPlaneQueryFlags::Wall;
	}
	return static_cast<EPlaneQueryFlags>(0);
}

MLPlanesQueryFlags UnrealToMLPlanesQueryFlags(const TArray<EPlaneQueryFlags>& QueryFlags)
{
	MLPlanesQueryFlags mlflags = static_cast<MLPlanesQueryFlags>(0);

	for (EPlaneQueryFlags flag : QueryFlags)
	{
		mlflags = static_cast<MLPlanesQueryFlags>(mlflags | UnrealToMLPlanesQueryFlagMap(flag));
	}

	return mlflags;
}

void MLToUnrealPlanesQueryFlags(uint32 QueryFlags, TArray<EPlaneQueryFlags>& OutPlaneFlags)
{
	static TArray<MLPlanesQueryFlags> AllMLFlags({
	  MLPlanesQueryFlag_Vertical,
	  MLPlanesQueryFlag_Horizontal,
	  MLPlanesQueryFlag_Arbitrary,
	  MLPlanesQueryFlag_OrientToGravity,
	  MLPlanesQueryFlag_Inner,
	  MLPlanesQueryFlag_IgnoreHoles,
	  MLPlanesQueryFlag_Semantic_Ceiling,
	  MLPlanesQueryFlag_Semantic_Floor,
	  MLPlanesQueryFlag_Semantic_Wall
	});

	OutPlaneFlags.Empty();

	for (MLPlanesQueryFlags flag : AllMLFlags)
	{
		if ((QueryFlags & static_cast<MLPlanesQueryFlags>(flag)) != 0)
		{
			OutPlaneFlags.Add(MLToUnrealPlanesQueryFlagMap(flag));
		}
	}
}
#endif //WITH_MLSDK

UPlanesComponent::UPlanesComponent()
	: QueryFlags({ EPlaneQueryFlags::Vertical, EPlaneQueryFlags::Horizontal, EPlaneQueryFlags::Arbitrary, EPlaneQueryFlags::PreferInner })
	, MaxResults(10)
	, MinHolePerimeter(50.0f)
	, MinPlaneArea(25.0f)
	, Impl(new FPlanesTrackerImpl())
{
	// Make sure this component ticks
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bAutoActivate = true;

	SearchVolume = CreateDefaultSubobject<UBoxComponent>(TEXT("SearchVolume"));
	SearchVolume->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
	SearchVolume->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SearchVolume->SetCanEverAffectNavigation(false);
	SearchVolume->CanCharacterStepUpOn = ECanBeCharacterBase::ECB_No;
	SearchVolume->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
	SearchVolume->SetGenerateOverlapEvents(false);
	// Recommended default box extents for meshing - 10m (5m radius)
	SearchVolume->SetBoxExtent(FVector(1000, 1000, 1000), false);

#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::PrePIEEnded.AddUObject(this, &UPlanesComponent::PrePIEEnded);
	}
#endif
}

UPlanesComponent::~UPlanesComponent()
{
	delete Impl;
	Impl = nullptr;
}

void UPlanesComponent::BeginPlay()
{
	Super::BeginPlay();
	Impl->Create();
}

void UPlanesComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
#if WITH_MLSDK
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!(IMagicLeapPlugin::Get().IsMagicLeapHMDValid()))
	{
		return;
	}

	FTransform PoseTransform = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this);

	for (auto& pair : PendingRequests)
	{
		uint32 outNumResults = 0;
		TArray<MLPlane> resultMLPlanes;
		resultMLPlanes.AddDefaulted(pair.Value.MaxResults);

		MLResult result = MLPlanesQueryGetResults(Impl->Tracker, pair.Key, resultMLPlanes.GetData(), &outNumResults);
		switch (result)
		{
		case MLResult_Pending:
			// Intentionally skip. We'll continue to check until it has completed.
			break;
		case MLResult_UnspecifiedFailure:
		{
			pair.Value.ResultDelegate.ExecuteIfBound(false, TArray<FPlaneResult>(), pair.Value.UserData);
			CompletedRequests.Add(pair.Key);
			break;
		}
		case MLResult_Ok:
		{
			const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
			float WorldToMetersScale = AppFramework.GetWorldToMetersScale();

			TArray<FPlaneResult> Planes;
			Planes.Reserve(outNumResults);

			for (uint32 i = 0; i < outNumResults; ++i)
			{
				FPlaneResult resultPlane;
				// Perception uses all coordinates in RUB so for them X axis is right and corresponds to the width of the plane.
				// Unreal uses FRU, so the Y-axis is towards the right which makes the Y component of the vector the width.
				resultPlane.PlaneDimensions = FVector2D(resultMLPlanes[i].height * WorldToMetersScale, resultMLPlanes[i].width * WorldToMetersScale);

				FTransform planeTransform = FTransform(MagicLeap::ToFQuat(resultMLPlanes[i].rotation), MagicLeap::ToFVector(resultMLPlanes[i].position, WorldToMetersScale), FVector(1.0f, 1.0f, 1.0f));
				if (planeTransform.ContainsNaN())
				{
					UE_LOG(LogMagicLeap, Error, TEXT("Plane result %d transform contains NaN."), i);
					continue;
				}
				if (!planeTransform.GetRotation().IsNormalized())
				{
					FQuat rotation = planeTransform.GetRotation();
					rotation.Normalize();
					planeTransform.SetRotation(rotation);
				}

				planeTransform.ConcatenateRotation(FQuat(FVector(0, 0, 1), PI));
				planeTransform.AddToTranslation(PoseTransform.GetLocation());
				planeTransform.ConcatenateRotation(PoseTransform.Rotator().Quaternion());
				resultPlane.PlanePosition = planeTransform.GetLocation();
				resultPlane.PlaneOrientation = planeTransform.Rotator();
				// The plane orientation has the forward axis (X) pointing in the direction of the plane's normal.
				// We are rotating it by 90 degrees clock-wise about the right axis (Y) to get the up vector (Z) to point in the direction of the plane's normal.
				// Since we are rotating the axis, the rotation is in the opposite direction of the object i.e. -90 degrees.
				resultPlane.ContentOrientation = UKismetMathLibrary::Conv_VectorToRotator(UKismetMathLibrary::RotateAngleAxis(UKismetMathLibrary::Conv_RotatorToVector(resultPlane.PlaneOrientation), -90, UKismetMathLibrary::GetRightVector(resultPlane.PlaneOrientation)));
				resultPlane.ID = FGuid(resultMLPlanes[i].id, (resultMLPlanes[i].id >> 32), 0, 0);
				MLToUnrealPlanesQueryFlags(resultMLPlanes[i].flags, resultPlane.PlaneFlags);

				Planes.Add(resultPlane);
			}

			pair.Value.ResultDelegate.ExecuteIfBound(true, Planes, pair.Value.UserData);
			CompletedRequests.Add(pair.Key);
			break;
		}
		default:
			UE_LOG(LogMagicLeap, Warning, TEXT("Unexpected return code from MLPlanesQueryGetResults: %d"), result);		
		}
	}

	// TODO: Implement better strategy to optimize memory allocation.
	if (CompletedRequests.Num() > 0)
	{
		for (MLHandle handle : CompletedRequests)
		{
			PendingRequests.Remove(handle);
		}
		CompletedRequests.Empty();
	}
#endif //WITH_MLSDK
}

bool UPlanesComponent::RequestPlanes(int32 UserData, const FPlaneResultDelegate& ResultDelegate)
{
#if WITH_MLSDK
	if (!IMagicLeapPlugin::Get().IsMagicLeapHMDValid())
	{
		return false;
	}

	const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
	float WorldToMetersScale = AppFramework.GetWorldToMetersScale();
	check(WorldToMetersScale != 0);

	FTransform PoseInverse = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this).Inverse();
	FPlane plane;

	MLPlanesQuery query;
	query.max_results = static_cast<uint32>(MaxResults);
	query.flags = UnrealToMLPlanesQueryFlags(QueryFlags);
	query.min_hole_length = MinHolePerimeter / WorldToMetersScale;
	query.min_plane_area = MinPlaneArea / (WorldToMetersScale * WorldToMetersScale);
	query.bounds_center = MagicLeap::ToMLVector(PoseInverse.TransformPosition(SearchVolume->GetComponentLocation()), WorldToMetersScale);
	query.bounds_rotation = MagicLeap::ToMLQuat(PoseInverse.TransformRotation(SearchVolume->GetComponentQuat()));
	query.bounds_extents = MagicLeap::ToMLVectorExtents(SearchVolume->GetScaledBoxExtent(), WorldToMetersScale);

	MLHandle handle;
	MLResult QueryResult = MLPlanesQueryBegin(Impl->Tracker, &query, &handle);
	if (QueryResult != MLResult_Ok || !MLHandleIsValid(handle))
	{
		UE_LOG(LogMagicLeap, Error, TEXT("Could not request planes."));
		return false;
	}

	FPlanesRequestMetaData& requestMetaData = PendingRequests.Add(handle);
	requestMetaData.MaxResults = query.max_results;
	requestMetaData.UserData = UserData;
	requestMetaData.ResultDelegate = ResultDelegate;
#endif //WITH_MLSDK

	return true;
}

void UPlanesComponent::FinishDestroy()
{
#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::PrePIEEnded.RemoveAll(this);
	}
#endif
	Impl->Destroy();

	Super::FinishDestroy();
}

#if WITH_EDITOR
void UPlanesComponent::PrePIEEnded(bool bWasSimulatingInEditor)
{
	Impl->Destroy();
}
#endif
