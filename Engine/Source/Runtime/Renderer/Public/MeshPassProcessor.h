// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	MeshPassProcessor.h
=============================================================================*/

#pragma once

#include "MeshMaterialShader.h"
#include "SceneUtils.h"
#include "MeshBatch.h"

#define MESH_DRAW_COMMAND_DEBUG_DATA ((!UE_BUILD_SHIPPING && !UE_BUILD_TEST) || VALIDATE_MESH_COMMAND_BINDINGS || WANTS_DRAW_MESH_EVENTS)

/** Mesh pass types supported. */
namespace EMeshPass
{
	enum Type
	{
		DepthPass,
		BasePass,
		CSMShadowDepth,
		Distortion,
		Velocity,
		TranslucencyStandard,
		TranslucencyAfterDOF,
		TranslucencyAll, /** Drawing all translucency, regardless of separate or standard.  Used when drawing translucency outside of the main renderer, eg FRendererModule::DrawTile. */
		LightmapDensity,
		DebugViewMode, /** Any of EDebugViewShaderMode */
		CustomDepth,
		MobileBasePassCSM,  /** Mobile base pass with CSM shading enabled */
		MobileInverseOpacity,  /** Mobile specific scene capture, Non-cached */

#if WITH_EDITOR
		HitProxy,
		HitProxyOpaqueOnly,
		EditorSelection,
#endif

		Num,
		NumBits = 5,
	};
}
static_assert(EMeshPass::Num <= (1 << EMeshPass::NumBits), "EMeshPass::Num will not fit in EMeshPass::NumBits");

inline const TCHAR* GetMeshPassName(EMeshPass::Type MeshPass)
{
	switch (MeshPass)
	{
	case EMeshPass::DepthPass: return TEXT("DepthPass");
	case EMeshPass::BasePass: return TEXT("BasePass");
	case EMeshPass::CSMShadowDepth: return TEXT("CSMShadowDepth");
	case EMeshPass::Distortion: return TEXT("Distortion");
	case EMeshPass::Velocity: return TEXT("Velocity");
	case EMeshPass::TranslucencyStandard: return TEXT("TranslucencyStandard");
	case EMeshPass::TranslucencyAfterDOF: return TEXT("TranslucencyAfterDOF");
	case EMeshPass::TranslucencyAll: return TEXT("TranslucencyAll");
	case EMeshPass::LightmapDensity: return TEXT("LightmapDensity");
	case EMeshPass::DebugViewMode: return TEXT("DebugViewMode");
	case EMeshPass::CustomDepth: return TEXT("CustomDepth");
	case EMeshPass::MobileBasePassCSM: return TEXT("MobileBasePassCSM");
	case EMeshPass::MobileInverseOpacity: return TEXT("MobileInverseOpacity");
#if WITH_EDITOR
	case EMeshPass::HitProxy: return TEXT("HitProxy");
	case EMeshPass::HitProxyOpaqueOnly: return TEXT("HitProxyOpaqueOnly");
	case EMeshPass::EditorSelection: return TEXT("EditorSelection");
#endif
	}

	checkf(0, TEXT("Missing case for EMeshPass %u"), (uint32)MeshPass);
	return nullptr;
}

/** Mesh pass mask - stores one bit per mesh pass. */
class FMeshPassMask
{
public:
	FMeshPassMask()
		: Data(0)
	{
	}

	void Set(EMeshPass::Type Pass) { Data |= (1 << Pass); }
	bool Get(EMeshPass::Type Pass) const { return !!(Data & (1 << Pass)); }

	void AppendTo(FMeshPassMask& Mask) const { Mask.Data |= Data; }
	void Reset() { Data = 0; }
	bool IsEmpty() const { return Data == 0; }

	uint32 Data;
};

static_assert(sizeof(FMeshPassMask::Data) * 8 >= EMeshPass::Num, "FMeshPassMask::Data is too small to fit all mesh passes.");

struct FRefCountedGraphicsMinimalPipelineStateInitializer
{
	FRefCountedGraphicsMinimalPipelineStateInitializer(const FGraphicsMinimalPipelineStateInitializer& InStateInitializer, int32 InRefNum = 0)
		: StateInitializer(InStateInitializer)
		, RefNum(InRefNum)
	{
	}

	FGraphicsMinimalPipelineStateInitializer StateInitializer;
	int32 RefNum = 0;
};

struct RefCountedGraphicsMinimalPipelineStateInitializerKeyFuncs : DefaultKeyFuncs<FRefCountedGraphicsMinimalPipelineStateInitializer, false>
{
	typedef typename TCallTraits<FGraphicsMinimalPipelineStateInitializer>::ConstReference KeyInitType;

	/**
	 * @return True if the keys match.
	 */
	static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
	{
		return A == B;
	}

	/**
	 * @return The key used to index the given element.
	 */
	static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
	{
		return Element.StateInitializer;
	}

	/** Calculates a hash index for a key. */
	static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
	{
		return GetTypeHash(Key);
	}
};

/** Uniquely represents a FGraphicsMinimalPipelineStateInitializer for fast compares. */
class FGraphicsMinimalPipelineStateId
{
public:
	FORCEINLINE_DEBUGGABLE uint32 GetId() const
	{
		checkSlow(IsValid());
		return PackedId;
	}

	inline bool IsValid() const 
	{
		return bValid != 0;
	}

	inline bool operator==(const FGraphicsMinimalPipelineStateId& rhs) const
	{
		return PackedId == rhs.PackedId;
	}

	inline bool operator!=(const FGraphicsMinimalPipelineStateId& rhs) const
	{
		return !(*this == rhs);
	}
	
	inline const FGraphicsMinimalPipelineStateInitializer& GetPipelineState() const
	{
		const FSetElementId SetElementId = FSetElementId::FromInteger(SetElementIndex);

		if (bOneFrameId)
		{
			return OneFrameIdTable[SetElementId];
		}

		return PersistentIdTable[SetElementId].StateInitializer;
	}

	/**
	 * Get a ref counted persistent pipeline id, which needs to manually released.
	 */
	static FGraphicsMinimalPipelineStateId GetPersistentId(const FGraphicsMinimalPipelineStateInitializer& InPipelineState);

	/**
	 * Removes a persistent pipeline Id from the global persistent Id table.
	 */
	static void RemovePersistentId(FGraphicsMinimalPipelineStateId Id);

	/**
	 * Get a pipeline id, which is valid only for a single frame and doesn't need to be released manually.
	 */
	RENDERER_API static FGraphicsMinimalPipelineStateId GetOneFrameId(const FGraphicsMinimalPipelineStateInitializer& InPipelineState);

	static void ResetOneFrameIdTable();
	static SIZE_T GetPersistentIdTableSize() { return PersistentIdTable.GetAllocatedSize(); }
	static int32 GetPersistentIdNum() { return PersistentIdTable.Num(); }
	static SIZE_T GetOneFrameIdTableSize() { return OneFrameIdTable.GetAllocatedSize(); }

private:
	union
	{
		uint32 PackedId = 0;

		struct
		{
			uint32 SetElementIndex	: 30;
			uint32 bOneFrameId		: 1;
			uint32 bValid			: 1;
		};
	};

	static TSet<FRefCountedGraphicsMinimalPipelineStateInitializer, RefCountedGraphicsMinimalPipelineStateInitializerKeyFuncs> PersistentIdTable;
	static TSet<FGraphicsMinimalPipelineStateInitializer> OneFrameIdTable;
	static FCriticalSection OneFrameIdTableCriticalSection;
};

struct FMeshProcessorShaders
{
	FMeshMaterialShader* VertexShader;
	FMeshMaterialShader* HullShader;
	FMeshMaterialShader* DomainShader;
	FMeshMaterialShader* PixelShader;
	FMeshMaterialShader* GeometryShader;
	FMeshMaterialShader* ComputeShader;
#if RHI_RAYTRACING
	FMeshMaterialShader* RayHitGroupShader;
#endif

	FMeshMaterialShader* GetShader(EShaderFrequency Frequency) const
	{
		if (Frequency == SF_Vertex)
		{
			return VertexShader;
		}
		else if (Frequency == SF_Hull)
		{
			return HullShader;
		}
		else if (Frequency == SF_Domain)
		{
			return DomainShader;
		}
		else if (Frequency == SF_Pixel)
		{
			return PixelShader;
		}
		else if (Frequency == SF_Geometry)
		{
			return GeometryShader;
		}
		else if (Frequency == SF_Compute)
		{
			return ComputeShader;
		}
#if RHI_RAYTRACING
		else if (Frequency == SF_RayHitGroup)
		{
			return RayHitGroupShader;
		}
#endif // RHI_RAYTRACING

		checkf(0, TEXT("Unhandled shader frequency"));
		return nullptr;
	}
};

/** 
 * Number of resource bindings to allocate inline within a FMeshDrawCommand.
 * This is tweaked so that the bindings for BasePass shaders of an average material using a FLocalVertexFactory fit into the inline storage.
 * Overflow of the inline storage will cause a heap allocation per draw (and corresponding cache miss on traversal)
 */
const int32 NumInlineShaderBindings = 10;

/**
* Debug only data for being able to backtrack the origin of given FMeshDrawCommand.
*/
struct FMeshDrawCommandDebugData
{
#if MESH_DRAW_COMMAND_DEBUG_DATA
	const FPrimitiveSceneProxy* PrimitiveSceneProxy;
	const FMaterial* Material;
	const FMaterialRenderProxy* MaterialRenderProxy;
	FMeshMaterialShader* VertexShader;
	FMeshMaterialShader* PixelShader;
#endif
};

/** 
 * Encapsulates shader bindings for a single FMeshDrawCommand.
 */
class FMeshDrawShaderBindings
{
public:

	FMeshDrawShaderBindings() {}
	FMeshDrawShaderBindings(const FMeshDrawShaderBindings& Other)
	{
		CopyFrom(Other);
	}
	RENDERER_API ~FMeshDrawShaderBindings();

	FMeshDrawShaderBindings& operator=(const FMeshDrawShaderBindings& Other)
	{
		CopyFrom(Other);
		return *this;
	}

	/** Allocates space for the bindings of all shaders. */
	void Initialize(FMeshProcessorShaders Shaders);

	/** Called once binding setup is complete. */
	void Finalize(const FMeshProcessorShaders* ShadersForDebugging);

	inline FMeshDrawSingleShaderBindings GetSingleShaderBindings(EShaderFrequency Frequency)
	{
		int32 DataOffset = 0;

		for (int32 BindingIndex = 0; BindingIndex < ShaderLayouts.Num(); BindingIndex++)
		{
			if (ShaderLayouts[BindingIndex].Frequency == Frequency)
			{
				return FMeshDrawSingleShaderBindings(ShaderLayouts[BindingIndex], GetData() + DataOffset);
			}

			DataOffset += ShaderLayouts[BindingIndex].GetDataSizeBytes();
		}

		checkf(0, TEXT("Invalid shader binding frequency requested"));
		FShader Shader;
		return FMeshDrawSingleShaderBindings(FMeshDrawShaderBindingsLayout(&Shader), nullptr);
	}

	/** Set shader bindings on the commandlist, filtered by state cache. */
	void SetOnCommandList(FRHICommandList& RHICmdList, FBoundShaderStateInput Shaders, class FShaderBindingState* StateCacheShaderBindings) const;

	void SetOnCommandListForCompute(FRHICommandList& RHICmdList, FComputeShaderRHIParamRef Shader) const;

#if RHI_RAYTRACING
	void SetRayTracingShaderBindingsForHitGroup(FRHICommandList& RHICmdList, FRayTracingSceneRHIParamRef Scene, uint32 InstanceIndex, uint32 SegmentIndex, FRayTracingPipelineStateRHIParamRef Pipeline, uint32 HitGroupIndex, uint32 ShaderSlot) const;
#endif // RHI_RAYTRACING

	/** Returns whether this set of shader bindings can be merged into an instanced draw call with another. */
	bool MatchesForDynamicInstancing(const FMeshDrawShaderBindings& Rhs) const;

	SIZE_T GetAllocatedSize() const
	{
		SIZE_T Bytes = ShaderLayouts.GetAllocatedSize();
		if (Size > sizeof(InlineStorage))
		{
			Bytes += Size;
		}

		return Bytes;
	}

	void GetShaderFrequencies(TArray<EShaderFrequency, TInlineAllocator<SF_NumFrequencies>>& OutShaderFrequencies) const
	{
		OutShaderFrequencies.Empty(ShaderLayouts.Num());

		for (int32 BindingIndex = 0; BindingIndex < ShaderLayouts.Num(); BindingIndex++)
		{
			OutShaderFrequencies.Add(ShaderLayouts[BindingIndex].Frequency);
		}
	}

	inline int32 GetDataSize() const { return Size; }

private:

	TArray<FMeshDrawShaderBindingsLayout, TInlineAllocator<2>> ShaderLayouts;

	union
	{
		uint8 InlineStorage[NumInlineShaderBindings * sizeof(void*)];
		uint8* HeapData = nullptr;
	};
	
	uint16 Size = 0;

	void Allocate(uint16 InSize)
	{
		check(Size == 0 && HeapData == nullptr);

		Size = InSize;

		if (InSize > ARRAY_COUNT(InlineStorage))
		{
			HeapData = new uint8[InSize];
		}
	}

	void AllocateZeroed(uint32 InSize) 
	{
		Allocate(InSize);

		// Verify no type overflow
		check(Size == InSize);

		FPlatformMemory::Memzero(GetData(), InSize);
	}

	uint8* GetData()
	{
		return Size <= ARRAY_COUNT(InlineStorage) ? &InlineStorage[0] : HeapData;
	}

	const uint8* GetData() const
	{
		return Size <= ARRAY_COUNT(InlineStorage) ? &InlineStorage[0] : HeapData;
	}

	RENDERER_API void CopyFrom(const FMeshDrawShaderBindings& Other);

	template<class RHIShaderType>
	static void SetShaderBindings(
		FRHICommandList& RHICmdList,
		RHIShaderType Shader,
		const class FReadOnlyMeshDrawSingleShaderBindings& RESTRICT SingleShaderBindings,
		FShaderBindingState& RESTRICT ShaderBindingState);

	template<class RHIShaderType>
	static void SetShaderBindings(
		FRHICommandList& RHICmdList,
		RHIShaderType Shader,
		const class FReadOnlyMeshDrawSingleShaderBindings& RESTRICT SingleShaderBindings);
};

/** 
 * FMeshDrawCommand fully describes a mesh pass draw call, captured just above the RHI.  
		FMeshDrawCommand should contain only data needed to draw.  For InitViews payloads, use FVisibleMeshDrawCommand.
 * FMeshDrawCommands are cached at Primitive AddToScene time for vertex factories that support it (no per-frame or per-view shader binding changes).
 * Dynamic Instancing operates at the FMeshDrawCommand level for robustness.  
		Adding per-command shader bindings will reduce the efficiency of Dynamic Instancing, but rendering will always be correct.
 * Any resources referenced by a command must be kept alive for the lifetime of the command.  FMeshDrawCommand is not responsible for lifetime management of resources.
		For uniform buffers referenced by cached FMeshDrawCommand's, RHIUpdateUniformBuffer makes it possible to access per-frame data in the shader without changing bindings.
 */
class FMeshDrawCommand
{
public:
	
	/**
	 * Resource bindings
	 */
	FMeshDrawShaderBindings ShaderBindings;
	FVertexInputStreamArray VertexStreams;
	FIndexBufferRHIParamRef IndexBuffer;

	/**
	 * PSO
	 */
	FGraphicsMinimalPipelineStateId CachedPipelineId;

	/**
	 * Draw command parameters
	 */
	uint32 FirstIndex;
	uint32 NumPrimitives;
	uint32 NumInstances;

	union
	{
		struct 
		{
			uint32 BaseVertexIndex;
			uint32 NumVertices;
		} VertexParams;
		
		FVertexBufferRHIParamRef IndirectArgsBuffer;
	};

	int8 PrimitiveIdStreamIndex;

	/** Non-pipeline state */
	uint8 StencilRef;

	FMeshDrawCommand()
	{}

	bool MatchesForDynamicInstancing(const FMeshDrawCommand& Rhs) const
	{
		return CachedPipelineId == Rhs.CachedPipelineId
			&& StencilRef == Rhs.StencilRef
			&& ShaderBindings.MatchesForDynamicInstancing(Rhs.ShaderBindings)
			&& VertexStreams == Rhs.VertexStreams
			&& PrimitiveIdStreamIndex == Rhs.PrimitiveIdStreamIndex
			&& IndexBuffer == Rhs.IndexBuffer
			&& FirstIndex == Rhs.FirstIndex
			&& NumPrimitives == Rhs.NumPrimitives
			&& NumInstances == Rhs.NumInstances
			&& ((NumPrimitives > 0 && VertexParams.BaseVertexIndex == Rhs.VertexParams.BaseVertexIndex && VertexParams.NumVertices == Rhs.VertexParams.NumVertices)
				|| (NumPrimitives == 0 && IndirectArgsBuffer == Rhs.IndirectArgsBuffer));
	}

	/** Sets shaders on the mesh draw command and allocates room for the shader bindings. */
	RENDERER_API void SetShaders(FVertexDeclarationRHIParamRef VertexDeclaration, const FMeshProcessorShaders& Shaders, FGraphicsMinimalPipelineStateInitializer& PipelineState);

	inline void SetStencilRef(uint32 InStencilRef)
	{
		StencilRef = InStencilRef;
		// Verify no overflow
		checkSlow((uint32)StencilRef == InStencilRef);
	}

	/** Called when the mesh draw command is complete. */
	RENDERER_API void SetDrawParametersAndFinalize(
		const FMeshBatch& MeshBatch, 
		int32 BatchElementIndex,
		FGraphicsMinimalPipelineStateId PipelineId,
		const FMeshProcessorShaders* ShadersForDebugging);

	void Finalize(FGraphicsMinimalPipelineStateId PipelineId, const FMeshProcessorShaders* ShadersForDebugging)
	{
		CachedPipelineId = PipelineId;
		ShaderBindings.Finalize(ShadersForDebugging);	
	}

	/** Submits commands to the RHI Commandlist to draw the MeshDrawCommand. */
	static void SubmitDraw(
		const FMeshDrawCommand& RESTRICT MeshDrawCommand, 
		FVertexBufferRHIParamRef ScenePrimitiveIdsBuffer, 
		int32 PrimitiveIdOffset,
		uint32 InstanceFactor,
		FRHICommandList& CommandList, 
		class FMeshDrawCommandStateCache& RESTRICT StateCache);

	FORCENOINLINE friend uint32 GetTypeHash( const FMeshDrawCommand& Other )
	{
		return Other.CachedPipelineId.GetId();
	}

	void SetDebugData(const FPrimitiveSceneProxy* PrimitiveSceneProxy, const FMaterial* Material, const FMaterialRenderProxy* MaterialRenderProxy, const FMeshProcessorShaders& UntypedShaders)
	{
#if MESH_DRAW_COMMAND_DEBUG_DATA
		DebugData.PrimitiveSceneProxy = PrimitiveSceneProxy;
		DebugData.Material = Material;
		DebugData.MaterialRenderProxy = MaterialRenderProxy;
		DebugData.VertexShader = UntypedShaders.VertexShader;
		DebugData.PixelShader = UntypedShaders.PixelShader;
#endif
	}

	SIZE_T GetAllocatedSize() const
	{
		return ShaderBindings.GetAllocatedSize() + VertexStreams.GetAllocatedSize();
	}

	SIZE_T GetDebugDataSize() const
	{
#if MESH_DRAW_COMMAND_DEBUG_DATA
		return sizeof(DebugData);
#endif
		return 0;
	}

#if MESH_DRAW_COMMAND_DEBUG_DATA
private:
	FMeshDrawCommandDebugData DebugData;
#endif
};

/** FVisibleMeshDrawCommand sort key. */
class RENDERER_API FMeshDrawCommandSortKey
{
public:
	union 
	{
		uint64 PackedData;

		struct
		{
			uint64 VertexShaderHash		: 16; // Order by vertex shader's hash.
			uint64 PixelShaderHash		: 32; // Order by pixel shader's hash.
			uint64 Masked				: 16; // First order by masked.
		} BasePass;

		struct
		{
			uint64 MeshIdInPrimitive	: 16; // Order meshes belonging to the same primitive by a stable id.
			uint64 Distance				: 32; // Order by distance.
			uint64 Priority				: 16; // First order by priority.
		} Translucent;

		struct 
		{
			uint64 VertexShaderHash : 32;	// Order by vertex shader's hash.
			uint64 PixelShaderHash : 32;	// First order by pixel shader's hash.
		} Generic;
	};

	FORCEINLINE bool operator!=(FMeshDrawCommandSortKey B) const
	{
		return PackedData != B.PackedData;
	}

	FORCEINLINE bool operator<(FMeshDrawCommandSortKey B) const
	{
		return PackedData < B.PackedData;
	}

	static const FMeshDrawCommandSortKey Default;
};

/** Interface for the different types of draw lists. */
class FMeshPassDrawListContext
{
public:

	virtual ~FMeshPassDrawListContext() {}

	virtual FMeshDrawCommand& AddCommand(const FMeshDrawCommand& Initializer) = 0;

	virtual void FinalizeCommand(
		const FMeshBatch& MeshBatch, 
		int32 BatchElementIndex,
		int32 DrawPrimitiveId,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode,
		FMeshDrawCommandSortKey SortKey,
		const FGraphicsMinimalPipelineStateInitializer& PipelineState,
		const FMeshProcessorShaders* ShadersForDebugging,
		FMeshDrawCommand& MeshDrawCommand) = 0;
};

/** Storage for Mesh Draw Commands built every frame. */
class FDynamicMeshDrawCommandStorage
{
public:
	// Using TChunkedArray to support growing without moving FMeshDrawCommand, since FVisibleMeshDrawCommand stores a pointer to these
	TChunkedArray<FMeshDrawCommand> MeshDrawCommands;
};

/** 
 * Stores information about a mesh draw command that has been determined to be visible, for further visibility processing. 
 * This class should only store data needed by InitViews operations (visibility, sorting) and not data needed for draw submission, which belongs in FMeshDrawCommand.
 */
class FVisibleMeshDrawCommand
{
public:

	// Note: no ctor as TChunkedArray::CopyToLinearArray requires POD types

	FORCEINLINE_DEBUGGABLE void Setup(const FMeshDrawCommand* InMeshDrawCommand, int32 InDrawPrimitiveIndex, int32 InStateBucketId, ERasterizerFillMode InMeshFillMode, ERasterizerCullMode InMeshCullMode, FMeshDrawCommandSortKey InSortKey)
	{
		MeshDrawCommand = InMeshDrawCommand;
		DrawPrimitiveId = InDrawPrimitiveIndex;
		PrimitiveIdBufferOffset = -1;
		StateBucketId = InStateBucketId;
		MeshFillMode = InMeshFillMode;
		MeshCullMode = InMeshCullMode;
		SortKey = InSortKey;
	}

	// Mesh Draw Command stored separately to avoid fetching its data during sorting
	const FMeshDrawCommand* MeshDrawCommand;

	// Sort key for non state based sorting (e.g. sort translucent draws by depth).
	FMeshDrawCommandSortKey SortKey;

	// Draw PrimitiveId this draw command is associated with - used by the shader to fetch primitive data from the PrimitiveSceneData SRV.
	// If it's < Scene->Primitives.Num() then it's a valid Scene PrimitiveIndex and can be used to backtrack to the FPrimitiveSceneInfo.
	int32 DrawPrimitiveId;

	// Offset into the buffer of PrimitiveIds built for this pass, in int32's.
	int32 PrimitiveIdBufferOffset;

	// Dynamic instancing state bucket ID.  
	// Any commands with the same StateBucketId can be merged into one draw call with instancing.
	// A value of -1 means the draw is not in any state bucket and should be sorted by other factors instead.
	int32 StateBucketId;

	// Needed for view overrides
	ERasterizerFillMode MeshFillMode : ERasterizerFillMode_NumBits + 1;
	ERasterizerCullMode MeshCullMode : ERasterizerCullMode_NumBits + 1;
};

template <>
struct TUseBitwiseSwap<FVisibleMeshDrawCommand>
{
	// Prevent Memcpy call overhead during FVisibleMeshDrawCommand sorting
	enum { Value = false };
};

typedef TArray<FVisibleMeshDrawCommand, SceneRenderingAllocator> FMeshCommandOneFrameArray;
typedef TMap<int32, FUniformBufferRHIRef, SceneRenderingSetAllocator> FTranslucentSelfShadowUniformBufferMap;

/** Context used when building FMeshDrawCommands for one frame only. */
class FDynamicPassMeshDrawListContext : public FMeshPassDrawListContext
{
public:
	FDynamicPassMeshDrawListContext
	(
		FDynamicMeshDrawCommandStorage& InDrawListStorage, 
		FMeshCommandOneFrameArray& InDrawList
	) :
		DrawListStorage(InDrawListStorage),
		DrawList(InDrawList)
	{}

	virtual FMeshDrawCommand& AddCommand(const FMeshDrawCommand& Initializer) override final
	{
		const int32 Index = DrawListStorage.MeshDrawCommands.AddElement(Initializer);
		FMeshDrawCommand& NewCommand = DrawListStorage.MeshDrawCommands[Index];
		return NewCommand;
	}

	virtual void FinalizeCommand(
		const FMeshBatch& MeshBatch, 
		int32 BatchElementIndex,
		int32 DrawPrimitiveId,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode,
		FMeshDrawCommandSortKey SortKey,
		const FGraphicsMinimalPipelineStateInitializer& PipelineState,
		const FMeshProcessorShaders* ShadersForDebugging,
		FMeshDrawCommand& MeshDrawCommand) override final
	{
		FGraphicsMinimalPipelineStateId PipelineId;
		PipelineId = FGraphicsMinimalPipelineStateId::GetOneFrameId(PipelineState);

		MeshDrawCommand.SetDrawParametersAndFinalize(MeshBatch, BatchElementIndex, PipelineId, ShadersForDebugging);

		FVisibleMeshDrawCommand NewVisibleMeshDrawCommand;
		//@todo MeshCommandPipeline - assign usable state ID for dynamic path draws
		// Currently dynamic path draws will not get dynamic instancing, but they will be roughly sorted by state
		NewVisibleMeshDrawCommand.Setup(&MeshDrawCommand, DrawPrimitiveId, -1, MeshFillMode, MeshCullMode, SortKey);
		DrawList.Add(NewVisibleMeshDrawCommand);
	}

private:
	FDynamicMeshDrawCommandStorage& DrawListStorage;
	FMeshCommandOneFrameArray& DrawList;
};

#if PLATFORM_SUPPORTS_PRAGMA_PACK
	#pragma pack (push,4)
#endif

/** 
 * Stores information about a mesh draw command which is cached in the scene. 
 * This is stored separately from the cached FMeshDrawCommand so that InitViews does not have to load the FMeshDrawCommand into cache.
 */
class FCachedMeshDrawCommandInfo
{
public:
	explicit FCachedMeshDrawCommandInfo() :
		SortKey(FMeshDrawCommandSortKey::Default),
		CommandIndex(-1),
		StateBucketId(-1),
		MeshPass(EMeshPass::Num),
		MeshFillMode(ERasterizerFillMode_Num),
		MeshCullMode(ERasterizerCullMode_Num)
	{}

	FMeshDrawCommandSortKey SortKey;

	// Stores the index into FScene::CachedDrawLists of the corresponding FMeshDrawCommand, or -1 if not stored there
	int32 CommandIndex;

	// Stores the index into FScene::CachedMeshDrawCommandStateBuckets of the corresponding FMeshDrawCommand, or -1 if not stored there
	int32 StateBucketId;

	// Needed for easier debugging and faster removal of cached mesh draw commands.
	EMeshPass::Type MeshPass : EMeshPass::NumBits + 1;

	// Needed for view overrides
	ERasterizerFillMode MeshFillMode : ERasterizerFillMode_NumBits + 1;
	ERasterizerCullMode MeshCullMode : ERasterizerCullMode_NumBits + 1;
};

#if PLATFORM_SUPPORTS_PRAGMA_PACK
	#pragma pack (pop)
#endif

class FCachedPassMeshDrawList
{
public:

	FCachedPassMeshDrawList() :
		LowestFreeIndexSearchStart(0)
	{}

	/** Indices held by FStaticMeshBatch::CachedMeshDrawCommands must be stable */
	TSparseArray<FMeshDrawCommand> MeshDrawCommands;
	int32 LowestFreeIndexSearchStart;
};

typedef TArray<int32, TInlineAllocator<5>> FDrawCommandIndices;

class FCachedPassMeshDrawListContext : public FMeshPassDrawListContext
{
public:
	FCachedPassMeshDrawListContext(FCachedMeshDrawCommandInfo& InCommandInfo, FCachedPassMeshDrawList& InDrawList, FScene& InScene);

	virtual FMeshDrawCommand& AddCommand(const FMeshDrawCommand& Initializer) override final;

	virtual void FinalizeCommand(
		const FMeshBatch& MeshBatch, 
		int32 BatchElementIndex,
		int32 DrawPrimitiveId,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode,
		FMeshDrawCommandSortKey SortKey,
		const FGraphicsMinimalPipelineStateInitializer& PipelineState,
		const FMeshProcessorShaders* ShadersForDebugging,
		FMeshDrawCommand& MeshDrawCommand) override final;

private:
	FMeshDrawCommand MeshDrawCommandForStateBucketing;
	FCachedMeshDrawCommandInfo& CommandInfo;
	FCachedPassMeshDrawList& DrawList;
	FScene& Scene;
	bool bUseStateBuckets;
};

template<typename VertexType, typename HullType, typename DomainType, typename PixelType, typename GeometryType = FMeshMaterialShader, typename RayHitGroupType = FMeshMaterialShader, typename ComputeType = FMeshMaterialShader>
struct TMeshProcessorShaders
{
	VertexType* VertexShader = nullptr;
	HullType* HullShader = nullptr;
	DomainType* DomainShader = nullptr;
	PixelType* PixelShader = nullptr;
	GeometryType* GeometryShader = nullptr;
	ComputeType* ComputeShader = nullptr;
#if RHI_RAYTRACING
	RayHitGroupType* RayHitGroupShader = nullptr;
#endif

	TMeshProcessorShaders() = default;

	FMeshProcessorShaders GetUntypedShaders()
	{
		FMeshProcessorShaders Shaders;
		Shaders.VertexShader = VertexShader;
		Shaders.HullShader = HullShader;
		Shaders.DomainShader = DomainShader;
		Shaders.PixelShader = PixelShader;
		Shaders.GeometryShader = GeometryShader;
		Shaders.ComputeShader = ComputeShader;
#if RHI_RAYTRACING
		Shaders.RayHitGroupShader = RayHitGroupShader;
#endif
		return Shaders;
	}
};

enum class EMeshPassFeatures
{
	Default = 0,
	PositionOnly = 1 << 0
};
ENUM_CLASS_FLAGS(EMeshPassFeatures);

/**
 * A set of render state overrides passed into a Mesh Pass Processor, so it can be configured from the outside.
 */
struct FMeshPassProcessorRenderState
{
	FMeshPassProcessorRenderState(const FSceneView& SceneView, FUniformBufferRHIParamRef InPassUniformBuffer = nullptr) : 
		  BlendState(nullptr)
		, DepthStencilState(nullptr)
		, DepthStencilAccess(FExclusiveDepthStencil::DepthRead_StencilRead)
		, ViewUniformBuffer(SceneView.ViewUniformBuffer)
		, InstancedViewUniformBuffer()
		, PassUniformBuffer(InPassUniformBuffer)
		, StencilRef(0)
	{
	}

	FMeshPassProcessorRenderState(const TUniformBufferRef<FViewUniformShaderParameters>& InViewUniformBuffer, FUniformBufferRHIParamRef InPassUniformBuffer) : 
		  BlendState(nullptr)
		, DepthStencilState(nullptr)
		, DepthStencilAccess(FExclusiveDepthStencil::DepthRead_StencilRead)
		, ViewUniformBuffer(InViewUniformBuffer)
		, InstancedViewUniformBuffer()
		, PassUniformBuffer(InPassUniformBuffer)
		, StencilRef(0)
	{
	}

	FMeshPassProcessorRenderState() :
		BlendState(nullptr)
		, DepthStencilState(nullptr)
		, ViewUniformBuffer()
		, InstancedViewUniformBuffer()
		, PassUniformBuffer(nullptr)
		, StencilRef(0)
	{
	}

	FORCEINLINE_DEBUGGABLE FMeshPassProcessorRenderState(const FMeshPassProcessorRenderState& DrawRenderState) :
		  BlendState(DrawRenderState.BlendState)
		, DepthStencilState(DrawRenderState.DepthStencilState)
		, DepthStencilAccess(DrawRenderState.DepthStencilAccess)
		, ViewUniformBuffer(DrawRenderState.ViewUniformBuffer)
		, InstancedViewUniformBuffer(DrawRenderState.InstancedViewUniformBuffer)
		, PassUniformBuffer(DrawRenderState.PassUniformBuffer)
		, StencilRef(DrawRenderState.StencilRef)
	{
	}

	~FMeshPassProcessorRenderState()
	{
	}

public:
	FORCEINLINE_DEBUGGABLE void SetBlendState(FBlendStateRHIParamRef InBlendState)
	{
		BlendState = InBlendState;
	}

	FORCEINLINE_DEBUGGABLE const FBlendStateRHIParamRef GetBlendState() const
	{
		return BlendState;
	}

	FORCEINLINE_DEBUGGABLE void SetDepthStencilState(FDepthStencilStateRHIParamRef InDepthStencilState)
	{
		DepthStencilState = InDepthStencilState;
		StencilRef = 0;
	}

	FORCEINLINE_DEBUGGABLE void SetStencilRef(uint32 InStencilRef)
		{
		StencilRef = InStencilRef;
	}

	FORCEINLINE_DEBUGGABLE const FDepthStencilStateRHIParamRef GetDepthStencilState() const
	{
		return DepthStencilState;
	}

	FORCEINLINE_DEBUGGABLE void SetDepthStencilAccess(FExclusiveDepthStencil::Type InDepthStencilAccess)
	{
		DepthStencilAccess = InDepthStencilAccess;
	}

	FORCEINLINE_DEBUGGABLE FExclusiveDepthStencil::Type GetDepthStencilAccess() const
	{
		return DepthStencilAccess;
	}

	FORCEINLINE_DEBUGGABLE void SetViewUniformBuffer(const TUniformBufferRef<FViewUniformShaderParameters>& InViewUniformBuffer)
	{
		ViewUniformBuffer = InViewUniformBuffer;
	}

	FORCEINLINE_DEBUGGABLE const TUniformBufferRef<FViewUniformShaderParameters>& GetViewUniformBuffer() const
	{
		return ViewUniformBuffer;
	}

	FORCEINLINE_DEBUGGABLE void SetInstancedViewUniformBuffer(const TUniformBufferRef<FInstancedViewUniformShaderParameters>& InViewUniformBuffer)
	{
		InstancedViewUniformBuffer = InViewUniformBuffer;
	}

	FORCEINLINE_DEBUGGABLE const TUniformBufferRef<FInstancedViewUniformShaderParameters>& GetInstancedViewUniformBuffer() const
	{
		return InstancedViewUniformBuffer.IsValid() ? InstancedViewUniformBuffer : reinterpret_cast<const TUniformBufferRef<FInstancedViewUniformShaderParameters>&>(ViewUniformBuffer);
	}

	FORCEINLINE_DEBUGGABLE void SetPassUniformBuffer(FUniformBufferRHIParamRef InPassUniformBuffer)
	{
		PassUniformBuffer = InPassUniformBuffer;
	}

	FORCEINLINE_DEBUGGABLE FUniformBufferRHIParamRef GetPassUniformBuffer() const
	{
		return PassUniformBuffer;
	}

	FORCEINLINE_DEBUGGABLE uint32 GetStencilRef() const
	{
		return StencilRef;
	}

	FORCEINLINE_DEBUGGABLE void ApplyToPSO(FGraphicsPipelineStateInitializer& GraphicsPSOInit) const
	{
		GraphicsPSOInit.BlendState = BlendState;
		GraphicsPSOInit.DepthStencilState = DepthStencilState;
	}

private:
	FBlendStateRHIParamRef			BlendState;
	FDepthStencilStateRHIParamRef	DepthStencilState;
	FExclusiveDepthStencil::Type	DepthStencilAccess;

	TUniformBufferRef<FViewUniformShaderParameters>	ViewUniformBuffer;
	TUniformBufferRef<FInstancedViewUniformShaderParameters> InstancedViewUniformBuffer;
	FUniformBufferRHIParamRef		PassUniformBuffer;
	uint32							StencilRef;
};

/** 
 * Base class of mesh processors, whose job is to transform FMeshBatch draw descriptions received from scene proxy implementations into FMeshDrawCommands ready for the RHI command list
 */
class FMeshPassProcessor
{
public:

	const FScene* RESTRICT Scene;
	ERHIFeatureLevel::Type FeatureLevel;
	const FSceneView* ViewIfDynamicMeshCommand;
	FMeshPassDrawListContext* DrawListContext;

	RENDERER_API FMeshPassProcessor(const FScene* InScene, ERHIFeatureLevel::Type InFeatureLevel, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext);

	virtual ~FMeshPassProcessor() {}

	void SetDrawListContext(FMeshPassDrawListContext* InDrawListContext)
	{
		DrawListContext = InDrawListContext;
	}

	// FMeshPassProcessor interface
	// Add a FMeshBatch to the pass
	virtual void AddMeshBatch(const FMeshBatch& RESTRICT MeshBatch, uint64 BatchElementMask, const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy, int32 StaticMeshId = -1) = 0;

	static FORCEINLINE_DEBUGGABLE ERasterizerCullMode InverseCullMode(ERasterizerCullMode CullMode)
	{
		return CullMode == CM_None ? CM_None : (CullMode == CM_CCW ? CM_CW : CM_CCW);
	}

	RENDERER_API ERasterizerFillMode ComputeMeshFillMode(const FMeshBatch& Mesh, const FMaterial& InMaterialResource) const;
	RENDERER_API ERasterizerCullMode ComputeMeshCullMode(const FMeshBatch& Mesh, const FMaterial& InMaterialResource) const;

	template<typename PassShadersType, typename ShaderElementDataType>
	void BuildMeshDrawCommands(
		const FMeshBatch& RESTRICT MeshBatch,
		uint64 BatchElementMask,
		const FPrimitiveSceneProxy* RESTRICT PrimitiveSceneProxy,
		const FMaterialRenderProxy& RESTRICT MaterialRenderProxy,
		const FMaterial& RESTRICT MaterialResource,
		const FMeshPassProcessorRenderState& RESTRICT DrawRenderState,
		PassShadersType PassShaders,
		ERasterizerFillMode MeshFillMode,
		ERasterizerCullMode MeshCullMode,
		FMeshDrawCommandSortKey SortKey,
		EMeshPassFeatures MeshPassFeatures,
		const ShaderElementDataType& ShaderElementData);

private:
	RENDERER_API int32 GetDrawCommandPrimitiveId(const FPrimitiveSceneInfo* RESTRICT PrimitiveSceneInfo, const FMeshBatchElement& BatchElement) const;
};

typedef FMeshPassProcessor* (*PassProcessorCreateFunction)(const FScene* Scene, const FSceneView* InViewIfDynamicMeshCommand, FMeshPassDrawListContext* InDrawListContext);

enum class EMeshPassFlags
{
	None = 0,
	CachedMeshCommands = 1 << 0,
	MainView = 1 << 1
};
ENUM_CLASS_FLAGS(EMeshPassFlags);

class FPassProcessorManager
{
public:
	static PassProcessorCreateFunction GetCreateFunction(EShadingPath ShadingPath, EMeshPass::Type PassType)
	{
		uint32 ShadingPathIdx = (uint32)ShadingPath;
		checkf(JumpTable[ShadingPathIdx][PassType], TEXT("Pass type %u create function was never registered for shading path %u.  Use a FRegisterPassProcessorCreateFunction to register a create function for this enum value."), (uint32)PassType, ShadingPathIdx);
		return JumpTable[ShadingPathIdx][PassType];
	}

	static EMeshPassFlags GetPassFlags(EShadingPath ShadingPath, EMeshPass::Type PassType)
	{
		uint32 ShadingPathIdx = (uint32)ShadingPath;
		return Flags[ShadingPathIdx][PassType];
	}

private:
	RENDERER_API static PassProcessorCreateFunction JumpTable[(uint32)EShadingPath::Num][EMeshPass::Num];
	RENDERER_API static EMeshPassFlags Flags[(uint32)EShadingPath::Num][EMeshPass::Num];
	friend class FRegisterPassProcessorCreateFunction;
};

class FRegisterPassProcessorCreateFunction
{
public:
	FRegisterPassProcessorCreateFunction(PassProcessorCreateFunction CreateFunction, EShadingPath InShadingPath, EMeshPass::Type InPassType, EMeshPassFlags PassFlags) 
		: ShadingPath(InShadingPath)
		, PassType(InPassType)
	{
		uint32 ShadingPathIdx = (uint32)ShadingPath;
		FPassProcessorManager::JumpTable[ShadingPathIdx][PassType] = CreateFunction;
		FPassProcessorManager::Flags[ShadingPathIdx][PassType] = PassFlags;
	}

	~FRegisterPassProcessorCreateFunction()
	{
		uint32 ShadingPathIdx = (uint32)ShadingPath;
		FPassProcessorManager::JumpTable[ShadingPathIdx][PassType] = nullptr;
		FPassProcessorManager::Flags[ShadingPathIdx][PassType] = EMeshPassFlags::None;
	}

private:
	EShadingPath ShadingPath;
	EMeshPass::Type PassType;
};

extern void SubmitMeshDrawCommands(
	const FMeshCommandOneFrameArray& VisibleMeshDrawCommands,
	FVertexBufferRHIParamRef PrimitiveIdsBuffer,
	int32 BasePrimitiveIdsOffset,
	bool bDynamicInstancing,
	uint32 InstanceFactor,
	FRHICommandList& RHICmdList);

extern void SubmitMeshDrawCommandsRange(
	const FMeshCommandOneFrameArray& VisibleMeshDrawCommands,
	FVertexBufferRHIParamRef PrimitiveIdsBuffer,
	int32 BasePrimitiveIdsOffset,
	bool bDynamicInstancing,
	int32 StartIndex,
	int32 NumMeshDrawCommands,
	uint32 InstanceFactor,
	FRHICommandList& RHICmdList);

RENDERER_API extern void DrawDynamicMeshPassPrivate(
	const FSceneView& View,
	FRHICommandList& RHICmdList,
	FMeshCommandOneFrameArray& VisibleMeshDrawCommands,
	FDynamicMeshDrawCommandStorage& DynamicMeshDrawCommandStorage,
	uint32 InstanceFactor);

RENDERER_API extern FMeshDrawCommandSortKey CalculateMeshStaticSortKey(const FMeshMaterialShader* VertexShader, const FMeshMaterialShader* PixelShader);

#if RHI_RAYTRACING
class FRayTracingMeshCommand
{
public:
	FMeshDrawShaderBindings ShaderBindings;

	uint32 MaterialShaderIndex = UINT_MAX;

	uint8 GeometrySegmentIndex = 0xFF;
	uint8 InstanceMask = 0xFF;

	bool bCastRayTracedShadows = true;
	bool bOpaque = true;

	/** Sets ray hit group shaders on the mesh command and allocates room for the shader bindings. */
	RENDERER_API void SetShaders(const FMeshProcessorShaders& Shaders);
};

class FVisibleRayTracingMeshCommand
{
public:
	const FRayTracingMeshCommand* RayTracingMeshCommand;

	uint32 InstanceIndex;
};

template <>
struct TUseBitwiseSwap<FVisibleRayTracingMeshCommand>
{
	// Prevent Memcpy call overhead during FVisibleRayTracingMeshCommand sorting
	enum { Value = false };
};

typedef TArray<FVisibleRayTracingMeshCommand, SceneRenderingAllocator> FRayTracingMeshCommandOneFrameArray;

class FRayTracingMeshCommandContext
{
public:

	virtual ~FRayTracingMeshCommandContext() {}

	virtual FRayTracingMeshCommand& AddCommand(const FRayTracingMeshCommand& Initializer) = 0;

	virtual void FinalizeCommand(FRayTracingMeshCommand& RayTracingMeshCommand) = 0;
};

struct FCachedRayTracingMeshCommandStorage
{
	TSparseArray<FRayTracingMeshCommand> RayTracingMeshCommands;
};

struct FDynamicRayTracingMeshCommandStorage
{
	TChunkedArray<FRayTracingMeshCommand> RayTracingMeshCommands;
};

class FCachedRayTracingMeshCommandContext : public FRayTracingMeshCommandContext
{
public:
	FCachedRayTracingMeshCommandContext(FCachedRayTracingMeshCommandStorage& InDrawListStorage) : DrawListStorage(InDrawListStorage) {}

	virtual FRayTracingMeshCommand& AddCommand(const FRayTracingMeshCommand& Initializer) override final
	{
		CommandIndex = DrawListStorage.RayTracingMeshCommands.Add(Initializer);
		return DrawListStorage.RayTracingMeshCommands[CommandIndex];
	}

	virtual void FinalizeCommand(FRayTracingMeshCommand& RayTracingMeshCommand) override final {}

	int32 CommandIndex = -1;

private:
	FCachedRayTracingMeshCommandStorage& DrawListStorage;
};

class FDynamicRayTracingMeshCommandContext : public FRayTracingMeshCommandContext
{
public:
	FDynamicRayTracingMeshCommandContext
	(
		FDynamicRayTracingMeshCommandStorage& InDynamicCommandStorage,
		FRayTracingMeshCommandOneFrameArray& InVisibleCommands,
		uint8 InGeometrySegmentIndex = 0xFF,
		uint32 InRayTracingInstanceIndex = ~0u
	) :
		DynamicCommandStorage(InDynamicCommandStorage),
		VisibleCommands(InVisibleCommands),
		GeometrySegmentIndex(InGeometrySegmentIndex),
		RayTracingInstanceIndex(InRayTracingInstanceIndex)
	{}

	virtual FRayTracingMeshCommand& AddCommand(const FRayTracingMeshCommand& Initializer) override final
	{
		const int32 Index = DynamicCommandStorage.RayTracingMeshCommands.AddElement(Initializer);
		FRayTracingMeshCommand& NewCommand = DynamicCommandStorage.RayTracingMeshCommands[Index];
		NewCommand.GeometrySegmentIndex = GeometrySegmentIndex;
		return NewCommand;
	}

	virtual void FinalizeCommand(FRayTracingMeshCommand& RayTracingMeshCommand) override final
	{
		FVisibleRayTracingMeshCommand NewVisibleMeshCommand;
		NewVisibleMeshCommand.RayTracingMeshCommand = &RayTracingMeshCommand;
		NewVisibleMeshCommand.InstanceIndex = RayTracingInstanceIndex;
		VisibleCommands.Add(NewVisibleMeshCommand);
	}

private:
	FDynamicRayTracingMeshCommandStorage& DynamicCommandStorage;
	FRayTracingMeshCommandOneFrameArray& VisibleCommands;
	uint8 GeometrySegmentIndex;
	uint32 RayTracingInstanceIndex;
};

#endif
