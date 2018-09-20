// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

// Implementation of D3D12 Pipelinestate related functions

//-----------------------------------------------------------------------------
//	Include Files
//-----------------------------------------------------------------------------
#include "D3D12RHIPrivate.h"


static TAutoConsoleVariable<float> CVarPSOStallWarningThresholdInMs(
	TEXT("D3D12.PSO.StallWarningThresholdInMs"),
	.5f,
	TEXT("Sets a threshold of when to logs messages about stalls due to PSO creation.\n")
	TEXT("Value is in milliseconds. (.5 is the default)\n"),
	ECVF_ReadOnly);

static TAutoConsoleVariable<float> CVarPSOStallTimeoutInMs(
	TEXT("D3D12.PSO.StallTimeoutInMs"),
	2000.0f,
	TEXT("The timeout interval. If a nonzero value is specified, the function waits until the PSO is created or the interval elapses.\n")
	TEXT("Value is in milliseconds. (2000.0 is the default)\n"),
	ECVF_ReadOnly);

/// @cond DOXYGEN_WARNINGS

FD3D12LowLevelGraphicsPipelineStateDesc GetLowLevelGraphicsPipelineStateDesc(const FGraphicsPipelineStateInitializer& Initializer, FD3D12BoundShaderState* BoundShaderState)
{
	// Memzero because we hash using the entire struct and we need to clear any padding.
	FD3D12LowLevelGraphicsPipelineStateDesc Desc;
	FMemory::Memzero(&Desc, sizeof(Desc));

	Desc.pRootSignature = BoundShaderState->pRootSignature;
	Desc.Desc.pRootSignature = Desc.pRootSignature->GetRootSignature();

#if !D3D12_USE_DERIVED_PSO
	Desc.Desc.BlendState = Initializer.BlendState ? FD3D12DynamicRHI::ResourceCast(Initializer.BlendState)->Desc : CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	Desc.Desc.SampleMask = 0xFFFFFFFF;
	Desc.Desc.RasterizerState = Initializer.RasterizerState ? FD3D12DynamicRHI::ResourceCast(Initializer.RasterizerState)->Desc : CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	Desc.Desc.DepthStencilState = Initializer.DepthStencilState ? CD3DX12_DEPTH_STENCIL_DESC1(FD3D12DynamicRHI::ResourceCast(Initializer.DepthStencilState)->Desc) : CD3DX12_DEPTH_STENCIL_DESC1(D3D12_DEFAULT);
#endif // !D3D12_USE_DERIVED_PSO

	Desc.Desc.PrimitiveTopologyType = D3D12PrimitiveTypeToTopologyType(TranslatePrimitiveType(Initializer.PrimitiveType));

	TranslateRenderTargetFormats(Initializer, Desc.Desc.RTFormatArray, Desc.Desc.DSVFormat);

	Desc.Desc.SampleDesc.Count = Initializer.NumSamples;
	Desc.Desc.SampleDesc.Quality = GetMaxMSAAQuality(Initializer.NumSamples);

	Desc.Desc.InputLayout = BoundShaderState->InputLayout;

	if (BoundShaderState->GetGeometryShader())
	{
		Desc.Desc.StreamOutput = BoundShaderState->GetGeometryShader()->StreamOutput;
	}

#define COPY_SHADER(Initial, Name) \
	if (FD3D12##Name##Shader* Shader = BoundShaderState->Get##Name##Shader()) \
	{ \
		Desc.Desc.Initial##S = Shader->ShaderBytecode.GetShaderBytecode(); \
		Desc.Initial##SHash = Shader->ShaderBytecode.GetHash(); \
	}
	COPY_SHADER(V, Vertex);
	COPY_SHADER(P, Pixel);
	COPY_SHADER(D, Domain);
	COPY_SHADER(H, Hull);
	COPY_SHADER(G, Geometry);
#undef COPY_SHADER

#if PLATFORM_WINDOWS
	// TODO: [PSO API] For now, keep DBT enabled, if available, until it is added as part of a member to the Initializer's DepthStencilState
	Desc.Desc.DepthStencilState.DepthBoundsTestEnable = GSupportsDepthBoundsTest && Initializer.bDepthBounds;
#endif

	return Desc;
}

FD3D12ComputePipelineStateDesc GetComputePipelineStateDesc(const FD3D12ComputeShader* ComputeShader)
{
	// Memzero because we hash using the entire struct and we need to clear any padding.
	FD3D12ComputePipelineStateDesc Desc;
	FMemory::Memzero(&Desc, sizeof(Desc));

	Desc.pRootSignature = ComputeShader->pRootSignature;
	Desc.Desc.pRootSignature = Desc.pRootSignature->GetRootSignature();
	Desc.Desc.CS = ComputeShader->ShaderBytecode.GetShaderBytecode();
	Desc.CSHash = ComputeShader->ShaderBytecode.GetHash();

	return Desc;
}

FD3D12PipelineStateWorker::FD3D12PipelineStateWorker(FD3D12Adapter* Adapter, const ComputePipelineCreationArgs& InArgs)
	: FD3D12AdapterChild(Adapter)
	, bIsGraphics(false)
{
	CreationArgs.ComputeArgs.Init(InArgs.Args);
};

FD3D12PipelineStateWorker::FD3D12PipelineStateWorker(FD3D12Adapter* Adapter, const GraphicsPipelineCreationArgs& InArgs)
	: FD3D12AdapterChild(Adapter)
	, bIsGraphics(true)
{
	CreationArgs.GraphicsArgs.Init(InArgs.Args);
};


/// @endcond

#ifndef __clang__

FORCEINLINE uint32 SSE4_CRC32(const void* Data, SIZE_T NumBytes)
{
	check(GCPUSupportsSSE4);
	uint32 Hash = 0;
#if defined(_WIN64)
	static const SIZE_T Alignment = 8;//64 Bit
#elif defined(_WIN32)
	static const SIZE_T Alignment = 4;//32 Bit
#else
	check(0);
	return 0;
#endif

	const SIZE_T RoundingIterations = (NumBytes & (Alignment - 1));
	uint8* UnalignedData = (uint8*)Data;
	for (SIZE_T i = 0; i < RoundingIterations; i++)
	{
		Hash = _mm_crc32_u8(Hash, UnalignedData[i]);
	}
	UnalignedData += RoundingIterations;
	NumBytes -= RoundingIterations;

	SIZE_T* AlignedData = (SIZE_T*)UnalignedData;
	check((NumBytes % Alignment) == 0);
	const SIZE_T NumIterations = (NumBytes / Alignment);
	for (SIZE_T i = 0; i < NumIterations; i++)
	{
#ifdef _WIN64
		Hash = _mm_crc32_u64(Hash, AlignedData[i]);
#else
		Hash = _mm_crc32_u32(Hash, AlignedData[i]);
#endif
	}

	return Hash;
}

#endif

uint32 FD3D12PipelineStateCacheBase::HashData(const void* Data, SIZE_T NumBytes)
{
#ifdef __clang__
	return FCrc::MemCrc32(Data, NumBytes);
#else
	if (GCPUSupportsSSE4)
	{
		return SSE4_CRC32(Data, NumBytes);
	}
	else
	{
		return FCrc::MemCrc32(Data, NumBytes);
	}
#endif
}

SIZE_T FD3D12PipelineStateCacheBase::HashPSODesc(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc)
{
	__declspec(align(32)) FD3D12LowLevelGraphicsPipelineStateDesc Hash;
	FMemory::Memcpy(&Hash, &Desc, sizeof(Hash)); // Memcpy to avoid introducing garbage due to alignment

	Hash.Desc.VS.pShaderBytecode = nullptr; // null out pointers so stale ones don't ruin the hash
	Hash.Desc.PS.pShaderBytecode = nullptr;
	Hash.Desc.HS.pShaderBytecode = nullptr;
	Hash.Desc.DS.pShaderBytecode = nullptr;
	Hash.Desc.GS.pShaderBytecode = nullptr;
	Hash.Desc.InputLayout.pInputElementDescs = nullptr;
	Hash.Desc.StreamOutput.pBufferStrides = nullptr;
	Hash.Desc.StreamOutput.pSODeclaration = nullptr;
	Hash.Desc.CachedPSO.pCachedBlob = nullptr;
	Hash.Desc.CachedPSO.CachedBlobSizeInBytes = 0;
	Hash.CombinedHash = 0;
	Hash.Desc.pRootSignature = nullptr;
	Hash.pRootSignature = nullptr;

	return SIZE_T(HashData(&Hash, sizeof(Hash)));
}

SIZE_T FD3D12PipelineStateCacheBase::HashPSODesc(const FD3D12ComputePipelineStateDesc& Desc)
{
	__declspec(align(32)) FD3D12ComputePipelineStateDesc Hash;
	FMemory::Memcpy(&Hash, &Desc, sizeof(Hash)); // Memcpy to avoid introducing garbage due to alignment

	Hash.Desc.CS.pShaderBytecode = nullptr;  // null out pointers so stale ones don't ruin the hash
	Hash.Desc.CachedPSO.pCachedBlob = nullptr;
	Hash.Desc.CachedPSO.CachedBlobSizeInBytes = 0;
	Hash.CombinedHash = 0;
	Hash.Desc.pRootSignature = nullptr;
	Hash.pRootSignature = nullptr;

	return SIZE_T(HashData(&Hash, sizeof(Hash)));
}

#define SSE4_2     0x100000 
#define SSE4_CPUID_ARRAY_INDEX 2

FD3D12PipelineStateCacheBase::FD3D12PipelineStateCacheBase(FD3D12Adapter* InParent) :
	FD3D12AdapterChild(InParent)
{
	// Check for SSE4 support see: https://msdn.microsoft.com/en-us/library/vstudio/hskdteyh(v=vs.100).aspx
	{
		int32 cpui[4];
		__cpuidex(cpui, 1, 0);
		GCPUSupportsSSE4 = !!(cpui[SSE4_CPUID_ARRAY_INDEX] & SSE4_2);
	}
}

FD3D12PipelineStateCacheBase::~FD3D12PipelineStateCacheBase()
{
	CleanupPipelineStateCaches();
}


FD3D12PipelineState::FD3D12PipelineState(FD3D12Adapter* Parent)
	: FD3D12AdapterChild(Parent)
	, FD3D12MultiNodeGPUObject(FRHIGPUMask::All(), FRHIGPUMask::All()) //Create on all, visible on all
	, Worker(nullptr)
	, PendingWaitOnWorkerCalls(0)
	, bAddToDiskCache(false)
{
	INC_DWORD_STAT(STAT_D3D12NumPSOs);
}

FD3D12PipelineState::~FD3D12PipelineState()
{
	if (Worker)
	{
		ensure(PendingWaitOnWorkerCalls == 0);
		Worker->EnsureCompletion(true);
		delete Worker;
		Worker = nullptr;
	}

	DEC_DWORD_STAT(STAT_D3D12NumPSOs);
}

ID3D12PipelineState* FD3D12PipelineState::GetPipelineState()
{
	if (Worker)
	{
		const bool bIsSyncThread = (FPlatformAtomics::InterlockedIncrement(&PendingWaitOnWorkerCalls) == 1);

		// Cache the Worker ptr as the thread with bIsSyncThread cloud clear it at any time.
		// MemoryBarrier() is required to prevent the compiler from caching Worker.
		FPlatformMisc::MemoryBarrier();
		FAsyncTask<FD3D12PipelineStateWorker>* WorkerRef = Worker;
		if (WorkerRef)
		{
			WorkerRef->EnsureCompletion(true);
			check(WorkerRef->IsWorkDone());		

			if (bIsSyncThread)
			{
				PipelineState = WorkerRef->GetTask().PSO;
				
				// Only set the worker to null after setting the PipelineState because of the initial branching.
				// Note that only one thread must set the pipeline state as TRefCountPtr is not threadsafe.
				Worker = nullptr;

				// Decrement but also wait till 0 before deleting the worker as other threads could be referring it.
				if (FPlatformAtomics::InterlockedDecrement(&PendingWaitOnWorkerCalls) != 0)
				{
					do
					{
						FPlatformProcess::Sleep(0);
					}
					while (PendingWaitOnWorkerCalls != 0);
				}

				delete WorkerRef;
			}
			else
			{
				// Cache the result before decrementing the counter because after the decrement, 
				// the worker could be deleted at any time by the thread with bIsSyncThread.
				// this allows to return immediately without having to wait for PendingWaitOnWorkerCalls to reach 0.
				ID3D12PipelineState* Result = WorkerRef->GetTask().PSO.GetReference();
				FPlatformAtomics::InterlockedDecrement(&PendingWaitOnWorkerCalls);
				return Result;
			}
		}
		else // Decrement but don't wait since if Worker is null, PipelineState is valid.
		{
			FPlatformAtomics::InterlockedDecrement(&PendingWaitOnWorkerCalls);
		}
	}
	return PipelineState.GetReference();
}

FD3D12GraphicsPipelineState::FD3D12GraphicsPipelineState(
	const FGraphicsPipelineStateInitializer& Initializer,
	FD3D12BoundShaderState* InBoundShaderState,
	FD3D12PipelineState* InPipelineState)
	: PipelineStateInitializer(Initializer)
	, RootSignature(InBoundShaderState->pRootSignature)
	, PipelineState(InPipelineState)
{
	FMemory::Memcpy(StreamStrides, InBoundShaderState->StreamStrides, sizeof(StreamStrides));
	bShaderNeedsGlobalConstantBuffer[SF_Vertex] = GetVertexShader() && GetVertexShader()->ResourceCounts.bGlobalUniformBufferUsed;
	bShaderNeedsGlobalConstantBuffer[SF_Pixel] = GetPixelShader() && GetPixelShader()->ResourceCounts.bGlobalUniformBufferUsed;
	bShaderNeedsGlobalConstantBuffer[SF_Hull] = GetHullShader() && GetHullShader()->ResourceCounts.bGlobalUniformBufferUsed;
	bShaderNeedsGlobalConstantBuffer[SF_Domain] = GetDomainShader() && GetDomainShader()->ResourceCounts.bGlobalUniformBufferUsed;
	bShaderNeedsGlobalConstantBuffer[SF_Geometry] = GetGeometryShader() && GetGeometryShader()->ResourceCounts.bGlobalUniformBufferUsed;
}

FD3D12GraphicsPipelineState::~FD3D12GraphicsPipelineState()
{
	// At this point the object is not safe to use in the PSO cache.
	// Currently, the PSO cache manages the lifetime but we could potentially
	// stop doing an AddRef() and remove the PipelineState from any caches at this point.

#if D3D12_USE_DERIVED_PSO
	// On XboxOne the pipeline state is the derived object.
	delete PipelineState;
	PipelineState = nullptr;
#endif // D3D12_USE_DERIVED_PSO
}

FD3D12ComputePipelineState::~FD3D12ComputePipelineState()
{
	// At this point the object is not safe to use in the PSO cache.
	// Currently, the PSO cache manages the lifetime but we could potentially
	// stop doing an AddRef() and remove the PipelineState from any caches at this point.
}

void FD3D12PipelineStateCacheBase::CleanupPipelineStateCaches()
{
	{
		FRWScopeLock Lock(InitializerToGraphicsPipelineMapMutex, FRWScopeLockType::SLT_Write);
		// The runtime caches manage the lifetime of their FD3D12GraphicsPipelineState and FD3D12ComputePipelineState.
		// We need to release them.
		for (auto Pair : InitializerToGraphicsPipelineMap)
		{
			FD3D12GraphicsPipelineState* GraphicsPipelineState = Pair.Value;
			ensure(GIsRHIInitialized || (!GIsRHIInitialized && GraphicsPipelineState->GetRefCount() == 1));
			GraphicsPipelineState->Release();
		}
		InitializerToGraphicsPipelineMap.Reset();
	}

	{
		FRWScopeLock Lock(ComputeShaderToComputePipelineMapMutex, FRWScopeLockType::SLT_Write);
		for (auto Pair : ComputeShaderToComputePipelineMap)
		{
			FD3D12ComputePipelineState* ComputePipelineState = Pair.Value;
			ensure(GIsRHIInitialized || (!GIsRHIInitialized && ComputePipelineState->GetRefCount() == 1));
			ComputePipelineState->Release();
		}
		ComputeShaderToComputePipelineMap.Reset();
	}

	{
		FRWScopeLock Lock(LowLevelGraphicsPipelineStateCacheMutex, FRWScopeLockType::SLT_Write);
		// The low level graphics and compute maps manage the lifetime of their PSOs.
		// We need to delete each element before we empty it.
		for (auto Iter = LowLevelGraphicsPipelineStateCache.CreateConstIterator(); Iter; ++Iter)
		{
			const FD3D12PipelineState* const PipelineState = Iter.Value();
			delete PipelineState;
		}
		LowLevelGraphicsPipelineStateCache.Empty();
	}

	{
		FRWScopeLock Lock(ComputePipelineStateCacheMutex, FRWScopeLockType::SLT_Write);
		for (auto Iter = ComputePipelineStateCache.CreateConstIterator(); Iter; ++Iter)
		{
			const FD3D12PipelineState* const PipelineState = Iter.Value();
			delete PipelineState;
		}
		ComputePipelineStateCache.Empty();
	}
}

FD3D12GraphicsPipelineState* FD3D12PipelineStateCacheBase::AddToRuntimeCache(const FGraphicsPipelineStateInitializer& Initializer, uint32 InitializerHash, FD3D12BoundShaderState* BoundShaderState, FD3D12PipelineState* PipelineState)
{
	// Lifetime managed by the runtime cache. AddRef() so the upper level doesn't delete the FD3D12GraphicsPipelineState objects while they're still in the runtime cache.
	// One alternative is to remove the object from the runtime cache in the FD3D12GraphicsPipelineState destructor.
	FD3D12GraphicsPipelineState* const GraphicsPipelineState = new FD3D12GraphicsPipelineState(Initializer, BoundShaderState, PipelineState);
	GraphicsPipelineState->AddRef();

	check(GraphicsPipelineState && InitializerHash != 0);
	check(GraphicsPipelineState->PipelineState != nullptr);

	{
		FRWScopeLock Lock(InitializerToGraphicsPipelineMapMutex, FRWScopeLockType::SLT_Write);
		InitializerToGraphicsPipelineMap.Add(InitializerHash, GraphicsPipelineState);
	}

	INC_DWORD_STAT(STAT_PSOGraphicsNumHighlevelCacheEntries);
	return GraphicsPipelineState;
}

FD3D12PipelineState* FD3D12PipelineStateCacheBase::FindInLowLevelCache(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc)
{
	check(Desc.CombinedHash != 0);

	{
		FRWScopeLock Lock(LowLevelGraphicsPipelineStateCacheMutex, FRWScopeLockType::SLT_ReadOnly);
		FD3D12PipelineState** Found = LowLevelGraphicsPipelineStateCache.Find(Desc);
		if (Found)
		{
			INC_DWORD_STAT(STAT_PSOGraphicsLowlevelCacheHit);
			return *Found;
		}
	}

	INC_DWORD_STAT(STAT_PSOGraphicsLowlevelCacheMiss);
	return nullptr;
}

FD3D12PipelineState* FD3D12PipelineStateCacheBase::CreateAndAddToLowLevelCache(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc)
{
	// Add PSO to low level cache.
	FD3D12PipelineState* PipelineState = nullptr;
	AddToLowLevelCache(Desc, &PipelineState, [this](FD3D12PipelineState** PipelineState, const FD3D12LowLevelGraphicsPipelineStateDesc& Desc)
	{ 
		OnPSOCreated(*PipelineState, Desc);

		// The lock will be held at this point so we can modify the cache.
		// Clean ourselves up if the compilation failed.
		// Note: This check is called here instead of in AddToLowLevelCache
		// because GetPipelineState will force a synchronization. This
		// path is always synchronous anyway.
		if ((*PipelineState)->GetPipelineState() == nullptr)
		{
			this->LowLevelGraphicsPipelineStateCache.Remove(Desc);
			delete *PipelineState;
			*PipelineState = nullptr;
		}
	});

	return PipelineState;
}

void FD3D12PipelineStateCacheBase::AddToLowLevelCache(const FD3D12LowLevelGraphicsPipelineStateDesc& Desc, FD3D12PipelineState** OutPipelineState, const FPostCreateGraphicCallback& PostCreateCallback)
{
	check(Desc.CombinedHash != 0);

	// Double check the desc doesn't already exist while the lock is taken.
	// This avoids having multiple threads try to create the same PSO.
	FRWScopeLock Lock(LowLevelGraphicsPipelineStateCacheMutex, FRWScopeLockType::SLT_Write);
	FD3D12PipelineState** const PipelineState = LowLevelGraphicsPipelineStateCache.Find(Desc);
	if (PipelineState)
	{
		// This desc already exists.
		*OutPipelineState = *PipelineState;
	}
	else
	{
		FD3D12PipelineState* NewPipelineState = new FD3D12PipelineState(GetParentAdapter());
		LowLevelGraphicsPipelineStateCache.Add(Desc, NewPipelineState);

		INC_DWORD_STAT(STAT_PSOGraphicsNumLowlevelCacheEntries);
		*OutPipelineState = NewPipelineState;

		// Do the callback now with the lock still on.
		PostCreateCallback(OutPipelineState, Desc);
	}
}

FD3D12ComputePipelineState* FD3D12PipelineStateCacheBase::AddToRuntimeCache(FD3D12ComputeShader* ComputeShader, FD3D12PipelineState* PipelineState)
{
	// Lifetime managed by the runtime cache. AddRef() so the upper level doesn't delete the FD3D12ComputePipelineState objects while they're still in the runtime cache.
	// One alternative is to remove the object from the runtime cache in the FD3D12ComputePipelineState destructor.
	FD3D12ComputePipelineState* const ComputePipelineState = new FD3D12ComputePipelineState(ComputeShader, PipelineState);
	ComputePipelineState->AddRef();

	check(ComputePipelineState && ComputeShader != nullptr);
	check(ComputePipelineState->PipelineState != nullptr);

	{
		FRWScopeLock Lock(ComputeShaderToComputePipelineMapMutex, FRWScopeLockType::SLT_Write);
		ComputeShaderToComputePipelineMap.Add(ComputeShader, ComputePipelineState);
	}

	INC_DWORD_STAT(STAT_PSOComputeNumHighlevelCacheEntries);
	return ComputePipelineState;
}

FD3D12PipelineState* FD3D12PipelineStateCacheBase::FindInLowLevelCache(const FD3D12ComputePipelineStateDesc& Desc)
{
	check(Desc.CombinedHash != 0);

	{
		FRWScopeLock Lock(ComputePipelineStateCacheMutex, FRWScopeLockType::SLT_ReadOnly);
		FD3D12PipelineState** Found = ComputePipelineStateCache.Find(Desc);
		if (Found)
		{
			INC_DWORD_STAT(STAT_PSOComputeLowlevelCacheHit);
			return *Found;
		}
	}

	INC_DWORD_STAT(STAT_PSOComputeLowlevelCacheMiss);
	return nullptr;
}

FD3D12PipelineState* FD3D12PipelineStateCacheBase::CreateAndAddToLowLevelCache(const FD3D12ComputePipelineStateDesc& Desc)
{
	// Add PSO to low level cache.
	FD3D12PipelineState* PipelineState = nullptr;

	AddToLowLevelCache(Desc, &PipelineState, [&](FD3D12PipelineState* PipelineState, const FD3D12ComputePipelineStateDesc& Desc)
	{ 
		OnPSOCreated(PipelineState, Desc); 
	});

	return PipelineState;
}

void FD3D12PipelineStateCacheBase::AddToLowLevelCache(const FD3D12ComputePipelineStateDesc& Desc, FD3D12PipelineState** OutPipelineState, const FPostCreateComputeCallback& PostCreateCallback)
{
	check(Desc.CombinedHash != 0);

	// Double check the desc doesn't already exist while the lock is taken.
	// This avoids having multiple threads try to create the same PSO.
	FRWScopeLock Lock(ComputePipelineStateCacheMutex, FRWScopeLockType::SLT_Write);
	FD3D12PipelineState** const PipelineState = ComputePipelineStateCache.Find(Desc);
	if (PipelineState)
	{
		// This desc already exists.
		*OutPipelineState = *PipelineState;
	}
	else
	{
		FD3D12PipelineState* NewPipelineState = new FD3D12PipelineState(GetParentAdapter());
		ComputePipelineStateCache.Add(Desc, NewPipelineState);

		INC_DWORD_STAT(STAT_PSOComputeNumLowlevelCacheEntries);
		*OutPipelineState = NewPipelineState;

		// Do the callback now with the lock still on.
		PostCreateCallback(NewPipelineState, Desc);
	}
}

FD3D12GraphicsPipelineState* FD3D12PipelineStateCacheBase::FindInRuntimeCache(const FGraphicsPipelineStateInitializer& Initializer, uint32& OutHash)
{
	OutHash = HashData(&Initializer, sizeof(Initializer));

	{
		FRWScopeLock Lock(InitializerToGraphicsPipelineMapMutex, FRWScopeLockType::SLT_ReadOnly);
		FD3D12GraphicsPipelineState** GraphicsPipelineState = InitializerToGraphicsPipelineMap.Find(OutHash);
		if (GraphicsPipelineState)
		{
			INC_DWORD_STAT(STAT_PSOGraphicsHighlevelCacheHit);
			return *GraphicsPipelineState;
		}
	}

	INC_DWORD_STAT(STAT_PSOGraphicsHighlevelCacheMiss);
	return nullptr;
}

FD3D12GraphicsPipelineState* FD3D12PipelineStateCacheBase::FindInLoadedCache(const FGraphicsPipelineStateInitializer& Initializer, uint32 InitializerHash, FD3D12BoundShaderState* BoundShaderState, FD3D12LowLevelGraphicsPipelineStateDesc& OutLowLevelDesc)
{
	// TODO: For now PSOs will be created on every node of the LDA chain.
	OutLowLevelDesc = GetLowLevelGraphicsPipelineStateDesc(Initializer, BoundShaderState);
	OutLowLevelDesc.Desc.NodeMask = (uint32)FRHIGPUMask::All();

	OutLowLevelDesc.CombinedHash = FD3D12PipelineStateCacheBase::HashPSODesc(OutLowLevelDesc);

	// First try to find the PSO in the low level cache that can be populated from disk.
	FD3D12PipelineState* PipelineState = FindInLowLevelCache(OutLowLevelDesc);
	if (PipelineState)
	{
		// Add the PSO to the runtime cache for better performance next time.
		return AddToRuntimeCache(Initializer, InitializerHash, BoundShaderState, PipelineState);
	}

	// TODO: Try to load from a PipelineLibrary now instead of at Create time.

	return nullptr;
}

FD3D12GraphicsPipelineState* FD3D12PipelineStateCacheBase::CreateAndAdd(const FGraphicsPipelineStateInitializer& Initializer, uint32 InitializerHash, FD3D12BoundShaderState* BoundShaderState, const FD3D12LowLevelGraphicsPipelineStateDesc& LowLevelDesc)
{
	FD3D12PipelineState* const PipelineState = CreateAndAddToLowLevelCache(LowLevelDesc);
	if (PipelineState == nullptr)
	{
		return nullptr;
	}

	// Add the PSO to the runtime cache for better performance next time.
	return AddToRuntimeCache(Initializer, InitializerHash, BoundShaderState, PipelineState);
}

FD3D12ComputePipelineState* FD3D12PipelineStateCacheBase::FindInRuntimeCache(const FD3D12ComputeShader* ComputeShader)
{
	{
		FRWScopeLock Lock(ComputeShaderToComputePipelineMapMutex, FRWScopeLockType::SLT_ReadOnly);
		FD3D12ComputePipelineState** ComputePipelineState = ComputeShaderToComputePipelineMap.Find(ComputeShader);
		if (ComputePipelineState)
		{
			INC_DWORD_STAT(STAT_PSOComputeHighlevelCacheHit);
			return *ComputePipelineState;
		}
	}

	INC_DWORD_STAT(STAT_PSOComputeHighlevelCacheMiss);
	return nullptr;
}

FD3D12ComputePipelineState* FD3D12PipelineStateCacheBase::FindInLoadedCache(FD3D12ComputeShader* ComputeShader, FD3D12ComputePipelineStateDesc& OutLowLevelDesc)
{
	// TODO: For now PSOs will be created on every node of the LDA chain.
	OutLowLevelDesc = GetComputePipelineStateDesc(ComputeShader);
	OutLowLevelDesc.Desc.NodeMask = (uint32)FRHIGPUMask::All();
	OutLowLevelDesc.CombinedHash = FD3D12PipelineStateCacheBase::HashPSODesc(OutLowLevelDesc);

	// First try to find the PSO in the low level cache that can be populated from disk.
	FD3D12PipelineState* PipelineState = FindInLowLevelCache(OutLowLevelDesc);
	if (PipelineState)
	{
		// Add the PSO to the runtime cache for better performance next time.
		return AddToRuntimeCache(ComputeShader, PipelineState);
	}

	// TODO: Try to load from a PipelineLibrary now instead of at Create time.

	return nullptr;
}

FD3D12ComputePipelineState* FD3D12PipelineStateCacheBase::CreateAndAdd(FD3D12ComputeShader* ComputeShader, const FD3D12ComputePipelineStateDesc& LowLevelDesc)
{
	FD3D12PipelineState* const PipelineState = CreateAndAddToLowLevelCache(LowLevelDesc);

	// Add the PSO to the runtime cache for better performance next time.
	return AddToRuntimeCache(ComputeShader, PipelineState);
}
