// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PrimitiveSceneInfo.cpp: Primitive scene info implementation.
=============================================================================*/

#include "PrimitiveSceneInfo.h"
#include "PrimitiveSceneProxy.h"
#include "Components/PrimitiveComponent.h"
#include "SceneManagement.h"
#include "SceneCore.h"
#include "VelocityRendering.h"
#include "ScenePrivate.h"
#include "RayTracing/RayTracingMaterialHitShaders.h"

/** An implementation of FStaticPrimitiveDrawInterface that stores the drawn elements for the rendering thread to use. */
class FBatchingSPDI : public FStaticPrimitiveDrawInterface
{
public:

	// Constructor.
	FBatchingSPDI(FPrimitiveSceneInfo* InPrimitiveSceneInfo):
		PrimitiveSceneInfo(InPrimitiveSceneInfo)
	{}

	// FStaticPrimitiveDrawInterface.
	virtual void SetHitProxy(HHitProxy* HitProxy) final override
	{
		CurrentHitProxy = HitProxy;

		if(HitProxy)
		{
			// Only use static scene primitive hit proxies in the editor.
			if(GIsEditor)
			{
				// Keep a reference to the hit proxy from the FPrimitiveSceneInfo, to ensure it isn't deleted while the static mesh still
				// uses its id.
				PrimitiveSceneInfo->HitProxies.Add(HitProxy);
			}
		}
	}

	virtual void ReserveMemoryForMeshes(int32 MeshNum)
	{
		PrimitiveSceneInfo->StaticMeshRelevances.Reserve(PrimitiveSceneInfo->StaticMeshRelevances.Max() + MeshNum);
		PrimitiveSceneInfo->StaticMeshes.Reserve(PrimitiveSceneInfo->StaticMeshes.Max() + MeshNum);
	}

	virtual void DrawMesh(const FMeshBatch& Mesh, float ScreenSize) final override
	{
		if (Mesh.GetNumPrimitives() > 0)
		{
			check(Mesh.VertexFactory);
			check(Mesh.VertexFactory->IsInitialized());
			checkSlow(IsInRenderingThread());

			FPrimitiveSceneProxy* PrimitiveSceneProxy = PrimitiveSceneInfo->Proxy;
			PrimitiveSceneProxy->VerifyUsedMaterial(Mesh.MaterialRenderProxy);

			FStaticMeshBatch* StaticMesh = new(PrimitiveSceneInfo->StaticMeshes) FStaticMeshBatch(
				PrimitiveSceneInfo,
				Mesh,
				CurrentHitProxy ? CurrentHitProxy->Id : FHitProxyId()
				);

			const ERHIFeatureLevel::Type FeatureLevel = PrimitiveSceneInfo->Scene->GetFeatureLevel();
			StaticMesh->PreparePrimitiveUniformBuffer(PrimitiveSceneProxy, FeatureLevel);

			const bool bSupportsCachingMeshDrawCommands = SupportsCachingMeshDrawCommands(StaticMesh->VertexFactory, PrimitiveSceneProxy, Mesh.MaterialRenderProxy, FeatureLevel);

			FStaticMeshBatchRelevance* StaticMeshRelevance = new(PrimitiveSceneInfo->StaticMeshRelevances) FStaticMeshBatchRelevance(
				*StaticMesh, 
				ScreenSize, 
				bSupportsCachingMeshDrawCommands
			);
		}
	}

private:
	FPrimitiveSceneInfo* PrimitiveSceneInfo;
	TRefCountPtr<HHitProxy> CurrentHitProxy;
};

FPrimitiveFlagsCompact::FPrimitiveFlagsCompact(const FPrimitiveSceneProxy* Proxy)
	: bCastDynamicShadow(Proxy->CastsDynamicShadow())
	, bStaticLighting(Proxy->HasStaticLighting())
	, bCastStaticShadow(Proxy->CastsStaticShadow())
{}

FPrimitiveSceneInfoCompact::FPrimitiveSceneInfoCompact(FPrimitiveSceneInfo* InPrimitiveSceneInfo) :
	PrimitiveFlagsCompact(InPrimitiveSceneInfo->Proxy)
{
	PrimitiveSceneInfo = InPrimitiveSceneInfo;
	Proxy = PrimitiveSceneInfo->Proxy;
	Bounds = PrimitiveSceneInfo->Proxy->GetBounds();
	MinDrawDistance = PrimitiveSceneInfo->Proxy->GetMinDrawDistance();
	MaxDrawDistance = PrimitiveSceneInfo->Proxy->GetMaxDrawDistance();

	VisibilityId = PrimitiveSceneInfo->Proxy->GetVisibilityId();
}

FPrimitiveSceneInfo::FPrimitiveSceneInfo(UPrimitiveComponent* InComponent,FScene* InScene):
	Proxy(InComponent->SceneProxy),
	PrimitiveComponentId(InComponent->ComponentId),
	ComponentLastRenderTime(&InComponent->LastRenderTime),
	ComponentLastRenderTimeOnScreen(&InComponent->LastRenderTimeOnScreen),
	IndirectLightingCacheAllocation(NULL),
	CachedPlanarReflectionProxy(NULL),
	CachedReflectionCaptureProxy(NULL),
	bNeedsCachedReflectionCaptureUpdate(true),
	DefaultDynamicHitProxy(NULL),
	LightList(NULL),
	LastRenderTime(-FLT_MAX),
	Scene(InScene),
	NumMobileMovablePointLights(0),
	bIsUsingCustomLODRules(Proxy->IsUsingCustomLODRules()),
	bIsUsingCustomWholeSceneShadowLODRules(Proxy->IsUsingCustomWholeSceneShadowLODRules()),
#if RHI_RAYTRACING
	bDrawInGame(Proxy->IsDrawnInGame()),
	bShouldRenderInMainPass(InComponent->SceneProxy->ShouldRenderInMainPass()),
	bIsVisibleInReflectionCaptures(InComponent->SceneProxy->IsVisibleInReflectionCaptures()),
	bIsRayTracingRelevant(InComponent->SceneProxy->IsRayTracingRelevant()),
	bIsRayTracingStaticRelevant(InComponent->SceneProxy->IsRayTracingStaticRelevant()),
	bIsVisibleInRayTracing(InComponent->SceneProxy->IsVisibleInRayTracing()),
#endif
	PackedIndex(INDEX_NONE),
	ComponentForDebuggingOnly(InComponent),
	bNeedsStaticMeshUpdate(false),
	bNeedsStaticMeshUpdateWithoutVisibilityCheck(false),
	bNeedsUniformBufferUpdate(false),
	bIndirectLightingCacheBufferDirty(false),
	LightmapDataOffset(INDEX_NONE),
	NumLightmapDataEntries(0)
{
	check(ComponentForDebuggingOnly);
	check(PrimitiveComponentId.IsValid());
	check(Proxy);

	UPrimitiveComponent* SearchParentComponent = Cast<UPrimitiveComponent>(InComponent->GetAttachmentRoot());

	if (SearchParentComponent && SearchParentComponent != InComponent)
	{
		LightingAttachmentRoot = SearchParentComponent->ComponentId;
	}

	// Only create hit proxies in the Editor as that's where they are used.
	if (GIsEditor)
	{
		// Create a dynamic hit proxy for the primitive. 
		DefaultDynamicHitProxy = Proxy->CreateHitProxies(InComponent,HitProxies);
		if( DefaultDynamicHitProxy )
		{
			DefaultDynamicHitProxyId = DefaultDynamicHitProxy->Id;
		}
	}

	// set LOD parent info if exists
	UPrimitiveComponent* LODParent = InComponent->GetLODParentPrimitive();
	if (LODParent)
	{
		LODParentComponentId = LODParent->ComponentId;
	}

	FMemory::Memzero(CachedReflectionCaptureProxies);

#if RHI_RAYTRACING
	RayTracingGeometries = InComponent->SceneProxy->MoveRayTracingGeometries();
#endif
}

FPrimitiveSceneInfo::~FPrimitiveSceneInfo()
{
	check(!OctreeId.IsValidId());
	check(StaticMeshCommandInfos.Num() == 0);
}

#if RHI_RAYTRACING
FRayTracingGeometryRHIRef FPrimitiveSceneInfo::GetStaticRayTracingGeometryInstance(int LodLevel)
{
	if (RayTracingGeometries.Num() > LodLevel)
	{
		return RayTracingGeometries[LodLevel];
	}
	else
	{
		return nullptr;
	}
}
#endif

void FPrimitiveSceneInfo::CacheMeshDrawCommands(FRHICommandListImmediate& RHICmdList)
{
	check(StaticMeshCommandInfos.Num() == 0);

	int32 MeshWithCachedCommandsNum = 0;
	for (int32 MeshIndex = 0; MeshIndex < StaticMeshes.Num(); MeshIndex++)
	{
		const FStaticMeshBatch& Mesh = StaticMeshes[MeshIndex];
		if (SupportsCachingMeshDrawCommands(Mesh.VertexFactory, Proxy))
		{
			++MeshWithCachedCommandsNum;
		}
	}

#if RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		int MaxLOD = -1;

		for (int32 MeshIndex = 0; MeshIndex < StaticMeshes.Num(); MeshIndex++)
		{
			FStaticMeshBatch& Mesh = StaticMeshes[MeshIndex];
			MaxLOD = MaxLOD < Mesh.LODIndex ? Mesh.LODIndex : MaxLOD;
		}

		if (StaticMeshes.Num() > 0)
		{
			CachedRayTracingMeshCommandIndicesPerLOD.Empty(MaxLOD + 1);
			CachedRayTracingMeshCommandIndicesPerLOD.AddDefaulted(MaxLOD + 1);
		}
	}
#endif

	if (MeshWithCachedCommandsNum > 0)
	{
		//@todo - only need material uniform buffers to be created since we are going to cache pointers to them
		// Any updates (after initial creation) don't need to be forced here
		FMaterialRenderProxy::UpdateDeferredCachedUniformExpressions();

		// Reserve based on assumption that we have on average 2 cached mesh draw commands per mesh.
		StaticMeshCommandInfos.Reserve(MeshWithCachedCommandsNum * 2);

		QUICK_SCOPE_CYCLE_COUNTER(STAT_CacheMeshDrawCommands);
		FMemMark Mark(FMemStack::Get());

		const EShadingPath ShadingPath = Scene->GetShadingPath();

		for (int32 MeshIndex = 0; MeshIndex < StaticMeshes.Num(); MeshIndex++)
		{
			FStaticMeshBatchRelevance& MeshRelevance = StaticMeshRelevances[MeshIndex];
			FStaticMeshBatch& Mesh = StaticMeshes[MeshIndex];

			check(MeshRelevance.CommandInfosMask.IsEmpty());
			MeshRelevance.CommandInfosBase = StaticMeshCommandInfos.Num();

			if (SupportsCachingMeshDrawCommands(Mesh.VertexFactory, Proxy))
			{
				for (int32 PassIndex = 0; PassIndex < EMeshPass::Num; PassIndex++)
				{
					EMeshPass::Type PassType = (EMeshPass::Type)PassIndex;

					if ((FPassProcessorManager::GetPassFlags(ShadingPath, PassType) & EMeshPassFlags::CachedMeshCommands) != EMeshPassFlags::None)
					{
						FCachedMeshDrawCommandInfo CommandInfo;
						CommandInfo.MeshPass = PassType;

						FCachedPassMeshDrawList& SceneDrawList = Scene->CachedDrawLists[PassType];
						FCachedPassMeshDrawListContext CachedPassMeshDrawListContext(CommandInfo, SceneDrawList, *Scene);

						PassProcessorCreateFunction CreateFunction = FPassProcessorManager::GetCreateFunction(ShadingPath, PassType);
						FMeshPassProcessor* PassMeshProcessor = CreateFunction(Scene, nullptr, &CachedPassMeshDrawListContext);

						if (PassMeshProcessor != nullptr)
						{
							check(!Mesh.bRequiresPerElementVisibility);
							uint64 BatchElementMask = ~0ull;
							PassMeshProcessor->AddMeshBatch(Mesh, BatchElementMask, Proxy);

							PassMeshProcessor->~FMeshPassProcessor();
						}

						if (CommandInfo.CommandIndex != -1 || CommandInfo.StateBucketId != -1)
						{
							static_assert(sizeof(MeshRelevance.CommandInfosMask) * 8 >= EMeshPass::Num, "CommandInfosMask is too small to contain all mesh passes.");

							MeshRelevance.CommandInfosMask.Set(PassType);

							StaticMeshCommandInfos.Add(CommandInfo);

#if DO_GUARD_SLOW
							if (ShadingPath == EShadingPath::Deferred)
							{
								FMeshDrawCommand* MeshDrawCommand = CommandInfo.StateBucketId >= 0
									? &Scene->CachedMeshDrawCommandStateBuckets[FSetElementId::FromInteger(CommandInfo.StateBucketId)].MeshDrawCommand
									: &SceneDrawList.MeshDrawCommands[CommandInfo.CommandIndex];

								ensureMsgf(MeshDrawCommand->VertexStreams.GetAllocatedSize() == 0, TEXT("Cached Mesh Draw command overflows VertexStreams.  VertexStream inline size should be tweaked."));
								
								if (PassType == EMeshPass::BasePass || PassType == EMeshPass::DepthPass || PassType == EMeshPass::CSMShadowDepth)
								{
									TArray<EShaderFrequency, TInlineAllocator<SF_NumFrequencies>> ShaderFrequencies;
									MeshDrawCommand->ShaderBindings.GetShaderFrequencies(ShaderFrequencies);

									for (int32 i = 0; i < ShaderFrequencies.Num(); i++)
									{
										FMeshDrawSingleShaderBindings SingleShaderBindings = MeshDrawCommand->ShaderBindings.GetSingleShaderBindings(ShaderFrequencies[i]);
										ensureMsgf(SingleShaderBindings.ParameterMapInfo.LooseParameterBuffers.Num() == 0, TEXT("Cached Mesh Draw command uses loose parameters.  This will break dynamic instancing in performance critical pass.  Use Uniform Buffers instead."));
										ensureMsgf(SingleShaderBindings.ParameterMapInfo.SRVs.Num() == 0, TEXT("Cached Mesh Draw command uses individual SRVs.  This will break dynamic instancing in performance critical pass.  Use Uniform Buffers instead."));
										ensureMsgf(SingleShaderBindings.ParameterMapInfo.TextureSamplers.Num() == 0, TEXT("Cached Mesh Draw command uses individual Texture Samplers.  This will break dynamic instancing in performance critical pass.  Use Uniform Buffers instead."));
									}
								}
							}
#endif
						}
					}
				}

			#if RHI_RAYTRACING
				if (IsRayTracingEnabled())
				{
					FCachedRayTracingMeshCommandContext CommandContext(Scene->CachedRayTracingMeshCommands);
					FRayTracingMeshProcessor RayTracingMeshProcessor(&CommandContext, Scene, nullptr);

					check(!Mesh.bRequiresPerElementVisibility);
					RayTracingMeshProcessor.AddMeshBatch(Mesh, ~0ull, Proxy);
					
					CachedRayTracingMeshCommandIndicesPerLOD[Mesh.LODIndex].Add(CommandContext.CommandIndex);
				}
			#endif
			}
		}
	}
}

void FPrimitiveSceneInfo::RemoveCachedMeshDrawCommands()
{
	checkSlow(IsInRenderingThread());

	for (int32 CommandIndex = 0; CommandIndex < StaticMeshCommandInfos.Num(); ++CommandIndex)
	{
		const FCachedMeshDrawCommandInfo& CachedCommand = StaticMeshCommandInfos[CommandIndex];
		const FSetElementId StateBucketId = FSetElementId::FromInteger(CachedCommand.StateBucketId);

		if (StateBucketId.IsValidId())
		{
			FMeshDrawCommandStateBucket& StateBucket = Scene->CachedMeshDrawCommandStateBuckets[StateBucketId];

			FGraphicsMinimalPipelineStateId::RemovePersistentId(StateBucket.MeshDrawCommand.CachedPipelineId);

			if (StateBucket.Num == 1)
			{
				Scene->CachedMeshDrawCommandStateBuckets.Remove(StateBucketId);
			}
			else
			{
				StateBucket.Num--;
			}
		}
		else if (CachedCommand.CommandIndex >= 0)
		{
			FCachedPassMeshDrawList& PassDrawList = Scene->CachedDrawLists[CachedCommand.MeshPass];

			FGraphicsMinimalPipelineStateId::RemovePersistentId(PassDrawList.MeshDrawCommands[CachedCommand.CommandIndex].CachedPipelineId);
			PassDrawList.MeshDrawCommands.RemoveAt(CachedCommand.CommandIndex);

			// Track the lowest index that might be free for faster AddAtLowestFreeIndex
			PassDrawList.LowestFreeIndexSearchStart = FMath::Min(PassDrawList.LowestFreeIndexSearchStart, CachedCommand.CommandIndex);
		}

	}

	for (int32 MeshIndex = 0; MeshIndex < StaticMeshRelevances.Num(); ++MeshIndex)
	{
		FStaticMeshBatchRelevance& MeshRelevance = StaticMeshRelevances[MeshIndex];

		MeshRelevance.CommandInfosBase = 0;
		MeshRelevance.CommandInfosMask.Reset();
	}

	StaticMeshCommandInfos.Empty();

#if RHI_RAYTRACING
	if (IsRayTracingEnabled())
	{
		for (auto& CachedRayTracingMeshCommandIndices : CachedRayTracingMeshCommandIndicesPerLOD)
		{
			for (auto CommandIndex : CachedRayTracingMeshCommandIndices)
			{
				if (CommandIndex >= 0)
				{
					Scene->CachedRayTracingMeshCommands.RayTracingMeshCommands.RemoveAt(CommandIndex);
				}
			}
		}

		CachedRayTracingMeshCommandIndicesPerLOD.Empty();
	}
#endif
}

void FPrimitiveSceneInfo::AddStaticMeshes(FRHICommandListImmediate& RHICmdList, bool bAddToStaticDrawLists)
{
	// Cache the primitive's static mesh elements.
	FBatchingSPDI BatchingSPDI(this);
	BatchingSPDI.SetHitProxy(DefaultDynamicHitProxy);
	Proxy->DrawStaticElements(&BatchingSPDI);
	StaticMeshes.Shrink();
	StaticMeshRelevances.Shrink();

	check(StaticMeshRelevances.Num() == StaticMeshes.Num());

	for(int32 MeshIndex = 0;MeshIndex < StaticMeshes.Num();MeshIndex++)
	{
		FStaticMeshBatchRelevance& MeshRelevance = StaticMeshRelevances[MeshIndex];
		FStaticMeshBatch& Mesh = StaticMeshes[MeshIndex];

		// Add the static mesh to the scene's static mesh list.
		FSparseArrayAllocationInfo SceneArrayAllocation = Scene->StaticMeshes.AddUninitialized();
		Scene->StaticMeshes[SceneArrayAllocation.Index] = &Mesh;
		Mesh.Id = SceneArrayAllocation.Index;
		MeshRelevance.Id = SceneArrayAllocation.Index;

		if (Mesh.bRequiresPerElementVisibility)
		{
			// Use a separate index into StaticMeshBatchVisibility, since most meshes don't use it
			Mesh.BatchVisibilityId = Scene->StaticMeshBatchVisibility.AddUninitialized().Index;
			Scene->StaticMeshBatchVisibility[Mesh.BatchVisibilityId] = true;
		}
	}

	if (bAddToStaticDrawLists)
	{
		CacheMeshDrawCommands(RHICmdList);
	}
}

void FPrimitiveSceneInfo::AddToScene(FRHICommandListImmediate& RHICmdList, bool bUpdateStaticDrawLists, bool bAddToStaticDrawLists)
{
	check(IsInRenderingThread());

	// Create an indirect lighting cache uniform buffer if we attaching a primitive that may require it, as it may be stored inside a cached mesh command.
	if (IsIndirectLightingCacheAllowed(Scene->GetFeatureLevel())
		&& Proxy->WillEverBeLit()
		&& ((Proxy->HasStaticLighting() && Proxy->NeedsUnbuiltPreviewLighting()) || (Proxy->IsMovable() && Proxy->GetIndirectLightingCacheQuality() != ILCQ_Off)))
	{
		if (!IndirectLightingCacheUniformBuffer)
		{
			FIndirectLightingCacheUniformParameters Parameters;

			GetIndirectLightingCacheParameters(
				Scene->GetFeatureLevel(), 
				Parameters, 
				nullptr,
				nullptr,
				FVector(0.0f, 0.0f, 0.0f),
				0, 
				nullptr);

			IndirectLightingCacheUniformBuffer = TUniformBufferRef<FIndirectLightingCacheUniformParameters>::CreateUniformBufferImmediate(Parameters, UniformBuffer_MultiFrame, EUniformBufferValidation::None);
		}
	}
	
	// If we are attaching a primitive that should be statically lit but has unbuilt lighting,
	// Allocate space in the indirect lighting cache so that it can be used for previewing indirect lighting
	if (Proxy->HasStaticLighting() 
		&& Proxy->NeedsUnbuiltPreviewLighting() 
		&& IsIndirectLightingCacheAllowed(Scene->GetFeatureLevel()))
	{
		FIndirectLightingCacheAllocation* PrimitiveAllocation = Scene->IndirectLightingCache.FindPrimitiveAllocation(PrimitiveComponentId);

		if (PrimitiveAllocation)
		{
			IndirectLightingCacheAllocation = PrimitiveAllocation;
			PrimitiveAllocation->SetDirty();
		}
		else
		{
			PrimitiveAllocation = Scene->IndirectLightingCache.AllocatePrimitive(this, true);
			PrimitiveAllocation->SetDirty();
			IndirectLightingCacheAllocation = PrimitiveAllocation;
		}
	}
	MarkIndirectLightingCacheBufferDirty();

	FPrimitiveSceneProxy::FLCIArray LCIs;
	Proxy->GetLCIs(LCIs);
	for (int32 i = 0; i < LCIs.Num(); ++i)
	{
		FLightCacheInterface* LCI = LCIs[i];

		if (LCI) 
		{
			LCI->CreatePrecomputedLightingUniformBuffer_RenderingThread(Scene->GetFeatureLevel());
		}
	}

	NumLightmapDataEntries = LCIs.Num();

	if (NumLightmapDataEntries > 0 && UseGPUScene(GMaxRHIShaderPlatform, Scene->GetFeatureLevel()))
	{
		LightmapDataOffset = Scene->GPUScene.LightmapDataAllocator.Allocate(NumLightmapDataEntries);
	}

	// Cache the nearest reflection proxy if needed
	if (NeedsReflectionCaptureUpdate())
	{
		CacheReflectionCaptures();
	}

	if (bUpdateStaticDrawLists)
	{
		AddStaticMeshes(RHICmdList, bAddToStaticDrawLists);
	}

	// create potential storage for our compact info
	FPrimitiveSceneInfoCompact CompactPrimitiveSceneInfo(this);

	// Add the primitive to the octree.
	check(!OctreeId.IsValidId());
	Scene->PrimitiveOctree.AddElement(CompactPrimitiveSceneInfo);
	check(OctreeId.IsValidId());

	if (Proxy->CastsDynamicIndirectShadow())
	{
		Scene->DynamicIndirectCasterPrimitives.Add(this);
	}

	Scene->PrimitiveSceneProxies[PackedIndex] = Proxy;
	Scene->PrimitiveTransforms[PackedIndex] = Proxy->GetLocalToWorld();

	// Set bounds.
	FPrimitiveBounds& PrimitiveBounds = Scene->PrimitiveBounds[PackedIndex];
	FBoxSphereBounds BoxSphereBounds = Proxy->GetBounds();
	PrimitiveBounds.BoxSphereBounds = BoxSphereBounds;
	PrimitiveBounds.MinDrawDistanceSq = FMath::Square(Proxy->GetMinDrawDistance());
	PrimitiveBounds.MaxDrawDistance = Proxy->GetMaxDrawDistance();
	PrimitiveBounds.MaxCullDistance = PrimitiveBounds.MaxDrawDistance;

	Scene->PrimitiveFlagsCompact[PackedIndex] = FPrimitiveFlagsCompact(Proxy);

	// Store precomputed visibility ID.
	int32 VisibilityBitIndex = Proxy->GetVisibilityId();
	FPrimitiveVisibilityId& VisibilityId = Scene->PrimitiveVisibilityIds[PackedIndex];
	VisibilityId.ByteIndex = VisibilityBitIndex / 8;
	VisibilityId.BitMask = (1 << (VisibilityBitIndex & 0x7));

	// Store occlusion flags.
	uint8 OcclusionFlags = EOcclusionFlags::None;
	if (Proxy->CanBeOccluded())
	{
		OcclusionFlags |= EOcclusionFlags::CanBeOccluded;
	}
	if (Proxy->HasSubprimitiveOcclusionQueries())
	{
		OcclusionFlags |= EOcclusionFlags::HasSubprimitiveQueries;
	}
	if (Proxy->AllowApproximateOcclusion()
		// Allow approximate occlusion if attached, even if the parent does not have bLightAttachmentsAsGroup enabled
		|| LightingAttachmentRoot.IsValid())
	{
		OcclusionFlags |= EOcclusionFlags::AllowApproximateOcclusion;
	}
	if (VisibilityBitIndex >= 0)
	{
		OcclusionFlags |= EOcclusionFlags::HasPrecomputedVisibility;
	}
	Scene->PrimitiveOcclusionFlags[PackedIndex] = OcclusionFlags;

	// Store occlusion bounds.
	FBoxSphereBounds OcclusionBounds = BoxSphereBounds;
	if (Proxy->HasCustomOcclusionBounds())
	{
		OcclusionBounds = Proxy->GetCustomOcclusionBounds();
	}
	OcclusionBounds.BoxExtent.X = OcclusionBounds.BoxExtent.X + OCCLUSION_SLOP;
	OcclusionBounds.BoxExtent.Y = OcclusionBounds.BoxExtent.Y + OCCLUSION_SLOP;
	OcclusionBounds.BoxExtent.Z = OcclusionBounds.BoxExtent.Z + OCCLUSION_SLOP;
	OcclusionBounds.SphereRadius = OcclusionBounds.SphereRadius + OCCLUSION_SLOP;
	Scene->PrimitiveOcclusionBounds[PackedIndex] = OcclusionBounds;

	// Store the component.
	Scene->PrimitiveComponentIds[PackedIndex] = PrimitiveComponentId;

	{
		FMemMark MemStackMark(FMemStack::Get());

		// Find lights that affect the primitive in the light octree.
		for (FSceneLightOctree::TConstElementBoxIterator<SceneRenderingAllocator> LightIt(Scene->LightOctree, Proxy->GetBounds().GetBox());
			LightIt.HasPendingElements();
			LightIt.Advance())
		{
			const FLightSceneInfoCompact& LightSceneInfoCompact = LightIt.GetCurrentElement();
			if (LightSceneInfoCompact.AffectsPrimitive(CompactPrimitiveSceneInfo.Bounds, CompactPrimitiveSceneInfo.Proxy))
			{
				FLightPrimitiveInteraction::Create(LightSceneInfoCompact.LightSceneInfo,this);
			}
		}
	}

	INC_MEMORY_STAT_BY(STAT_PrimitiveInfoMemory, sizeof(*this) + StaticMeshes.GetAllocatedSize() + StaticMeshRelevances.GetAllocatedSize() + Proxy->GetMemoryFootprint());
}

void FPrimitiveSceneInfo::RemoveStaticMeshes()
{
	// Remove static meshes from the scene.
	StaticMeshes.Empty();
	StaticMeshRelevances.Empty();
	RemoveCachedMeshDrawCommands();
}

void FPrimitiveSceneInfo::RemoveFromScene(bool bUpdateStaticDrawLists)
{
	check(IsInRenderingThread());

	// implicit linked list. The destruction will update this "head" pointer to the next item in the list.
	while(LightList)
	{
		FLightPrimitiveInteraction::Destroy(LightList);
	}
	
	// Remove the primitive from the octree.
	check(OctreeId.IsValidId());
	check(Scene->PrimitiveOctree.GetElementById(OctreeId).PrimitiveSceneInfo == this);
	Scene->PrimitiveOctree.RemoveElement(OctreeId);
	OctreeId = FOctreeElementId();

	if (LightmapDataOffset != INDEX_NONE && UseGPUScene(GMaxRHIShaderPlatform, Scene->GetFeatureLevel()))
	{
		Scene->GPUScene.LightmapDataAllocator.Free(LightmapDataOffset, NumLightmapDataEntries);
	}
	
	if (Proxy->CastsDynamicIndirectShadow())
	{
		Scene->DynamicIndirectCasterPrimitives.RemoveSingleSwap(this);
	}

	IndirectLightingCacheAllocation = NULL;
	ClearIndirectLightingCacheBuffer(false);

	DEC_MEMORY_STAT_BY(STAT_PrimitiveInfoMemory, sizeof(*this) + StaticMeshes.GetAllocatedSize() + StaticMeshRelevances.GetAllocatedSize() + Proxy->GetMemoryFootprint());

	if (bUpdateStaticDrawLists)
	{
		if (bNeedsStaticMeshUpdate)
		{
			Scene->PrimitivesNeedingStaticMeshUpdate.Remove(this);

			bNeedsStaticMeshUpdate = false;
		}

		if (bNeedsStaticMeshUpdateWithoutVisibilityCheck)
		{
			Scene->PrimitivesNeedingStaticMeshUpdateWithoutVisibilityCheck.Remove(this);

			bNeedsStaticMeshUpdateWithoutVisibilityCheck = false;
		}

		// IndirectLightingCacheUniformBuffer may be cached inside cached mesh draw commands, so we 
		// can't delete it unless we also update cached mesh command.
		IndirectLightingCacheUniformBuffer.SafeRelease();

		RemoveStaticMeshes();
	}
}

void FPrimitiveSceneInfo::UpdateStaticMeshes(FRHICommandListImmediate& RHICmdList, bool bReAddToDrawLists)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_FPrimitiveSceneInfo_UpdateStaticMeshes);
	const bool bOriginalNeedsStaticMeshUpdate = bNeedsStaticMeshUpdate;
	bNeedsStaticMeshUpdate = !bReAddToDrawLists;

	if (bOriginalNeedsStaticMeshUpdate != bNeedsStaticMeshUpdate)
	{
		if (bNeedsStaticMeshUpdate)
		{
			Scene->PrimitivesNeedingStaticMeshUpdate.Add(this);
		}
		else
		{
			Scene->PrimitivesNeedingStaticMeshUpdate.Remove(this);
		}
	}

	if (!bNeedsStaticMeshUpdate && bNeedsStaticMeshUpdateWithoutVisibilityCheck)
	{
		Scene->PrimitivesNeedingStaticMeshUpdateWithoutVisibilityCheck.Remove(this);

		bNeedsStaticMeshUpdateWithoutVisibilityCheck = false;
	}

	RemoveCachedMeshDrawCommands();
	if (bReAddToDrawLists)
	{
		CacheMeshDrawCommands(RHICmdList);
	}
}

void FPrimitiveSceneInfo::UpdateUniformBuffer(FRHICommandListImmediate& RHICmdList)
{
	checkSlow(bNeedsUniformBufferUpdate);
	bNeedsUniformBufferUpdate = false;
	Proxy->UpdateUniformBuffer();
}

void FPrimitiveSceneInfo::BeginDeferredUpdateStaticMeshes()
{
	if (!bNeedsStaticMeshUpdate)
	{
		// Set a flag which causes InitViews to update the static meshes the next time the primitive is visible.
		bNeedsStaticMeshUpdate = true;

		Scene->PrimitivesNeedingStaticMeshUpdate.Add(this);
	}
}

void FPrimitiveSceneInfo::BeginDeferredUpdateStaticMeshesWithoutVisibilityCheck()
{
	if (bNeedsStaticMeshUpdate && !bNeedsStaticMeshUpdateWithoutVisibilityCheck)
	{
		bNeedsStaticMeshUpdateWithoutVisibilityCheck = true;

		Scene->PrimitivesNeedingStaticMeshUpdateWithoutVisibilityCheck.Add(this);
	}
}

void FPrimitiveSceneInfo::LinkLODParentComponent()
{
	if (LODParentComponentId.IsValid())
	{
		Scene->SceneLODHierarchy.AddChildNode(LODParentComponentId, this);
	}
}

void FPrimitiveSceneInfo::UnlinkLODParentComponent()
{
	if(LODParentComponentId.IsValid())
	{
		Scene->SceneLODHierarchy.RemoveChildNode(LODParentComponentId, this);
		// I don't think this will be reused but just in case
		LODParentComponentId = FPrimitiveComponentId();
	}
}

void FPrimitiveSceneInfo::LinkAttachmentGroup()
{
	// Add the primitive to its attachment group.
	if (LightingAttachmentRoot.IsValid())
	{
		FAttachmentGroupSceneInfo* AttachmentGroup = Scene->AttachmentGroups.Find(LightingAttachmentRoot);

		if (!AttachmentGroup)
		{
			// If this is the first primitive attached that uses this attachment parent, create a new attachment group.
			AttachmentGroup = &Scene->AttachmentGroups.Add(LightingAttachmentRoot, FAttachmentGroupSceneInfo());
		}

		AttachmentGroup->Primitives.Add(this);
	}
	else if (Proxy->LightAttachmentsAsGroup())
	{
		FAttachmentGroupSceneInfo* AttachmentGroup = Scene->AttachmentGroups.Find(PrimitiveComponentId);

		if (!AttachmentGroup)
		{
			// Create an empty attachment group 
			AttachmentGroup = &Scene->AttachmentGroups.Add(PrimitiveComponentId, FAttachmentGroupSceneInfo());
		}

		AttachmentGroup->ParentSceneInfo = this;
	}
}

void FPrimitiveSceneInfo::UnlinkAttachmentGroup()
{
	// Remove the primitive from its attachment group.
	if (LightingAttachmentRoot.IsValid())
	{
		FAttachmentGroupSceneInfo& AttachmentGroup = Scene->AttachmentGroups.FindChecked(LightingAttachmentRoot);
		AttachmentGroup.Primitives.RemoveSwap(this);

		if (AttachmentGroup.Primitives.Num() == 0)
		{
			// If this was the last primitive attached that uses this attachment root, free the group.
			Scene->AttachmentGroups.Remove(LightingAttachmentRoot);
		}
	}
	else if (Proxy->LightAttachmentsAsGroup())
	{
		FAttachmentGroupSceneInfo* AttachmentGroup = Scene->AttachmentGroups.Find(PrimitiveComponentId);
		
		if (AttachmentGroup)
		{
			AttachmentGroup->ParentSceneInfo = NULL;
		}
	}
}

void FPrimitiveSceneInfo::GatherLightingAttachmentGroupPrimitives(TArray<FPrimitiveSceneInfo*, SceneRenderingAllocator>& OutChildSceneInfos)
{
#if ENABLE_NAN_DIAGNOSTIC
	// local function that returns full name of object
	auto GetObjectName = [](const UPrimitiveComponent* InPrimitive)->FString
	{
		return (InPrimitive) ? InPrimitive->GetFullName() : FString(TEXT("Unknown Object"));
	};

	// verify that the current object has a valid bbox before adding it
	const float& BoundsRadius = this->Proxy->GetBounds().SphereRadius;
	if (ensureMsgf(!FMath::IsNaN(BoundsRadius) && FMath::IsFinite(BoundsRadius),
		TEXT("%s had an ill-formed bbox and was skipped during shadow setup, contact DavidH."), *GetObjectName(this->ComponentForDebuggingOnly)))
	{
		OutChildSceneInfos.Add(this);
	}
	else
	{
		// return, leaving the TArray empty
		return;
	}

#else 
	// add self at the head of this queue
	OutChildSceneInfos.Add(this);
#endif

	if (!LightingAttachmentRoot.IsValid() && Proxy->LightAttachmentsAsGroup())
	{
		const FAttachmentGroupSceneInfo* AttachmentGroup = Scene->AttachmentGroups.Find(PrimitiveComponentId);

		if (AttachmentGroup)
		{
			
			for (int32 ChildIndex = 0, ChildIndexMax = AttachmentGroup->Primitives.Num(); ChildIndex < ChildIndexMax; ChildIndex++)
			{
				FPrimitiveSceneInfo* ShadowChild = AttachmentGroup->Primitives[ChildIndex];
#if ENABLE_NAN_DIAGNOSTIC
				// Only enqueue objects with valid bounds using the normality of the SphereRaduis as criteria.

				const float& ShadowChildBoundsRadius = ShadowChild->Proxy->GetBounds().SphereRadius;

				if (ensureMsgf(!FMath::IsNaN(ShadowChildBoundsRadius) && FMath::IsFinite(ShadowChildBoundsRadius),
					TEXT("%s had an ill-formed bbox and was skipped during shadow setup, contact DavidH."), *GetObjectName(ShadowChild->ComponentForDebuggingOnly)))
				{
					checkSlow(!OutChildSceneInfos.Contains(ShadowChild))
				    OutChildSceneInfos.Add(ShadowChild);
				}
#else
				// enqueue all objects.
				checkSlow(!OutChildSceneInfos.Contains(ShadowChild))
			    OutChildSceneInfos.Add(ShadowChild);
#endif
			}
		}
	}
}

void FPrimitiveSceneInfo::GatherLightingAttachmentGroupPrimitives(TArray<const FPrimitiveSceneInfo*, SceneRenderingAllocator>& OutChildSceneInfos) const
{
	OutChildSceneInfos.Add(this);

	if (!LightingAttachmentRoot.IsValid() && Proxy->LightAttachmentsAsGroup())
	{
		const FAttachmentGroupSceneInfo* AttachmentGroup = Scene->AttachmentGroups.Find(PrimitiveComponentId);

		if (AttachmentGroup)
		{
			for (int32 ChildIndex = 0, ChildIndexMax = AttachmentGroup->Primitives.Num(); ChildIndex < ChildIndexMax; ChildIndex++)
			{
				const FPrimitiveSceneInfo* ShadowChild = AttachmentGroup->Primitives[ChildIndex];

				checkSlow(!OutChildSceneInfos.Contains(ShadowChild))
			    OutChildSceneInfos.Add(ShadowChild);
			}
		}
	}
}

FBoxSphereBounds FPrimitiveSceneInfo::GetAttachmentGroupBounds() const
{
	FBoxSphereBounds Bounds = Proxy->GetBounds();

	if (!LightingAttachmentRoot.IsValid() && Proxy->LightAttachmentsAsGroup())
	{
		const FAttachmentGroupSceneInfo* AttachmentGroup = Scene->AttachmentGroups.Find(PrimitiveComponentId);

		if (AttachmentGroup)
		{
			for (int32 ChildIndex = 0; ChildIndex < AttachmentGroup->Primitives.Num(); ChildIndex++)
			{
				FPrimitiveSceneInfo* AttachmentChild = AttachmentGroup->Primitives[ChildIndex];
				Bounds = Bounds + AttachmentChild->Proxy->GetBounds();
			}
		}
	}

	return Bounds;
}

uint32 FPrimitiveSceneInfo::GetMemoryFootprint()
{
	return( sizeof( *this ) + HitProxies.GetAllocatedSize() + StaticMeshes.GetAllocatedSize() + StaticMeshRelevances.GetAllocatedSize() );
}

void FPrimitiveSceneInfo::ApplyWorldOffset(FVector InOffset)
{
	Proxy->ApplyWorldOffset(InOffset);
}

void FPrimitiveSceneInfo::UpdateIndirectLightingCacheBuffer(
	const FIndirectLightingCache* LightingCache,
	const FIndirectLightingCacheAllocation* LightingAllocation,
	FVector VolumetricLightmapLookupPosition,
	uint32 SceneFrameNumber,
	FVolumetricLightmapSceneData* VolumetricLightmapSceneData)
{
	FIndirectLightingCacheUniformParameters Parameters;

	GetIndirectLightingCacheParameters(
		Scene->GetFeatureLevel(),
		Parameters,
		LightingCache,
		LightingAllocation,
		VolumetricLightmapLookupPosition,
		SceneFrameNumber,
		VolumetricLightmapSceneData);

	if (IndirectLightingCacheUniformBuffer)
	{
		IndirectLightingCacheUniformBuffer.UpdateUniformBufferImmediate(Parameters);
	}
}

void FPrimitiveSceneInfo::UpdateIndirectLightingCacheBuffer()
{
	// The update is invalid if the lighting cache allocation was not in a functional state.
	if (bIndirectLightingCacheBufferDirty && (!IndirectLightingCacheAllocation || (Scene->IndirectLightingCache.IsInitialized() && IndirectLightingCacheAllocation->bHasEverUpdatedSingleSample)))
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UpdateIndirectLightingCacheBuffer);

		if (!RHISupportsVolumeTextures(Scene->GetFeatureLevel())
			&& Scene->VolumetricLightmapSceneData.HasData()
			&& (Proxy->IsMovable() || Proxy->NeedsUnbuiltPreviewLighting() || Proxy->GetLightmapType() == ELightmapType::ForceVolumetric)
			&& Proxy->WillEverBeLit())
		{
			UpdateIndirectLightingCacheBuffer(
				nullptr, 
				nullptr,
				Proxy->GetBounds().Origin,
				Scene->GetFrameNumber(),
				&Scene->VolumetricLightmapSceneData);
		}
		else if (IndirectLightingCacheAllocation)
		{
			UpdateIndirectLightingCacheBuffer(
				&Scene->IndirectLightingCache,
				IndirectLightingCacheAllocation,
				FVector(0, 0, 0),
				0,
				nullptr);
		}
		else
		{
			// Fallback to the global empty buffer parameters
			UpdateIndirectLightingCacheBuffer(nullptr, nullptr, FVector(0.0f, 0.0f, 0.0f), 0, nullptr);
		}

		bIndirectLightingCacheBufferDirty = false;
	}
}

void FPrimitiveSceneInfo::ClearIndirectLightingCacheBuffer(bool bSingleFrameOnly)
{
	if (!bSingleFrameOnly || Proxy->IsOftenMoving())
	{
		MarkIndirectLightingCacheBufferDirty();
	}
}

void FPrimitiveSceneInfo::GetStaticMeshesLODRange(int8& OutMinLOD, int8& OutMaxLOD) const
{
	OutMinLOD = MAX_int8;
	OutMaxLOD = 0;

	for (int32 MeshIndex = 0; MeshIndex < StaticMeshRelevances.Num(); ++MeshIndex)
	{
		const FStaticMeshBatchRelevance& MeshRelevance = StaticMeshRelevances[MeshIndex];
		OutMinLOD = FMath::Min(OutMinLOD, MeshRelevance.LODIndex);
		OutMaxLOD = FMath::Max(OutMaxLOD, MeshRelevance.LODIndex);
	}
}

const FMeshBatch* FPrimitiveSceneInfo::GetMeshBatch(int8 InLODIndex) const
{
	if (StaticMeshes.IsValidIndex(InLODIndex))
	{
		return &StaticMeshes[InLODIndex];
	}

	return nullptr;
}

bool FPrimitiveSceneInfo::NeedsReflectionCaptureUpdate() const
{
	return bNeedsCachedReflectionCaptureUpdate && 
		// For mobile, the per-object reflection is used for everything
		(Scene->GetShadingPath() == EShadingPath::Mobile || IsForwardShadingEnabled(Scene->GetShaderPlatform()));
}

void FPrimitiveSceneInfo::CacheReflectionCaptures()
{
	// do not use Scene->PrimitiveBounds here, as it may be not initialized yet
	FBoxSphereBounds BoxSphereBounds = Proxy->GetBounds(); 
	
	CachedReflectionCaptureProxy = Scene->FindClosestReflectionCapture(BoxSphereBounds.Origin);
	CachedPlanarReflectionProxy = Scene->FindClosestPlanarReflection(BoxSphereBounds);
	if (Scene->GetShadingPath() == EShadingPath::Mobile)
	{
		// mobile HQ reflections
		Scene->FindClosestReflectionCaptures(BoxSphereBounds.Origin, CachedReflectionCaptureProxies);
	}
	
	bNeedsCachedReflectionCaptureUpdate = false;
}
