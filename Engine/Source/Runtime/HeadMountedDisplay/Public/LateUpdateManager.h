// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

class USceneComponent;
class FSceneInterface;
class FPrimitiveSceneInfo;

/**
* Utility class for applying an offset to a hierarchy of components in the renderer thread.
*/
class HEADMOUNTEDDISPLAY_API FLateUpdateManager
{
public:
	FLateUpdateManager();
	virtual ~FLateUpdateManager() {}

	/** Setup state for applying the render thread late update */
	void Setup(const FTransform& ParentToWorld, USceneComponent* Component, bool bSkipLateUpdate);

	/** Returns true if the LateUpdateSetup data is stale. */
	bool GetSkipLateUpdate_RenderThread() const;

	/** Apply the late update delta to the cached components */
	void Apply_RenderThread(FSceneInterface* Scene, const FTransform& OldRelativeTransform, const FTransform& NewRelativeTransform);

	/** Increments the double buffered read index, etc. - in prep for the next render frame (read: MUST be called for each frame Setup() was called on). */
	void PostRender_RenderThread();

private:

	/** A utility method that calls CacheSceneInfo on ParentComponent and all of its descendants */
	void GatherLateUpdatePrimitives(USceneComponent* ParentComponent);
	/** Generates a LateUpdatePrimitiveInfo for the given component if it has a SceneProxy and appends it to the current LateUpdatePrimitives array */
	void CacheSceneInfo(USceneComponent* Component);

	/** Parent world transform used to reconstruct new world transforms for late update scene proxies */
	FTransform LateUpdateParentToWorld[2];
	/** Primitives that need late update before rendering */
	TMap<FPrimitiveSceneInfo*,int32> LateUpdatePrimitives[2];
	/** Late Update Info Stale, if this is found true do not late update */
	bool SkipLateUpdate[2];

	int32 LateUpdateGameWriteIndex;
	int32 LateUpdateRenderReadIndex;

};

