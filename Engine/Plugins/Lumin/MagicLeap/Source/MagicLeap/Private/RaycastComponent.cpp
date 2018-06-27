// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "RaycastComponent.h"
#include "MagicLeapHMD.h"
#include "MagicLeapMath.h"
#include "AppFramework.h"
#include "Engine/Engine.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#if WITH_EDITOR
#include "Editor.h"
#endif
#include "MagicLeapPluginUtil.h" // for ML_INCLUDES_START/END

#if WITH_MLSDK
ML_INCLUDES_START
#include <ml_raycast.h>
ML_INCLUDES_END
#endif //WITH_MLSDK

class FRaycastTrackerImpl
{
public:
	FRaycastTrackerImpl()
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
			MLResult Result = MLRaycastCreate(&Tracker);
			if (Result != MLResult_Ok)
			{
				UE_LOG(LogMagicLeap, Error, TEXT("MLRaycastCreate failed with error %d."), Result);
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
			MLResult Result = MLRaycastDestroy(Tracker);
			UE_CLOG(Result != MLResult_Ok, LogMagicLeap, Error, TEXT("MLRaycastDestroy failed with error %d."), Result);
			Tracker = ML_INVALID_HANDLE;
		}
#endif //WITH_MLSDK
	}
};

#if WITH_MLSDK
ERaycastResultState MLToUnrealRaycastResultState(MLRaycastResultState state)
{
	switch (state)
	{
	case MLRaycastResultState_RequestFailed:
		return ERaycastResultState::RequestFailed;
	case MLRaycastResultState_HitObserved:
		return ERaycastResultState::HitObserved;
	case MLRaycastResultState_HitUnobserved:
		return ERaycastResultState::HitUnobserved;
	case MLRaycastResultState_NoCollision:
		return ERaycastResultState::NoCollision;
	}
	return ERaycastResultState::RequestFailed;
}
#endif //WITH_MLSDK

URaycastComponent::URaycastComponent()
	: Impl(new FRaycastTrackerImpl())
{
	// Make sure this component ticks
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	bAutoActivate = true;

#if WITH_EDITOR
	if (GIsEditor)
	{
		FEditorDelegates::PrePIEEnded.AddUObject(this, &URaycastComponent::PrePIEEnded);
	}
#endif
}

URaycastComponent::~URaycastComponent()
{
	delete Impl;
}

void URaycastComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if WITH_MLSDK
	if (!(IMagicLeapPlugin::Get().IsMagicLeapHMDValid() && MLHandleIsValid(Impl->Tracker)))
	{
		return;
	}

	for (auto& pair : PendingRequests)
	{
		MLRaycastResult result;
		MLResult APICallResult = MLRaycastGetResult(Impl->Tracker, pair.Key, &result);
		if (APICallResult == MLResult_Ok)
		{
			const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
			float WorldToMetersScale = AppFramework.IsInitialized() ? AppFramework.GetWorldToMetersScale() : 100.0f;

			// TODO: Should we apply this transform here or expect the user to use the result as a child of the XRPawn like the other features?
			// This being for raycast, we should probably apply the transform since the result might be used for other than just placing objects.
			FTransform Pose = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this);

			FRaycastHitResult hitResult;
			hitResult.HitState = MLToUnrealRaycastResultState(result.state);
			hitResult.HitPoint = Pose.TransformPosition(MagicLeap::ToFVector(result.hitpoint, WorldToMetersScale));
			hitResult.Normal = Pose.TransformVectorNoScale(MagicLeap::ToFVector(result.normal, 1.0f));
			hitResult.Confidence = result.confidence;
			hitResult.UserData = pair.Value.UserData;

			if (hitResult.HitPoint.ContainsNaN() || hitResult.Normal.ContainsNaN())
			{
				UE_LOG(LogMagicLeap, Error, TEXT("Raycast result contains NaNs."));
				hitResult.HitState = ERaycastResultState::RequestFailed;
			}

			pair.Value.ResultDelegate.ExecuteIfBound(hitResult);
			CompletedRequests.Add(pair.Key);
		}
		else if (APICallResult != MLResult_Pending)
		{
			UE_LOG(LogMagicLeap, Error, TEXT("MLRaycastGetResult failed with result %d."), APICallResult);
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

bool URaycastComponent::RequestRaycast(const FRaycastQueryParams& RequestParams, const FRaycastResultDelegate& ResultDelegate)
{
#if WITH_MLSDK
	if (!(IMagicLeapPlugin::Get().IsMagicLeapHMDValid() && Impl->Create()))
	{
		return false;
	}

	const FAppFramework& AppFramework = static_cast<FMagicLeapHMD*>(GEngine->XRSystem->GetHMDDevice())->GetAppFrameworkConst();
	float WorldToMetersScale = AppFramework.IsInitialized() ? AppFramework.GetWorldToMetersScale() : 100.0f;

	FTransform PoseInverse = UHeadMountedDisplayFunctionLibrary::GetTrackingToWorldTransform(this).Inverse();

	MLRaycastQuery query;
	query.position = MagicLeap::ToMLVector(PoseInverse.TransformPosition(RequestParams.Position), WorldToMetersScale);
	query.direction = MagicLeap::ToMLVectorNoScale(PoseInverse.TransformVectorNoScale(RequestParams.Direction));
	query.up_vector = MagicLeap::ToMLVectorNoScale(PoseInverse.TransformVectorNoScale(RequestParams.UpVector));
	query.width = static_cast<uint32>(RequestParams.Width);
	query.height = static_cast<uint32>(RequestParams.Height);
	query.collide_with_unobserved = RequestParams.CollideWithUnobserved;
	query.horizontal_fov_degrees = RequestParams.HorizontalFovDegrees;

	MLHandle Handle = ML_INVALID_HANDLE;
	MLResult Result = MLRaycastRequest(Impl->Tracker, &query, &Handle);
	if (Result != MLResult_Ok)
	{
		UE_LOG(LogMagicLeap, Error, TEXT("MLRaycastRequest failed with error %d."), Result);
		return false;
	}

	FRaycastRequestMetaData& requestMetaData = PendingRequests.Add(Handle);
	requestMetaData.UserData = RequestParams.UserData;
	requestMetaData.ResultDelegate = ResultDelegate;
#endif //WITH_MLSDK

	return true;
}

void URaycastComponent::FinishDestroy()
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
void URaycastComponent::PrePIEEnded(bool bWasSimulatingInEditor)
{
	Impl->Destroy();
}
#endif
