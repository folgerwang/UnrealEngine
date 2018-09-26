// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 Pipelinestate related functions

#pragma once

#define D3D12_USE_DERIVED_PSO PLATFORM_XBOXONE

static bool GCPUSupportsSSE4;

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Graphics: Num high-level cache entries"), STAT_PSOGraphicsNumHighlevelCacheEntries, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Graphics: Num low-level cache entries"), STAT_PSOGraphicsNumLowlevelCacheEntries, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Graphics: Low-level cache hit"), STAT_PSOGraphicsLowlevelCacheHit, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Graphics: Low-level cache miss"), STAT_PSOGraphicsLowlevelCacheMiss, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Graphics: High-level cache hit"), STAT_PSOGraphicsHighlevelCacheHit, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Graphics: High-level cache miss"), STAT_PSOGraphicsHighlevelCacheMiss, STATGROUP_D3D12PipelineState);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Compute: Num high-level cache entries"), STAT_PSOComputeNumHighlevelCacheEntries, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Compute: Num low-level cache entries"), STAT_PSOComputeNumLowlevelCacheEntries, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Compute: Low-level cache hit"), STAT_PSOComputeLowlevelCacheHit, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Compute: Low-level cache miss"), STAT_PSOComputeLowlevelCacheMiss, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Compute: High-level cache hit"), STAT_PSOComputeHighlevelCacheHit, STATGROUP_D3D12PipelineState);
DECLARE_DWORD_COUNTER_STAT(TEXT("Compute: High-level cache miss"), STAT_PSOComputeHighlevelCacheMiss, STATGROUP_D3D12PipelineState);


// Graphics pipeline struct that represents the latest versions of PSO subobjects currently supported by the RHI.
struct FD3D12_GRAPHICS_PIPELINE_STATE_DESC
{
	ID3D12RootSignature *pRootSignature;
	D3D12_SHADER_BYTECODE VS;
	D3D12_SHADER_BYTECODE PS;
	D3D12_SHADER_BYTECODE DS;
	D3D12_SHADER_BYTECODE HS;
	D3D12_SHADER_BYTECODE GS;
	D3D12_STREAM_OUTPUT_DESC StreamOutput;
#if !D3D12_USE_DERIVED_PSO
	D3D12_BLEND_DESC BlendState;
	uint32 SampleMask;
	D3D12_RASTERIZER_DESC RasterizerState;
	D3D12_DEPTH_STENCIL_DESC1 DepthStencilState;
#endif // !D3D12_USE_DERIVED_PSO
	D3D12_INPUT_LAYOUT_DESC InputLayout;
	D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
	D3D12_RT_FORMAT_ARRAY RTFormatArray;
	DXGI_FORMAT DSVFormat;
	DXGI_SAMPLE_DESC SampleDesc;
	uint32 NodeMask;
	D3D12_CACHED_PIPELINE_STATE CachedPSO;
	D3D12_PIPELINE_STATE_FLAGS Flags;

#if PLATFORM_WINDOWS
	FD3D12_GRAPHICS_PIPELINE_STATE_STREAM PipelineStateStream() const;
	D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicsDescV0() const;
#endif // PLATFORM_WINDOWS
};

struct FD3D12LowLevelGraphicsPipelineStateDesc
{
	const FD3D12RootSignature *pRootSignature;
	FD3D12_GRAPHICS_PIPELINE_STATE_DESC Desc;
	ShaderBytecodeHash VSHash;
	ShaderBytecodeHash HSHash;
	ShaderBytecodeHash DSHash;
	ShaderBytecodeHash GSHash;
	ShaderBytecodeHash PSHash;

	SIZE_T CombinedHash;

	FORCEINLINE FString GetName() const { return FString::Printf(TEXT("%llu"), CombinedHash); }

#if PLATFORM_XBOXONE
	void Destroy();
#endif
};

// Compute pipeline struct that represents the latest versions of PSO subobjects currently supported by the RHI.
struct FD3D12_COMPUTE_PIPELINE_STATE_DESC : public D3D12_COMPUTE_PIPELINE_STATE_DESC
{
#if PLATFORM_WINDOWS
	FD3D12_COMPUTE_PIPELINE_STATE_STREAM PipelineStateStream() const;
	D3D12_COMPUTE_PIPELINE_STATE_DESC ComputeDescV0() const;
#endif
};

struct FD3D12ComputePipelineStateDesc
{
	const FD3D12RootSignature* pRootSignature;
	FD3D12_COMPUTE_PIPELINE_STATE_DESC Desc;
	ShaderBytecodeHash CSHash;

	SIZE_T CombinedHash;

	FORCEINLINE FString GetName() const { return FString::Printf(TEXT("%llu"), CombinedHash); }

#if PLATFORM_XBOXONE
	void Destroy();
#endif
};


FD3D12LowLevelGraphicsPipelineStateDesc GetLowLevelGraphicsPipelineStateDesc(const FGraphicsPipelineStateInitializer& Initializer, FD3D12BoundShaderState* BoundShaderState);
FD3D12ComputePipelineStateDesc GetComputePipelineStateDesc(const FD3D12ComputeShader* ComputeShader);

#define PSO_IF_NOT_EQUAL_RETURN_FALSE( value ) if(lhs.##value != rhs.##value){ return false; }

#define PSO_IF_MEMCMP_FAILS_RETURN_FALSE( value ) if(FMemory::Memcmp(&lhs.##value, &rhs.##value, sizeof(rhs.##value)) != 0){ return false; }

#define PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE( value ) \
	const char* const lhString = lhs.##value##; \
	const char* const rhString = rhs.##value##; \
	if (lhString != rhString) \
	{ \
		if (strcmp(lhString, rhString) != 0) \
		{ \
			return false; \
		} \
	}

template <typename TDesc> struct equality_pipeline_state_desc;

template <> struct equality_pipeline_state_desc<FD3D12LowLevelGraphicsPipelineStateDesc>
{
	bool operator()(const FD3D12LowLevelGraphicsPipelineStateDesc& lhs, const FD3D12LowLevelGraphicsPipelineStateDesc& rhs)
	{
		// Order from most likely to change to least
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.PS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.VS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.GS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.DS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.HS.BytecodeLength)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.NumElements)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.RTFormatArray.NumRenderTargets)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.DSVFormat)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.PrimitiveTopologyType)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.Flags)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.pRootSignature)
#if !D3D12_USE_DERIVED_PSO
		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.BlendState)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleMask)
		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.RasterizerState)
		PSO_IF_MEMCMP_FAILS_RETURN_FALSE(Desc.DepthStencilState)
#endif
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.IBStripCutValue)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.NodeMask)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.RasterizedStream)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.NumEntries)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.NumStrides)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleDesc.Count)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.SampleDesc.Quality)

		for (uint32 i = 0; i < lhs.Desc.RTFormatArray.NumRenderTargets; i++)
		{
			PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.RTFormatArray.RTFormats[i]);
		}

		// Shader byte code is hashed with SHA1 (160 bit) so the chances of collision
		// should be tiny i.e if there were 1 quadrillion shaders the chance of a 
		// collision is ~ 1 in 10^18. so only do a full check on debug builds
		PSO_IF_NOT_EQUAL_RETURN_FALSE(VSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(PSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(GSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(HSHash)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(DSHash)

		if (lhs.Desc.StreamOutput.pSODeclaration != rhs.Desc.StreamOutput.pSODeclaration &&
			lhs.Desc.StreamOutput.NumEntries)
		{
			for (uint32 i = 0; i < lhs.Desc.StreamOutput.NumEntries; i++)
			{
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].Stream)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].SemanticIndex)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].StartComponent)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].ComponentCount)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].OutputSlot)
				PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE(Desc.StreamOutput.pSODeclaration[i].SemanticName)
			}
		}

		if (lhs.Desc.StreamOutput.pBufferStrides != rhs.Desc.StreamOutput.pBufferStrides &&
			lhs.Desc.StreamOutput.NumStrides)
		{
			for (uint32 i = 0; i < lhs.Desc.StreamOutput.NumStrides; i++)
			{
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.StreamOutput.pBufferStrides[i])
			}
		}

		if (lhs.Desc.InputLayout.pInputElementDescs != rhs.Desc.InputLayout.pInputElementDescs &&
			lhs.Desc.InputLayout.NumElements)
		{
			for (uint32 i = 0; i < lhs.Desc.InputLayout.NumElements; i++)
			{
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].SemanticIndex)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].Format)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InputSlot)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].AlignedByteOffset)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InputSlotClass)
				PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].InstanceDataStepRate)
				PSO_IF_STRING_COMPARE_FAILS_RETURN_FALSE(Desc.InputLayout.pInputElementDescs[i].SemanticName)
			}
		}
		return true;
	}
};

template <> struct equality_pipeline_state_desc<FD3D12ComputePipelineStateDesc>
{
	bool operator()(const FD3D12ComputePipelineStateDesc& lhs, const FD3D12ComputePipelineStateDesc& rhs)
	{
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.CS.BytecodeLength)
#if PLATFORM_WINDOWS
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.Flags)
#endif
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.pRootSignature)
		PSO_IF_NOT_EQUAL_RETURN_FALSE(Desc.NodeMask)

		// Shader byte code is hashed with SHA1 (160 bit) so the chances of collision
		// should be tiny i.e if there were 1 quadrillion shaders the chance of a 
		// collision is ~ 1 in 10^18. so only do a full check on debug builds
		PSO_IF_NOT_EQUAL_RETURN_FALSE(CSHash)

#if UE_BUILD_DEBUG
		if (lhs.Desc.CS.pShaderBytecode != rhs.Desc.CS.pShaderBytecode &&
			lhs.Desc.CS.pShaderBytecode != nullptr &&
			lhs.Desc.CS.BytecodeLength)
		{
			if (FMemory::Memcmp(lhs.Desc.CS.pShaderBytecode, rhs.Desc.CS.pShaderBytecode, lhs.Desc.CS.BytecodeLength) != 0)
			{
				return false;
			}
		}
#endif

		return true;
	}
};

struct ComputePipelineCreationArgs;
struct GraphicsPipelineCreationArgs;

struct FD3D12PipelineStateWorker : public FD3D12AdapterChild, public FNonAbandonableTask
{
	FD3D12PipelineStateWorker(FD3D12Adapter* Adapter, const ComputePipelineCreationArgs& InArgs);
	FD3D12PipelineStateWorker(FD3D12Adapter* Adapter, const GraphicsPipelineCreationArgs& InArgs);

	void DoWork();

	FORCEINLINE TStatId GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(FD3D12PipelineStateWorker, STATGROUP_ThreadPoolAsyncTasks); }

	union PipelineCreationArgs
	{
		ComputePipelineCreationArgs_POD ComputeArgs;
		GraphicsPipelineCreationArgs_POD GraphicsArgs;
	} CreationArgs;

	const bool bIsGraphics;
	TRefCountPtr<ID3D12PipelineState> PSO;
};

struct FD3D12PipelineState : public FD3D12AdapterChild, public FD3D12MultiNodeGPUObject, public FNoncopyable
{
public:
	explicit FD3D12PipelineState(FD3D12Adapter* Parent);
	~FD3D12PipelineState();

	void Create(const ComputePipelineCreationArgs& InCreationArgs);
	void CreateAsync(const ComputePipelineCreationArgs& InCreationArgs);

#if PLATFORM_XBOXONE
	void Create(const FGraphicsPipelineStateInitializer& Initializer, FD3D12PipelineState* BasePSO);
#endif

	void Create(const GraphicsPipelineCreationArgs& InCreationArgs);
	void CreateAsync(const GraphicsPipelineCreationArgs& InCreationArgs);

	ID3D12PipelineState* GetPipelineState();

	FD3D12PipelineState& operator=(const FD3D12PipelineState& other)
	{
		checkSlow(GPUMask == other.GPUMask);
		checkSlow(VisibilityMask == other.VisibilityMask);
		ensure(PendingWaitOnWorkerCalls == 0);

		PipelineState = other.PipelineState;
		Worker = other.Worker;
		return *this;
	}

	// Indicates this PSO should be added to any disk caches.
	void MarkForDiskCacheAdd()
	{
		bAddToDiskCache = PipelineState.GetReference() ? true : false;
	}

	bool ShouldAddToDiskCache() const
	{
		return bAddToDiskCache;
	}

protected:
	TRefCountPtr<ID3D12PipelineState> PipelineState;
	FAsyncTask<FD3D12PipelineStateWorker>* Worker;
	volatile int32 PendingWaitOnWorkerCalls;
	bool bAddToDiskCache;
};

struct FD3D12GraphicsPipelineState : public FRHIGraphicsPipelineState
{
	explicit FD3D12GraphicsPipelineState(const FGraphicsPipelineStateInitializer& Initializer, FD3D12BoundShaderState* InBoundShaderState, FD3D12PipelineState* InPipelineState);
	~FD3D12GraphicsPipelineState();

	const FGraphicsPipelineStateInitializer PipelineStateInitializer;
	const FD3D12RootSignature* RootSignature;
	uint16 StreamStrides[MaxVertexElementCount];
	bool bShaderNeedsGlobalConstantBuffer[SF_NumFrequencies];

	FD3D12PipelineState* PipelineState;

	FORCEINLINE class FD3D12VertexShader*   GetVertexShader() const { return (FD3D12VertexShader*)PipelineStateInitializer.BoundShaderState.VertexShaderRHI; }
	FORCEINLINE class FD3D12PixelShader*    GetPixelShader() const { return (FD3D12PixelShader*)PipelineStateInitializer.BoundShaderState.PixelShaderRHI; }
	FORCEINLINE class FD3D12HullShader*     GetHullShader() const { return (FD3D12HullShader*)PipelineStateInitializer.BoundShaderState.HullShaderRHI; }
	FORCEINLINE class FD3D12DomainShader*   GetDomainShader() const { return (FD3D12DomainShader*)PipelineStateInitializer.BoundShaderState.DomainShaderRHI; }
	FORCEINLINE class FD3D12GeometryShader* GetGeometryShader() const { return (FD3D12GeometryShader*)PipelineStateInitializer.BoundShaderState.GeometryShaderRHI; }
};

struct FD3D12ComputePipelineState : public FRHIComputePipelineState
{
	explicit FD3D12ComputePipelineState(
		FD3D12ComputeShader* InComputeShader,
		FD3D12PipelineState* InPipelineState)
		: ComputeShader(InComputeShader)
		, PipelineState(InPipelineState)
	{
	}

	~FD3D12ComputePipelineState();

	TRefCountPtr<FD3D12ComputeShader> ComputeShader;
	FD3D12PipelineState* const PipelineState;
};

struct FInitializerToGPSOMapKey
{
	const FGraphicsPipelineStateInitializer* Initializer;
	uint32 Hash;

	FInitializerToGPSOMapKey() = default;

	FInitializerToGPSOMapKey(const FGraphicsPipelineStateInitializer* InInitializer, uint32 InHash) :
		Initializer(InInitializer),
		Hash(InHash)
	{}

	inline bool operator==(const FInitializerToGPSOMapKey& Other) const
	{
		return *Initializer == *Other.Initializer;
	}
};

inline uint32 GetTypeHash(const FInitializerToGPSOMapKey& Key)
{
	return Key.Hash;
}

class FD3D12PipelineStateCacheBase : public FD3D12AdapterChild
{
protected:
	enum PSO_CACHE_TYPE
	{
		PSO_CACHE_GRAPHICS,
		PSO_CACHE_COMPUTE,
		NUM_PSO_CACHE_TYPES
	};

	template <typename TDesc, typename TValue>
	struct TStateCacheKeyFuncs : BaseKeyFuncs<TPair<TDesc, TValue>, TDesc, false>
	{
		typedef typename TTypeTraits<TDesc>::ConstPointerType KeyInitType;
		typedef const TPairInitializer<typename TTypeTraits<TDesc>::ConstInitType, typename TTypeTraits<TValue>::ConstInitType>& ElementInitType;

		static FORCEINLINE KeyInitType GetSetKey(ElementInitType Element)
		{
			return Element.Key;
		}
		static FORCEINLINE bool Matches(KeyInitType A, KeyInitType B)
		{
			equality_pipeline_state_desc<TDesc> equal;
			return equal(A, B);
		}
		static FORCEINLINE uint32 GetKeyHash(KeyInitType Key)
		{
			return Key.CombinedHash;
		}
	};

	template <typename TDesc, typename TValue = FD3D12PipelineState*>
	using TPipelineCache = TMap<TDesc, TValue, FDefaultSetAllocator, TStateCacheKeyFuncs<TDesc, TValue>>;

	TMap<FInitializerToGPSOMapKey, FD3D12GraphicsPipelineState*> InitializerToGraphicsPipelineMap;
	TMap<FD3D12ComputeShader*, FD3D12ComputePipelineState*> ComputeShaderToComputePipelineMap;

	TPipelineCache<FD3D12LowLevelGraphicsPipelineStateDesc> LowLevelGraphicsPipelineStateCache;
	TPipelineCache<FD3D12ComputePipelineStateDesc> ComputePipelineStateCache;

	// Thread access mutual exclusion
	mutable FRWLock InitializerToGraphicsPipelineMapMutex;
	mutable FRWLock LowLevelGraphicsPipelineStateCacheMutex;
	mutable FRWLock ComputeShaderToComputePipelineMapMutex;
	mutable FRWLock ComputePipelineStateCacheMutex;

	FCriticalSection DiskCachesCS;

#if !PLATFORM_WINDOWS
	FRWLock CS;
#endif

	FDiskCacheInterface DiskCaches[NUM_PSO_CACHE_TYPES];

	void CleanupPipelineStateCaches();

	typedef TFunction<void(FD3D12PipelineState**, const FD3D12LowLevelGraphicsPipelineStateDesc&)> FPostCreateGraphicCallback;
	typedef TFunction<void(FD3D12PipelineState*, const FD3D12ComputePipelineStateDesc&)> FPostCreateComputeCallback;

	virtual FD3D12GraphicsPipelineState* AddToRuntimeCache(const FGraphicsPipelineStateInitializer& Initializer, uint32 InitializerHash, FD3D12BoundShaderState* BoundShaderState, FD3D12PipelineState* PipelineState);
	FD3D12PipelineState* FindInLowLevelCache(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc);
	FD3D12PipelineState* CreateAndAddToLowLevelCache(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc);
	void AddToLowLevelCache(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc, FD3D12PipelineState** OutPipelineState, const FPostCreateGraphicCallback& PostCreateCallback);
	virtual void OnPSOCreated(FD3D12PipelineState* PipelineState, const FD3D12LowLevelGraphicsPipelineStateDesc& Desc) = 0;

	FD3D12ComputePipelineState* AddToRuntimeCache(FD3D12ComputeShader* ComputeShader, FD3D12PipelineState* PipelineState);
	FD3D12PipelineState* FindInLowLevelCache(const FD3D12ComputePipelineStateDesc& Desc);
	FD3D12PipelineState* CreateAndAddToLowLevelCache(const FD3D12ComputePipelineStateDesc& Desc);
	void AddToLowLevelCache(const FD3D12ComputePipelineStateDesc& Desc, FD3D12PipelineState** OutPipelineState, const FPostCreateComputeCallback& PostCreateCallback);
	virtual void OnPSOCreated(FD3D12PipelineState* PipelineState, const FD3D12ComputePipelineStateDesc& Desc) = 0;

public:

	FD3D12GraphicsPipelineState* FindInRuntimeCache(const FGraphicsPipelineStateInitializer& Initializer, uint32& OutHash);
	FD3D12GraphicsPipelineState* FindInLoadedCache(const FGraphicsPipelineStateInitializer& Initializer, uint32 InitializerHash, FD3D12BoundShaderState* BoundShaderState, FD3D12LowLevelGraphicsPipelineStateDesc& OutLowLevelDesc);
	FD3D12GraphicsPipelineState* CreateAndAdd(const FGraphicsPipelineStateInitializer& Initializer, uint32 InitializerHash, FD3D12BoundShaderState* BoundShaderState, const FD3D12LowLevelGraphicsPipelineStateDesc& LowLevelDesc);

	FD3D12ComputePipelineState* FindInRuntimeCache(const FD3D12ComputeShader* ComputeShader);
	FD3D12ComputePipelineState* FindInLoadedCache(FD3D12ComputeShader* ComputeShader, FD3D12ComputePipelineStateDesc& OutLowLevelDesc);
	FD3D12ComputePipelineState* CreateAndAdd(FD3D12ComputeShader* ComputeShader, const FD3D12ComputePipelineStateDesc& LowLevelDesc);

	static SIZE_T HashPSODesc(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc);
	static SIZE_T HashPSODesc(const FD3D12ComputePipelineStateDesc& Desc);

	static uint32 HashData(const void* Data, SIZE_T NumBytes);

	FD3D12PipelineStateCacheBase(FD3D12Adapter* InParent);
	virtual ~FD3D12PipelineStateCacheBase();
};
