// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneCore.h: Core scene definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Templates/RefCounting.h"
#include "HitProxies.h"
#include "MeshBatch.h"
#include "MeshPassProcessor.h"

class FLightSceneInfo;
class FPrimitiveSceneInfo;
class FScene;
class UExponentialHeightFogComponent;

/**
 * An interaction between a light and a primitive.
 */
class FLightPrimitiveInteraction
{
public:

	/** Creates an interaction for a light-primitive pair. */
	static void InitializeMemoryPool();
	static void Create(FLightSceneInfo* LightSceneInfo,FPrimitiveSceneInfo* PrimitiveSceneInfo);
	static void Destroy(FLightPrimitiveInteraction* LightPrimitiveInteraction);

	/** Returns current size of memory pool */
	static uint32 GetMemoryPoolSize();

	// Accessors.
	bool HasShadow() const { return bCastShadow; }
	bool IsLightMapped() const { return bLightMapped; }
	bool IsDynamic() const { return bIsDynamic; }
	bool IsShadowMapped() const { return bIsShadowMapped; }
	bool IsUncachedStaticLighting() const { return bUncachedStaticLighting; }
	bool HasTranslucentObjectShadow() const { return bHasTranslucentObjectShadow; }
	bool HasInsetObjectShadow() const { return bHasInsetObjectShadow; }
	bool CastsSelfShadowOnly() const { return bSelfShadowOnly; }
	bool IsMobileDynamicPointLight() const { return bMobileDynamicPointLight; }
	FLightSceneInfo* GetLight() const { return LightSceneInfo; }
	int32 GetLightId() const { return LightId; }
	FPrimitiveSceneInfo* GetPrimitiveSceneInfo() const { return PrimitiveSceneInfo; }
	FLightPrimitiveInteraction* GetNextPrimitive() const { return NextPrimitive; }
	FLightPrimitiveInteraction* GetNextLight() const { return NextLight; }

	/** Hash function required for TMap support */
	friend uint32 GetTypeHash( const FLightPrimitiveInteraction* Interaction )
	{
		return (uint32)Interaction->LightId;
	}

	/** Clears cached shadow maps, if possible */
	void FlushCachedShadowMapData();

	/** Custom new/delete */
	void* operator new(size_t Size);
	void operator delete(void *RawMemory);

private:
	/** The light which affects the primitive. */
	FLightSceneInfo* LightSceneInfo;

	/** The primitive which is affected by the light. */
	FPrimitiveSceneInfo* PrimitiveSceneInfo;

	/** A pointer to the NextPrimitive member of the previous interaction in the light's interaction list. */
	FLightPrimitiveInteraction** PrevPrimitiveLink;

	/** The next interaction in the light's interaction list. */
	FLightPrimitiveInteraction* NextPrimitive;

	/** A pointer to the NextLight member of the previous interaction in the primitive's interaction list. */
	FLightPrimitiveInteraction** PrevLightLink;

	/** The next interaction in the primitive's interaction list. */
	FLightPrimitiveInteraction* NextLight;

	/** The index into Scene->Lights of the light which affects the primitive. */
	int32 LightId;

	/** True if the primitive casts a shadow from the light. */
	uint32 bCastShadow : 1;

	/** True if the primitive has a light-map containing the light. */
	uint32 bLightMapped : 1;

	/** True if the interaction is dynamic. */
	uint32 bIsDynamic : 1;

	/** Whether the light's shadowing is contained in the primitive's static shadow map. */
	uint32 bIsShadowMapped : 1;

	/** True if the interaction is an uncached static lighting interaction. */
	uint32 bUncachedStaticLighting : 1;

	/** True if the interaction has a translucent per-object shadow. */
	uint32 bHasTranslucentObjectShadow : 1;

	/** True if the interaction has an inset per-object shadow. */
	uint32 bHasInsetObjectShadow : 1;

	/** True if the primitive only shadows itself. */
	uint32 bSelfShadowOnly : 1;

	/** True this is an ES2 dynamic point light interaction. */
	uint32 bMobileDynamicPointLight : 1;

	/** Initialization constructor. */
	FLightPrimitiveInteraction(FLightSceneInfo* InLightSceneInfo,FPrimitiveSceneInfo* InPrimitiveSceneInfo,
		bool bIsDynamic,bool bInLightMapped,bool bInIsShadowMapped, bool bInHasTranslucentObjectShadow, bool bInHasInsetObjectShadow);

	/** Hide dtor */
	~FLightPrimitiveInteraction();

};

/**
 * A mesh which is defined by a primitive at scene segment construction time and never changed.
 * Lights are attached and detached as the segment containing the mesh is added or removed from a scene.
 */
class FStaticMeshBatch : public FMeshBatch
{
public:

	/** The render info for the primitive which created this mesh. */
	FPrimitiveSceneInfo* PrimitiveSceneInfo;

	/** The index of the mesh in the scene's static meshes array. */
	int32 Id;

	/** Index of the mesh into the scene's StaticMeshBatchVisibility array. */
	int32 BatchVisibilityId;

	// Constructor/destructor.
	FStaticMeshBatch(
		FPrimitiveSceneInfo* InPrimitiveSceneInfo,
		const FMeshBatch& InMesh,
		FHitProxyId InHitProxyId
		):
		FMeshBatch(InMesh),
		PrimitiveSceneInfo(InPrimitiveSceneInfo),
		Id(INDEX_NONE),
		BatchVisibilityId(INDEX_NONE)
	{
		BatchHitProxyId = InHitProxyId;
	}

	~FStaticMeshBatch();

private:
	/** Private copy constructor. */
	FStaticMeshBatch(const FStaticMeshBatch& InStaticMesh):
		FMeshBatch(InStaticMesh),
		PrimitiveSceneInfo(InStaticMesh.PrimitiveSceneInfo),
		Id(InStaticMesh.Id),
		BatchVisibilityId(InStaticMesh.BatchVisibilityId)
	{}
};

/**
 * FStaticMeshBatch data which is InitViews specific. Stored separately for cache efficiency.
 */
class FStaticMeshBatchRelevance
{
public:
	FStaticMeshBatchRelevance(const FStaticMeshBatch& StaticMesh, float InScreenSize, bool InbSupportsCachingMeshDrawCommands)
		: Id(StaticMesh.Id)
		, ScreenSize(InScreenSize)
		, CommandInfosBase(0)
		, LODIndex(StaticMesh.LODIndex)
		, NumElements(StaticMesh.Elements.Num())
		, bDitheredLODTransition(StaticMesh.bDitheredLODTransition)
		, bRequiresPerElementVisibility(StaticMesh.bRequiresPerElementVisibility)
		, bSelectable(StaticMesh.bSelectable)
		, CastShadow(StaticMesh.CastShadow)
		, bUseForMaterial(StaticMesh.bUseForMaterial)
		, bUseForDepthPass(StaticMesh.bUseForDepthPass)
		, bUseAsOccluder(StaticMesh.bUseAsOccluder)
		, bSupportsCachingMeshDrawCommands(InbSupportsCachingMeshDrawCommands)
	{
	}

	/** The index of the mesh in the scene's static meshes array. */
	int32 Id;

	/** The screen space size to draw this primitive at */
	float ScreenSize;

	/** Starting offset into continuous array of command infos for this mesh in FPrimitiveSceneInfo::CachedMeshDrawCommandInfos. */
	FMeshPassMask CommandInfosMask;

	/* Every bit corresponds to one MeshPass. If bit is set, then FPrimitiveSceneInfo::CachedMeshDrawCommandInfos contains this mesh pass. */
	uint16 CommandInfosBase;

	/** LOD index of the mesh, used for fading LOD transitions. */
	int8 LODIndex;

	/** Number of elements in this mesh. */
	uint16 NumElements;

	/** Whether the mesh batch should apply dithered LOD. */
	uint8 bDitheredLODTransition : 1;

	/** Whether the mesh batch needs VertexFactory->GetStaticBatchElementVisibility to be called each frame to determine which elements of the batch are visible. */
	uint8 bRequiresPerElementVisibility : 1;

	/** Whether the mesh batch can be selected through editor selection, aka hit proxies. */
	uint8 bSelectable : 1;

	uint8 CastShadow		: 1; // Whether it can be used in shadow renderpasses.
	uint8 bUseForMaterial	: 1; // Whether it can be used in renderpasses requiring material outputs.
	uint8 bUseForDepthPass	: 1; // Whether it can be used in depth pass.
	uint8 bUseAsOccluder	: 1; // User hint whether it's a good occluder.

	/** Cached from vertex factory to avoid dereferencing VF in InitViews. */
	uint8 bSupportsCachingMeshDrawCommands : 1;

	/** Computes index of cached mesh draw command in FPrimitiveSceneInfo::CachedMeshDrawCommandInfos, for a given mesh pass. */
	int32 GetStaticMeshCommandInfoIndex(EMeshPass::Type MeshPass) const;
};

/** The properties of a exponential height fog layer which are used for rendering. */
class FExponentialHeightFogSceneInfo
{
public:

	struct FExponentialHeightFogSceneData
	{
		float Height;
		float Density;
		float HeightFalloff;
	};

	/** Number of supported individual fog settings on this ExponentialHeightFog */
	static constexpr int NumFogs = 2;

	/** The fog component the scene info is for. */
	const UExponentialHeightFogComponent* Component;
	FExponentialHeightFogSceneData FogData[NumFogs];
	float FogMaxOpacity;
	float StartDistance; 
	float FogCutoffDistance;
	float LightTerminatorAngle;
	FLinearColor FogColor;
	float DirectionalInscatteringExponent; 
	float DirectionalInscatteringStartDistance;
	FLinearColor DirectionalInscatteringColor;
	UTextureCube* InscatteringColorCubemap;
	float InscatteringColorCubemapAngle;
	float FullyDirectionalInscatteringColorDistance;
	float NonDirectionalInscatteringColorDistance;

	bool bEnableVolumetricFog;
	float VolumetricFogScatteringDistribution;
	FLinearColor VolumetricFogAlbedo;
	FLinearColor VolumetricFogEmissive;
	float VolumetricFogExtinctionScale;
	float VolumetricFogDistance;
	float VolumetricFogStaticLightingScatteringIntensity;
	bool bOverrideLightColorsWithFogInscatteringColors;

	/** Initialization constructor. */
	FExponentialHeightFogSceneInfo(const UExponentialHeightFogComponent* InComponent);
};

/** Returns true if the indirect lighting cache can be used at all. */
extern bool IsIndirectLightingCacheAllowed(ERHIFeatureLevel::Type InFeatureLevel);

/** Returns true if the indirect lighting cache can use the volume texture atlas on this feature level. */
extern bool CanIndirectLightingCacheUseVolumeTexture(ERHIFeatureLevel::Type InFeatureLevel);
