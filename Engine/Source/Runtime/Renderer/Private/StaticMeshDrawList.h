// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	StaticMeshDrawList.h: Static mesh draw list definition.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RenderResource.h"
#include "EngineDefines.h"
#include "RendererInterface.h"
#include "ScenePrivateBase.h"
#include "SceneCore.h"
#include "SceneRendering.h"

extern ENGINE_API bool GDrawListsLocked;

/** Base class of the static draw list, used when comparing draw lists and the drawing policy type is not necessary. */
class FStaticMeshDrawListBase
{
public:

	static SIZE_T TotalBytesUsed;
};

/**
 * Statistics for a static mesh draw list.
 */
struct FDrawListStats
{
	int32 NumMeshes;
	int32 NumDrawingPolicies;
	int32 MedianMeshesPerDrawingPolicy;
	int32 MaxMeshesPerDrawingPolicy;
	int32 NumSingleMeshDrawingPolicies;
	TMap<FString, int32> SingleMeshPolicyMatchFailedReasons;
	TMap<FName, int32> SingleMeshPolicyVertexFactoryFrequency;
};


inline uint8 PointerHash8(const void* Ptr)
{
	const int32 PtrShift1 = PLATFORM_64BITS ? 4 : 3;
	const int32 PtrShift2 = PLATFORM_64BITS ? 12 : 11;
	uint8 Hash1 = (reinterpret_cast<UPTRINT>(Ptr) >> PtrShift1) & 0xff;
	uint8 Hash2 = (reinterpret_cast<UPTRINT>(Ptr) >> PtrShift2) & 0xff;
	return Hash1^Hash2;
}

/** Fields in the key used to sort mesh elements in a draw list. */

#define USE_SORT_DRAWLISTS_BY_SHADER (PLATFORM_ANDROID)

#if !USE_SORT_DRAWLISTS_BY_SHADER

struct FDrawListSortKeyFields
{
	uint64 MeshElementIndex : 16;			//
	uint64 DepthBits : 8;					// Order by mesh depth
	uint64 MeshMI : 8;						// Order by mesh material instance within DrawPolicy (Tex/Constants)
	uint64 MeshVF : 8;						// Order by mesh VertexFactory within DrawPolicy (VBO)
	uint64 DrawingPolicyIndex : 16;			// Order by DrawPolicy ( PSO )
	uint64 DrawingPolicyDepthBits : 7;		// Order DrawingPolicies front to back
	uint64 bBackground : 1;					// Non-background meshes first 
};

/** Key for sorting mesh elements. */
union FDrawListSortKey
{
	FDrawListSortKeyFields Fields;
	uint64 PackedInt;
};

FORCEINLINE bool operator<(FDrawListSortKey A, FDrawListSortKey B)
{
	return A.PackedInt < B.PackedInt;
}

FORCEINLINE void ZeroDrawListSortKey(FDrawListSortKey& A)
{
	A.PackedInt = 0;
}

#else

struct FDrawListSortKeyFields
{
	uint64 MeshElementIndex : 16;			//
	uint64 DepthBits : 8;					// Order by mesh depth
	uint64 MeshVF : 8;						// Order by mesh VertexFactory within DrawPolicy (VBO)
	uint64 MeshMI : 8;						// Order by mesh material instance (Tex/Constants)
	uint64 DrawingPolicyIndex : 16;			// Order by DrawPolicy ( PSO )
	uint64 DrawingPolicyDepthBits : 7;		// Order DrawingPolicies front to back
	uint64 PixelShaderHash : 8;				// Order by mesh pixel shader
	uint64 VertexShaderHash : 8;			// Order by mesh vertex shader
	uint64 bBackground : 1;					// Non-background meshes first 
};

struct FPackedIntPair
{
	uint64 PackedIntLow;
	uint64 PackedIntHigh;
};

/** Key for sorting mesh elements. */
union FDrawListSortKey
{
	FDrawListSortKeyFields Fields;
	FPackedIntPair PackedIntPair;
};

FORCEINLINE bool operator<(FDrawListSortKey A, FDrawListSortKey B)
{
	return (A.PackedIntPair.PackedIntHigh == B.PackedIntPair.PackedIntHigh) ?
		A.PackedIntPair.PackedIntLow < B.PackedIntPair.PackedIntLow :
		A.PackedIntPair.PackedIntHigh < B.PackedIntPair.PackedIntHigh;
}

FORCEINLINE void ZeroDrawListSortKey(FDrawListSortKey& A)
{
	A.PackedIntPair.PackedIntLow = 0;
	A.PackedIntPair.PackedIntHigh = 0;
}

FORCEINLINE void SetShadersDrawListSortKey(FDrawListSortKey& A, const FBoundShaderStateInput& BSSI)
{
	A.Fields.PixelShaderHash = PointerHash8(BSSI.PixelShaderRHI);
	A.Fields.VertexShaderHash = PointerHash8(BSSI.VertexShaderRHI);
}

#endif


/** Builds a sort key. */
inline FDrawListSortKey GetSortKey(bool bBackground, float BoundsRadius, float DrawingPolicyDistanceSq, int32 DrawingPolicyIndex, float DistanceSq, int32 MeshElementIndex, FStaticMesh* Mesh)
{
	union FFloatToInt { float F; uint32 I; };
	FFloatToInt F2I;
	
	FDrawListSortKey Key;
	ZeroDrawListSortKey(Key);

	Key.Fields.bBackground = bBackground || BoundsRadius > HALF_WORLD_MAX/4.0f;
	F2I.F = DrawingPolicyDistanceSq/HALF_WORLD_MAX;
	Key.Fields.DrawingPolicyDepthBits = (F2I.I >> 24) & 0xff; // store policy depth 7 bit exponent
	Key.Fields.DrawingPolicyIndex = DrawingPolicyIndex;
	Key.Fields.MeshVF = PointerHash8(Mesh->VertexFactory);
	Key.Fields.MeshMI = PointerHash8(Mesh->MaterialRenderProxy);
	F2I.F = DistanceSq/HALF_WORLD_MAX;
	Key.Fields.DepthBits = (F2I.I >> 23) & 0xff; // store mesh depth 8 bit exponent
	Key.Fields.MeshElementIndex = MeshElementIndex;
	return Key;
}


/**
 * A set of static meshs, each associated with a mesh drawing policy of a particular type.
 * @param DrawingPolicyType - The drawing policy type used to draw mesh in this draw list.
 * @param HashSize - The number of buckets to use in the drawing policy hash.
 */
template<typename DrawingPolicyType>
class FDrawVisibleAnyThreadTask;

template<typename DrawingPolicyType>
class TStaticMeshDrawList : public FStaticMeshDrawListBase, public FRenderResource
{
public:
	friend class FDrawVisibleAnyThreadTask<DrawingPolicyType>;

	typedef typename DrawingPolicyType::ElementDataType ElementPolicyDataType;

private:

	/** A handle to an element in the draw list.  Used by FStaticMesh to keep track of draw lists containing the mesh. */
	class FElementHandle : public FStaticMesh::FDrawListElementLink
	{
	public:

		/** Initialization constructor. */
		FElementHandle(TStaticMeshDrawList* InStaticMeshDrawList, FSetElementId InSetId, int32 InElementIndex) :
			StaticMeshDrawList(InStaticMeshDrawList)
			, SetId(InSetId)
			, ElementIndex(InElementIndex)
		{
		}

		virtual bool IsInDrawList(const FStaticMeshDrawListBase* DrawList) const
		{
			return DrawList == StaticMeshDrawList;
		}
		// FAbstractDrawListElementLink interface.
		virtual void Remove(const bool bUnlinkMesh = true );

	private:
		TStaticMeshDrawList* StaticMeshDrawList;
		FSetElementId SetId;
		int32 ElementIndex;
	};

	/**
	 * This structure stores the info needed for visibility culling a static mesh element.
	 * Stored separately to avoid bringing the other info about non-visible meshes into the cache.
	 */
	struct FElementCompact
	{
		int32 MeshId;
		FElementCompact() {}
		FElementCompact(int32 InMeshId)
			: MeshId(InMeshId)
		{}
	};

	struct FElement
	{
		ElementPolicyDataType PolicyData;
		FStaticMesh* Mesh;
		FBoxSphereBounds Bounds;
		bool bBackground;
		TRefCountPtr<FElementHandle> Handle;

		/** Default constructor. */
		FElement() :
			Mesh(NULL)
		{}

		/** Minimal initialization constructor. */
		FElement(
			FStaticMesh* InMesh,
			const ElementPolicyDataType& InPolicyData,
			TStaticMeshDrawList* StaticMeshDrawList,
			FSetElementId SetId,
			int32 ElementIndex
		) :
			PolicyData(InPolicyData),
			Mesh(InMesh),
			Handle(new FElementHandle(StaticMeshDrawList, SetId, ElementIndex))
		{
			// Cache bounds so we can use them for sorting quickly, without having to dereference the proxy
			Bounds = Mesh->PrimitiveSceneInfo->Proxy->GetBounds();
			bBackground = Mesh->PrimitiveSceneInfo->Proxy->TreatAsBackgroundForOcclusion();
		}

		/** Destructor. */
		~FElement()
		{
			if (Mesh)
			{
				Mesh->UnlinkDrawList(Handle);
			}
		}
	};

	/** A set of draw list elements with the same drawing policy. */
	struct FDrawingPolicyLink
	{
		/** The elements array and the compact elements array are always synchronized */
		TArray<FElementCompact>		 CompactElements;
		TArray<FElement>			 Elements;
		DrawingPolicyType		 	 DrawingPolicy;
		FBoundShaderStateInput		 BoundShaderStateInput;
		ERHIFeatureLevel::Type		 FeatureLevel;

		/** Used when sorting policy links */
		FSphere						 CachedBoundingSphere;

		/** The id of this link in the draw list's set of drawing policy links. */
		FSetElementId SetId;

		TStaticMeshDrawList* DrawList;

		uint32 VisibleCount;

		/** Initialization constructor. */
		FDrawingPolicyLink(TStaticMeshDrawList* InDrawList, const DrawingPolicyType& InDrawingPolicy, ERHIFeatureLevel::Type InFeatureLevel) :
			DrawingPolicy(InDrawingPolicy),
			FeatureLevel(InFeatureLevel),
			DrawList(InDrawList),
			VisibleCount(0)
		{
			check(IsInRenderingThread());
			BoundShaderStateInput = DrawingPolicy.GetBoundShaderStateInput(FeatureLevel);
		}

		SIZE_T GetSizeBytes() const
		{
			return sizeof(*this) + CompactElements.GetAllocatedSize() + Elements.GetAllocatedSize();
		}
	};

	/** Functions to extract the drawing policy from FDrawingPolicyLink as a key for TSet. */
	struct FDrawingPolicyKeyFuncs : BaseKeyFuncs<FDrawingPolicyLink, DrawingPolicyType>
	{
		static const DrawingPolicyType& GetSetKey(const FDrawingPolicyLink& Link)
		{
			return Link.DrawingPolicy;
		}

		static bool Matches(const DrawingPolicyType& A, const DrawingPolicyType& B)
		{
			return A.Matches(B).Result();
		}

		static uint32 GetKeyHash(const DrawingPolicyType& DrawingPolicy)
		{
			return DrawingPolicy.GetTypeHash();
		}
	};

	/**
	* Draws a single FElement
	* @param View - The view of the meshes to render.
	* @param Element - The mesh element
	* @param BatchElementMask - Visibility bitmask for element's batch elements.
	* @param DrawingPolicyLink - the drawing policy link
	* @param bDrawnShared - determines whether to draw shared
	*/
	int32 DrawElement(FRHICommandList& RHICmdList, const FViewInfo& View, const typename DrawingPolicyType::ContextDataType PolicyContext, FDrawingPolicyRenderState& DrawRenderState, const FElement& Element, uint64 BatchElementMask, FDrawingPolicyLink* DrawingPolicyLink, bool &bDrawnShared);

public:

	/**
	 * Adds a mesh to the draw list.
	 * @param Mesh - The mesh to add.
	 * @param PolicyData - The drawing policy data for the mesh.
	 * @param InDrawingPolicy - The drawing policy to use to draw the mesh.
	 * @param InFeatureLevel - The feature level of the scene we're rendering
	 */
	void AddMesh(
		FStaticMesh* Mesh,
		const ElementPolicyDataType& PolicyData,
		const DrawingPolicyType& InDrawingPolicy,
		ERHIFeatureLevel::Type InFeatureLevel
	);

	/**
	* Draws only the static meshes which are in the visibility map, limited to a range of policies
	* @param View - The view of the meshes to render
	* @param StaticMeshVisibilityMap - An map from FStaticMesh::Id to visibility state.
	* @param BatchVisibilityArray - An array of batch element visibility bitmasks.
	* @param FirstPolicy - First policy to render
	* @param LastPolicy - Last policy to render
	* @return True if any static meshes were drawn.
	*/
	bool DrawVisibleInner(FRHICommandList& RHICmdList,
		const FViewInfo& View,
		const typename DrawingPolicyType::ContextDataType PolicyContext,
		FDrawingPolicyRenderState& DrawRenderState,
		const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap,
		const TArray<uint64, SceneRenderingAllocator>& BatchVisibilityArray,
		int32 FirstPolicy,
		int32 LastPolicy,
		bool bUpdateCounts);

	/**
	 * Draws only the static meshes which are in the visibility map.
	 * @param View - The view of the meshes to render.
	 * @param StaticMeshVisibilityMap - An map from FStaticMesh::Id to visibility state.
	 * @param BatchVisibilityArray - An array of batch element visibility bitmasks.
	 * @return True if any static meshes were drawn.
	 */
	bool DrawVisible(FRHICommandList& RHICmdList, const FViewInfo& View, const typename DrawingPolicyType::ContextDataType PolicyContext, const FDrawingPolicyRenderState& DrawRenderState, const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap, const TArray<uint64, SceneRenderingAllocator>& BatchVisibilityArray);
	
	/**
	* Draws only the static meshes which are in the visibility map.
	* @param StaticMeshVisibilityMap - An map from FStaticMesh::Id to visibility state.
	* @param BatchVisibilityArray - An array of batch element visibility bitmasks.
	* @param ParallelCommandListSet - holds information on how to get a fresh command list and deal with submits, etc
	*/
	void DrawVisibleParallel(
		const typename DrawingPolicyType::ContextDataType PolicyContext,
		const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap,
		const TArray<uint64, SceneRenderingAllocator>& BatchVisibilityArray,
		FParallelCommandListSet& ParallelCommandListSet);

	/**
	* Draws only the static meshes which are in the visibility map, sorted front-to-back.
	* @param View - The view of the meshes to render
	* @param StaticMeshVisibilityMap - An map from FStaticMesh::Id to visibility state.
	* @param BatchVisibilityArray - An array of batch element visibility bitmasks.
	* @param MaxToDraw - The maximum number of meshes to be drawn.
	 * @return The number of static meshes drawn.
	*/
	int32 DrawVisibleFrontToBack(
		FRHICommandList& RHICmdList,
		const FViewInfo& View,
		FDrawingPolicyRenderState& DrawRenderState,
		const typename DrawingPolicyType::ContextDataType PolicyContext,
		const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap,
		const TArray<uint64, SceneRenderingAllocator>& BatchVisibilityArray,
		int32 MaxToDraw);

	/**
	 * Helper functions when policy context is not needed.
	 */
	inline bool DrawVisible(const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap)
	{
		return DrawVisible(View, typename DrawingPolicyType::ContextDataType(View.IsInstancedStereoPass()), DrawRenderState, StaticMeshVisibilityMap);
	}

	inline bool DrawVisible(FRHICommandList& RHICmdList, const FViewInfo& View, const FDrawingPolicyRenderState& DrawRenderState, const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap, const TArray<uint64,SceneRenderingAllocator>& BatchVisibilityArray)
	{
		return DrawVisible(RHICmdList, View, typename DrawingPolicyType::ContextDataType(View.IsInstancedStereoPass()), DrawRenderState, StaticMeshVisibilityMap, BatchVisibilityArray);
	}

	inline void DrawVisibleParallel(const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap, const TArray<uint64, SceneRenderingAllocator>& BatchVisibilityArray, FParallelCommandListSet& ParallelCommandListSet)
	{
		DrawVisibleParallel(typename DrawingPolicyType::ContextDataType(ParallelCommandListSet.View.IsInstancedStereoPass()), StaticMeshVisibilityMap, BatchVisibilityArray, ParallelCommandListSet);
	}

	inline int32 DrawVisibleFrontToBack(FRHICommandList& RHICmdList, const FViewInfo& View, FDrawingPolicyRenderState& DrawRenderState, const TBitArray<SceneRenderingBitArrayAllocator>& StaticMeshVisibilityMap, const TArray<uint64,SceneRenderingAllocator>& BatchVisibilityArray, int32 MaxToDraw)
	{
		return DrawVisibleFrontToBack(RHICmdList, View, DrawRenderState, typename DrawingPolicyType::ContextDataType(View.IsInstancedStereoPass()), StaticMeshVisibilityMap, BatchVisibilityArray, MaxToDraw);
	}

	/** Sorts OrderedDrawingPolicies front to back. */
	void SortFrontToBack(FVector ViewPosition);

	/** Computes bounding boxes for each Drawing Policy using only visible meshes */
	void ComputeVisiblePoliciesBounds(const TBitArray<SceneRenderingBitArrayAllocator>& VisibilityMap);

	/** Builds a list of primitives that use the given materials in this static draw list. */
	void GetUsedPrimitivesBasedOnMaterials(ERHIFeatureLevel::Type InFeatureLevel, const TArray<const FMaterial*>& Materials, TArray<FPrimitiveSceneInfo*>& PrimitivesToUpdate);

	/**
	 * Shifts all meshes bounds by an arbitrary delta.
	 * Called on world origin changes
	 * @param InOffset - The delta to shift by
	 */
	void ApplyWorldOffset(FVector InOffset);

	/**
	 * @return total number of meshes in all draw policies
	 */
	int32 NumMeshes() const;

	TStaticMeshDrawList();
	~TStaticMeshDrawList();

	// FRenderResource interface.
	virtual void ReleaseRHI();

	typedef TSet<FDrawingPolicyLink, FDrawingPolicyKeyFuncs> TDrawingPolicySet;
	/** Sorts OrderedDrawingPolicies front to back.  Relies on static variables SortDrawingPolicySet and SortViewPosition being set. */
	static int32 Compare(FSetElementId A, FSetElementId B, const TDrawingPolicySet * const InSortDrawingPolicySet, const FVector InSortViewPosition);

	/** Computes statistics for this draw list. */
	FDrawListStats GetStats() const;

private:
	void CollectClosestMatchingPolicies(
		typename TArray<FSetElementId>::TConstIterator DrawingPolicyIter,
		TMap<FString, int32>& MatchFailedReasons
		) const;

	/** All drawing policies in the draw list, in rendering order. */
	TArray<FSetElementId> OrderedDrawingPolicies;
	
	/** All drawing policy element sets in the draw list, hashed by drawing policy. */
	TDrawingPolicySet DrawingPolicySet;

	uint32 FrameNumberForVisibleCount;
	uint32 ViewStateUniqueId;
};

/** Helper struct for sorting */
template<typename DrawingPolicyType>
struct TCompareStaticMeshDrawList
{
private:
	const typename TStaticMeshDrawList<DrawingPolicyType>::TDrawingPolicySet * const SortDrawingPolicySet;
	const FVector SortViewPosition;

public:
	TCompareStaticMeshDrawList(const typename TStaticMeshDrawList<DrawingPolicyType>::TDrawingPolicySet * const InSortDrawingPolicySet, const FVector InSortViewPosition)
		: SortDrawingPolicySet(InSortDrawingPolicySet)
		, SortViewPosition(InSortViewPosition)
	{
	}

	FORCEINLINE bool operator()(const FSetElementId& A, const FSetElementId& B) const
	{
		// Use static Compare from TStaticMeshDrawList
		return TStaticMeshDrawList<DrawingPolicyType>::Compare(A, B, SortDrawingPolicySet, SortViewPosition) < 0;
	}
};

#include "StaticMeshDrawList.inl"

