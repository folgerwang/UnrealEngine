// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Async/TaskGraphInterfaces.h"

/*=============================================================================
	SceneSoftwareOcclusion.h
=============================================================================*/
class FRHICommandListImmediate;
class FScene;
class FViewInfo;
struct FOcclusionFrameResults;

class FSceneSoftwareOcclusion
{
public:
	FSceneSoftwareOcclusion();
	~FSceneSoftwareOcclusion();

	int32 Process(FRHICommandListImmediate& RHICmdList, const FScene* Scene, FViewInfo& View);
	void FlushResults();
	void DebugDraw(FRHICommandListImmediate& RHICmdList, const FViewInfo& View, int32 InX, int32 InY);

private:
	FGraphEventRef TaskRef;
	TUniquePtr<FOcclusionFrameResults> Available;
	TUniquePtr<FOcclusionFrameResults> Processing;
};