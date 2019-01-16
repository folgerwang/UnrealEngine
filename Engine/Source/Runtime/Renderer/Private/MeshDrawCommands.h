// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
MeshDrawCommands.h: Mesh draw commands.
=============================================================================*/

#pragma once

#include "MeshPassProcessor.h"
#include "TranslucencyPass.h"

struct FMeshBatchAndRelevance;
class FStaticMeshBatch;
class FParallelCommandListSet;

/**	
 * Parallel mesh draw command pass setup task context.
 */
class FMeshDrawCommandPassSetupTaskContext
{
public:
	FMeshDrawCommandPassSetupTaskContext()
		: View(nullptr)
		, ShadingPath(EShadingPath::Num)
		, PassType(EMeshPass::Num)
		, bUseGPUScene(false)
		, bDynamicInstancing(false)
		, bReverseCulling(false)
		, bRenderSceneTwoSided(false)
		, BasePassDepthStencilAccess(FExclusiveDepthStencil::DepthNop_StencilNop)
		, MeshPassProcessor(nullptr)
		, MobileBasePassCSMMeshPassProcessor(nullptr)
		, DynamicMeshElements(nullptr)
		, NumDynamicMeshElements(0)
		, NumDynamicMeshCommandBuildRequestElements(0)
		, PrimitiveBounds(nullptr)
		, VisibleMeshDrawCommandsNum(0)
		, NewPassVisibleMeshDrawCommandsNum(0)
		, MaxInstances(1)
	{
	}

	const FViewInfo* View;
	EShadingPath ShadingPath;
	EMeshPass::Type PassType;
	bool bUseGPUScene;
	bool bDynamicInstancing;
	bool bReverseCulling;
	bool bRenderSceneTwoSided;
	FExclusiveDepthStencil::Type BasePassDepthStencilAccess;

	// Mesh pass processor.
	FMeshPassProcessor* MeshPassProcessor;
	FMeshPassProcessor* MobileBasePassCSMMeshPassProcessor;
	const TArray<FMeshBatchAndRelevance, SceneRenderingAllocator>* DynamicMeshElements;

	// Commands.
	int32 NumDynamicMeshElements;
	int32 NumDynamicMeshCommandBuildRequestElements;
	FMeshCommandOneFrameArray MeshDrawCommands;
	FMeshCommandOneFrameArray MobileBasePassCSMMeshDrawCommands;
	TArray<const FStaticMeshBatch*, SceneRenderingAllocator> DynamicMeshCommandBuildRequests;
	TArray<const FStaticMeshBatch*, SceneRenderingAllocator> MobileBasePassCSMDynamicMeshCommandBuildRequests;
	FDynamicMeshDrawCommandStorage MeshDrawCommandStorage;

	// Preallocated resources.
	FGlobalDynamicVertexBuffer::FAllocation PrimitiveIdBuffer;
	FMeshCommandOneFrameArray TempVisibleMeshDrawCommands;

	// For UpdateTranslucentMeshSortKeys.
	ETranslucencyPass::Type TranslucencyPass;
	ETranslucentSortPolicy::Type TranslucentSortPolicy;
	FVector TranslucentSortAxis;
	FVector ViewOrigin;
	FMatrix ViewMatrix;
	const TArray<struct FPrimitiveBounds>* PrimitiveBounds;

	// For logging instancing stats.
	int32 VisibleMeshDrawCommandsNum;
	int32 NewPassVisibleMeshDrawCommandsNum;
	int32 MaxInstances;
};

/**
 * Parallel mesh draw command processing and rendering. 
 * Encapsulates two parallel tasks - mesh command setup task and drawing task.
 */
class FParallelMeshDrawCommandPass
{
public:
	FParallelMeshDrawCommandPass()
		: MaxNumDraws(0)
	{
	}

	~FParallelMeshDrawCommandPass();

	/**
	 * Dispatch visible mesh draw command process task, which prepares this pass for drawing.
	 * This includes generation of dynamic mesh draw commands, draw sorting and draw merging.
	 */
	void DispatchPassSetup(
		FScene* Scene,
		const FViewInfo& View, 
		EMeshPass::Type PassType, 
		FExclusiveDepthStencil::Type BasePassDepthStencilAccess,
		FMeshPassProcessor* MeshPassProcessor,
		const TArray<FMeshBatchAndRelevance, SceneRenderingAllocator>& DynamicMeshElements,
		int32 NumDynamicMeshElements,
		TArray<const FStaticMeshBatch*, SceneRenderingAllocator>& InOutDynamicMeshCommandBuildRequests,
		int32 NumDynamicMeshCommandBuildRequestElements,
		FMeshCommandOneFrameArray& InOutMeshDrawCommands,
		FMeshPassProcessor* MobileBasePassCSMMeshPassProcessor = nullptr, // Required only for the mobile base pass.
		FMeshCommandOneFrameArray* InOutMobileBasePassCSMMeshDrawCommands = nullptr // Required only for the mobile base pass.
	);

	/**
	 * Dispatch visible mesh draw command draw task.
	 */
	void DispatchDraw(FParallelCommandListSet* ParallelCommandListSet, FRHICommandList& RHICmdList) const;

	void Empty();
	void SetDumpInstancingStats(const FString& InPassName);
	bool HasAnyDraw() const { return MaxNumDraws > 0; }


private:
	FMeshDrawCommandPassSetupTaskContext TaskContext;
	FGraphEventArray TaskEventRefs;
	FString PassNameForStats;

	// Maximum number of draws for this pass. Used to prealocate resources on rendering thread. 
	// Has a guarantee that if there won't be any draws, then MaxNumDraws = 0;
	int32 MaxNumDraws;

	void DumpInstancingStats() const;
	void WaitForMeshPassSetupTask() const;
};
