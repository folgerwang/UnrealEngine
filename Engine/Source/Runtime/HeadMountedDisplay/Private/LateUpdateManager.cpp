// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "LateUpdateManager.h"
#include "PrimitiveSceneProxy.h"
#include "Components/PrimitiveComponent.h"
#include "PrimitiveSceneInfo.h"

FLateUpdateManager::FLateUpdateManager() 
	: LateUpdateGameWriteIndex(0)
	, LateUpdateRenderReadIndex(0)
{
	SkipLateUpdate[0] = false;
	SkipLateUpdate[1] = false;
}

void FLateUpdateManager::Setup(const FTransform& ParentToWorld, USceneComponent* Component, bool bSkipLateUpdate)
{
	check(IsInGameThread());

	LateUpdateParentToWorld[LateUpdateGameWriteIndex] = ParentToWorld;
	LateUpdatePrimitives[LateUpdateGameWriteIndex].Reset();
	GatherLateUpdatePrimitives(Component);
	SkipLateUpdate[LateUpdateGameWriteIndex] = bSkipLateUpdate;

	LateUpdateGameWriteIndex = (LateUpdateGameWriteIndex + 1) % 2;
}

bool FLateUpdateManager::GetSkipLateUpdate_RenderThread() const
{
	return SkipLateUpdate[LateUpdateRenderReadIndex];
}

void FLateUpdateManager::Apply_RenderThread(FSceneInterface* Scene, const FTransform& OldRelativeTransform, const FTransform& NewRelativeTransform)
{
	check(IsInRenderingThread());

	if (!LateUpdatePrimitives[LateUpdateRenderReadIndex].Num())
	{
		return;
	}

	if (GetSkipLateUpdate_RenderThread())
	{
		return;
	}

	const FTransform OldCameraTransform = OldRelativeTransform * LateUpdateParentToWorld[LateUpdateRenderReadIndex];
	const FTransform NewCameraTransform = NewRelativeTransform * LateUpdateParentToWorld[LateUpdateRenderReadIndex];
	const FMatrix LateUpdateTransform = (OldCameraTransform.Inverse() * NewCameraTransform).ToMatrixWithScale();

	bool bIndicesHaveChanged = false;

	// Apply delta to the cached scene proxies
	// Also check whether any primitive indices have changed, in case the scene has been modified in the meantime.
	for (auto PrimitivePair : LateUpdatePrimitives[LateUpdateRenderReadIndex])
	{
		FPrimitiveSceneInfo* RetrievedSceneInfo = Scene->GetPrimitiveSceneInfo(PrimitivePair.Value);
		FPrimitiveSceneInfo* CachedSceneInfo = PrimitivePair.Key;

		// If the retrieved scene info is different than our cached scene info then the scene has changed in the meantime
		// and we need to search through the entire scene to make sure it still exists.
		if (CachedSceneInfo != RetrievedSceneInfo)
		{
			bIndicesHaveChanged = true;
			break; // No need to continue here, as we are going to brute force the scene primitives below anyway.
		}
		else if (CachedSceneInfo->Proxy)
		{
			CachedSceneInfo->Proxy->ApplyLateUpdateTransform(LateUpdateTransform);
			PrimitivePair.Value = -1; // Set the cached index to -1 to indicate that this primitive was already processed
		}
	}

	// Indices have changed, so we need to scan the entire scene for primitives that might still exist
	if (bIndicesHaveChanged)
	{
		int32 Index = 0;
		FPrimitiveSceneInfo* RetrievedSceneInfo;
		RetrievedSceneInfo = Scene->GetPrimitiveSceneInfo(Index++);
		while(RetrievedSceneInfo)
		{
			if (RetrievedSceneInfo->Proxy && LateUpdatePrimitives[LateUpdateRenderReadIndex].Contains(RetrievedSceneInfo) && LateUpdatePrimitives[LateUpdateRenderReadIndex][RetrievedSceneInfo] >= 0)
			{
				RetrievedSceneInfo->Proxy->ApplyLateUpdateTransform(LateUpdateTransform);
			}
			RetrievedSceneInfo = Scene->GetPrimitiveSceneInfo(Index++);
		}
	}
}

void FLateUpdateManager::PostRender_RenderThread()
{
	LateUpdatePrimitives[LateUpdateRenderReadIndex].Reset();
	SkipLateUpdate[LateUpdateRenderReadIndex] = false;
	LateUpdateRenderReadIndex = (LateUpdateRenderReadIndex + 1) % 2;
}

void FLateUpdateManager::CacheSceneInfo(USceneComponent* Component)
{
	// If a scene proxy is present, cache it
	UPrimitiveComponent* PrimitiveComponent = dynamic_cast<UPrimitiveComponent*>(Component);
	if (PrimitiveComponent && PrimitiveComponent->SceneProxy)
	{
		FPrimitiveSceneInfo* PrimitiveSceneInfo = PrimitiveComponent->SceneProxy->GetPrimitiveSceneInfo();
		if (PrimitiveSceneInfo)
		{
			LateUpdatePrimitives[LateUpdateGameWriteIndex].Emplace(PrimitiveSceneInfo, PrimitiveSceneInfo->GetIndex());
		}
	}
}

void FLateUpdateManager::GatherLateUpdatePrimitives(USceneComponent* ParentComponent)
{
	CacheSceneInfo(ParentComponent);

	TArray<USceneComponent*> Components;
	ParentComponent->GetChildrenComponents(true, Components);
	for(USceneComponent* Component : Components)
	{
		if (Component != nullptr)
		{
			CacheSceneInfo(Component);
		}
	}
}