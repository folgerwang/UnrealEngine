// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "D3D12RayTracing.h"

#if D3D12_RHI_RAYTRACING

#include "D3D12Resources.h"
#include "Containers/DynamicRHIResourceArray.h"
#include "Containers/BitArray.h"
#include "BuiltInRayTracingShaders.h"
#include "CommonRayTracingBuiltInResources.ush"
#include "Hash/CityHash.h"
#include "HAL/CriticalSection.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeLock.h"

static int32 GRayTracingDebugForceOpaque = 0;
static FAutoConsoleVariableRef CVarRayTracingDebugForceOpaque(
	TEXT("r.RayTracing.DebugForceOpaque"),
	GRayTracingDebugForceOpaque,
	TEXT("Forces all ray tracing geometry instances to be opaque, effectively disabling any-hit shaders. This is useful for debugging and profiling. (default = 0)")
);

static int32 GRayTracingDebugDisableTriangleCull = 0;
static FAutoConsoleVariableRef CVarRayTracingDebugDisableTriangleCull(
	TEXT("r.RayTracing.DebugDisableTriangleCull"),
	GRayTracingDebugDisableTriangleCull,
	TEXT("Forces all ray tracing geometry instances to be double-sided by disabling back-face culling. This is useful for debugging and profiling. (default = 0)")
);

#define VERIFYHRESULT(expr) { HRESULT HR##__LINE__ = expr; if (FAILED(HR##__LINE__)) { UE_LOG(LogD3D12RHI, Fatal, TEXT(#expr " failed: Result=%08x"), HR##__LINE__); } }

// Built-in local root parameters that are always bound to all hit shaders
struct FHitGroupSystemParameters
{
	D3D12_GPU_VIRTUAL_ADDRESS IndexBuffer;
	D3D12_GPU_VIRTUAL_ADDRESS VertexBuffer;
	FHitGroupSystemVertexFetchParameters FetchParameters;
};

struct FD3D12ShaderIdentifier
{
	// Shader identifier size is statically defined on RS5, but is dynamic on RS4.
	// RS5 size is always D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES (32) and we assume that it will never be bigger than that on RS4.

	uint64 Data[4] = {~0ull, ~0ull, ~0ull, ~0ull};

	bool operator == (const FD3D12ShaderIdentifier& Other) const
	{
		return Data[0] == Other.Data[0]
			&& Data[1] == Other.Data[1]
			&& Data[2] == Other.Data[2]
			&& Data[3] == Other.Data[3];
	}

	bool operator != (const FD3D12ShaderIdentifier& Other) const
	{
		return !(*this == Other);
	}

	bool IsValid() const
	{
		return *this != FD3D12ShaderIdentifier();
	}
};

static_assert(sizeof(FD3D12ShaderIdentifier) == D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, "Unexpected shader identifier size");

struct FDXILLibrary
{
	FDXILLibrary() = default;

	//no copy assignment or move because FDXILLibrary points to internal struct memory
	FDXILLibrary(FDXILLibrary&&) = delete;
	FDXILLibrary(const FDXILLibrary&) = delete;
	FDXILLibrary& operator=(const FDXILLibrary&) = delete;
	FDXILLibrary& operator=(FDXILLibrary&&) = delete;

	void InitFromDXIL(const void* Bytecode, uint32 BytecodeLength, const LPCWSTR* InEntryNames, const LPCWSTR* InExportNames, uint32 NumEntryNames)
	{
		check(NumEntryNames != 0);
		check(InEntryNames);
		check(InExportNames);

		EntryNames.SetNum(NumEntryNames);
		ExportNames.SetNum(NumEntryNames);
		ExportDesc.SetNum(NumEntryNames);

		for (uint32 EntryIndex = 0; EntryIndex < NumEntryNames; ++EntryIndex)
		{
			EntryNames[EntryIndex] = InEntryNames[EntryIndex];
			ExportNames[EntryIndex] = InExportNames[EntryIndex];

			ExportDesc[EntryIndex].ExportToRename = *(EntryNames[EntryIndex]);
			ExportDesc[EntryIndex].Flags = D3D12_EXPORT_FLAG_NONE;
			ExportDesc[EntryIndex].Name = *(ExportNames[EntryIndex]);
		}

		Desc.DXILLibrary.pShaderBytecode = Bytecode;
		Desc.DXILLibrary.BytecodeLength = BytecodeLength;
		Desc.NumExports = ExportDesc.Num();
		Desc.pExports = ExportDesc.GetData();
	}

	void InitFromDXIL(const D3D12_SHADER_BYTECODE& ShaderBytecode, LPCWSTR* InEntryNames, LPCWSTR* InExportNames, uint32 NumEntryNames)
	{
		InitFromDXIL(ShaderBytecode.pShaderBytecode, ShaderBytecode.BytecodeLength, InEntryNames, InExportNames, NumEntryNames);
	}

	void InitFromDXIL(const FD3D12ShaderBytecode& ShaderBytecode, LPCWSTR* InEntryNames, LPCWSTR* InExportNames, uint32 NumEntryNames)
	{
		InitFromDXIL(ShaderBytecode.GetShaderBytecode(), InEntryNames, InExportNames, NumEntryNames);
	}

	D3D12_STATE_SUBOBJECT GetSubobject() const
	{
		D3D12_STATE_SUBOBJECT Subobject = {};
		Subobject.Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
		Subobject.pDesc = &Desc;
		return Subobject;
	}

	// NOTE: typical DXIL library contains only a single function (up to 3 for hit groups with closest hit, any hit and intersection shaders)
	TArray<D3D12_EXPORT_DESC, TInlineAllocator<1>> ExportDesc;
	TArray<FString, TInlineAllocator<1>> EntryNames;
	TArray<FString, TInlineAllocator<1>> ExportNames;

	D3D12_DXIL_LIBRARY_DESC Desc = {};
};

static TRefCountPtr<ID3D12StateObject> CreateRayTracingStateObject(
	ID3D12Device5* RayTracingDevice,
	const TArrayView<const FDXILLibrary*>& ShaderLibraries,
	const TArrayView<LPCWSTR>& Exports,
	uint32 MaxPayloadSizeInBytes,
	const TArrayView<const D3D12_HIT_GROUP_DESC>& HitGroups,
	const FD3D12RootSignature& GlobalRootSignature,
	const TArrayView<ID3D12RootSignature*>& LocalRootSignatures,
	const TArrayView<uint32>& LocalRootSignatureAssociations // indices into LocalRootSignatures, one per export
)
{
	checkf(LocalRootSignatureAssociations.Num() == Exports.Num(), TEXT("There must be exactly one local root signature association per export."));

	TRefCountPtr<ID3D12StateObject> Result;

	// There are several pipeline sub-objects that are always required:
	// 1) D3D12_RAYTRACING_SHADER_CONFIG
	// 2) D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION
	// 3) D3D12_RAYTRACING_PIPELINE_CONFIG
	// 4) Global root signature
	static constexpr uint32 NumRequiredSubobjects = 4;

	TArray<D3D12_STATE_SUBOBJECT, TInlineAllocator<16>> Subobjects;
	Subobjects.SetNumUninitialized(NumRequiredSubobjects
		+ ShaderLibraries.Num()
		+ HitGroups.Num()
		+ LocalRootSignatures.Num()
		+ LocalRootSignatureAssociations.Num()
	);

	TArray<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION, TInlineAllocator<16>> ExportAssociations;
	ExportAssociations.SetNumUninitialized(LocalRootSignatureAssociations.Num());

	uint32 Index = 0;

	const uint32 NumExports = Exports.Num();

	// Shader libraries

	for (const FDXILLibrary* Library : ShaderLibraries)
	{
		Subobjects[Index++] = Library->GetSubobject();
	}

	// Shader config

	D3D12_RAYTRACING_SHADER_CONFIG ShaderConfig = {};
	ShaderConfig.MaxAttributeSizeInBytes = 8; // sizeof 2 floats (barycentrics)
	ShaderConfig.MaxPayloadSizeInBytes = MaxPayloadSizeInBytes;
	const uint32 ShaderConfigIndex = Index;
	Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG, &ShaderConfig};

	// Shader config association

	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION ShaderConfigAssociation = {};
	ShaderConfigAssociation.NumExports = Exports.Num();
	ShaderConfigAssociation.pExports = Exports.GetData();
	ShaderConfigAssociation.pSubobjectToAssociate = &Subobjects[ShaderConfigIndex];
	Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &ShaderConfigAssociation };

	// Intersection hit group

	for (const D3D12_HIT_GROUP_DESC& HitGroupDesc : HitGroups)
	{
		Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP, &HitGroupDesc };
	}

	// Pipeline config

	D3D12_RAYTRACING_PIPELINE_CONFIG PipelineConfig = {};
	PipelineConfig.MaxTraceRecursionDepth = 1; // Only allow ray tracing from RayGen shader
	const uint32 PipelineConfigIndex = Index;
	Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG, &PipelineConfig };

	// Global root signature

	ID3D12RootSignature* GlobalRootSignaturePtr = GlobalRootSignature.GetRootSignature();
	Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE, &GlobalRootSignaturePtr };

	// Local root signatures

	const uint32 LocalRootSignatureBaseIndex = Index;
	for (int32 SignatureIndex = 0; SignatureIndex < LocalRootSignatures.Num(); ++SignatureIndex)
	{
		checkf(LocalRootSignatures[SignatureIndex], TEXT("All local root signatures must be valid"));
		Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE, &LocalRootSignatures[SignatureIndex] };
	}

	// Local root signature associations

	for (int32 ExportIndex = 0; ExportIndex < Exports.Num(); ++ExportIndex)
	{
		const int32 LocalRootSignatureIndex = LocalRootSignatureAssociations[ExportIndex];

		D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION& Association = ExportAssociations[ExportIndex];
		Association = D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION{};
		Association.NumExports = 1;
		Association.pExports = &Exports[ExportIndex];

		check(LocalRootSignatureIndex < LocalRootSignatures.Num());
		Association.pSubobjectToAssociate = &Subobjects[LocalRootSignatureBaseIndex + LocalRootSignatureIndex];

		Subobjects[Index++] = D3D12_STATE_SUBOBJECT{ D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION, &ExportAssociations[ExportIndex] };
	}

	checkf(Index == Subobjects.Num(), TEXT("All pipeline subobjects must be initialized."));

	// Create ray tracing pipeline state object

	D3D12_STATE_OBJECT_DESC Desc = {};
	Desc.NumSubobjects = Index;
	Desc.pSubobjects = &Subobjects[0];
	Desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

	VERIFYHRESULT(RayTracingDevice->CreateStateObject(&Desc, IID_PPV_ARGS(Result.GetInitReference())));

	return Result;
}

uint32 FD3D12DynamicRHI::RHIGetRayTracingSupport()
{
	return GetAdapter().GetD3DRayTracingDevice() ? 1 : 0;
}

// #dxr_todo: FD3D12Device::GlobalViewHeap/GlobalSamplerHeap should be used instead of ad-hoc heaps here.
// Unfortunately, this requires a major refactor of how global heaps work.
// FD3D12CommandContext-s should not get static chunks of the global heap, but instead should dynamically allocate
// chunks on as-needed basis and release them when possible.
// This would allow ray tracing code to sub-allocate heap blocks from the same global heap.
class FD3D12RayTracingDescriptorHeapCache : FD3D12DeviceChild
{
	FD3D12RayTracingDescriptorHeapCache(FD3D12RayTracingDescriptorHeapCache&&) = delete;
	FD3D12RayTracingDescriptorHeapCache(const FD3D12RayTracingDescriptorHeapCache&) = delete;
	FD3D12RayTracingDescriptorHeapCache& operator=(const FD3D12RayTracingDescriptorHeapCache&) = delete;
	FD3D12RayTracingDescriptorHeapCache& operator=(FD3D12RayTracingDescriptorHeapCache&&) = delete;

public:

	struct Entry
	{
		ID3D12DescriptorHeap* Heap = nullptr;
		FD3D12CLSyncPoint SyncPoint;
		uint32 NumDescriptors = 0;
		D3D12_DESCRIPTOR_HEAP_TYPE Type = D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES;
	};

	FD3D12RayTracingDescriptorHeapCache(FD3D12Device* Device)
		: FD3D12DeviceChild(Device)
	{

	}

	~FD3D12RayTracingDescriptorHeapCache()
	{
		check(AllocatedEntries == 0);

		Flush();

		check(Entries.Num() == 0);
	}

	void ReleaseHeap(Entry& Entry)
	{
		FScopeLock Lock(&CriticalSection);

		Entries.Add(Entry);

		check(AllocatedEntries != 0);
		--AllocatedEntries;
	}

	Entry AllocateHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32 NumDescriptors)
	{
		FScopeLock Lock(&CriticalSection);

		++AllocatedEntries;

		Entry Result = {};

		for (int32 EntryIndex = 0; EntryIndex < Entries.Num(); ++EntryIndex)
		{
			const Entry& It = Entries[EntryIndex];
			if (It.Type == Type && It.NumDescriptors >= NumDescriptors && It.SyncPoint.IsComplete())
			{
				Result = It;

				Entries[EntryIndex] = Entries.Last();
				Entries.Pop();

				return Result;
			}
		}

		D3D12_DESCRIPTOR_HEAP_DESC Desc = {};

		Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		Desc.Type = Type;
		Desc.NumDescriptors = NumDescriptors;
		Desc.NodeMask = 1; // #dxr_todo: handle mGPU

		ID3D12DescriptorHeap* D3D12Heap = nullptr;

		VERIFYHRESULT(GetParentDevice()->GetDevice()->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&D3D12Heap)));
		SetName(D3D12Heap, Desc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ? L"RT View Heap" : L"RT Sampler Heap");

		Result.NumDescriptors = NumDescriptors;
		Result.Type = Type;
		Result.Heap = D3D12Heap;

		return Result;
	}

	void Flush()
	{
		FD3D12Device* Device = GetParentDevice();

		FScopeLock Lock(&CriticalSection);

		for (const Entry& It : Entries)
		{
			Device->GetParentAdapter()->GetDeferredDeletionQueue().EnqueueResource(It.Heap);
		}
		Entries.Empty();
	}

	FCriticalSection CriticalSection;
	TArray<Entry> Entries;
	uint32 AllocatedEntries = 0;
};

struct FD3D12RayTracingDescriptorHeap : public FD3D12DeviceChild
{
	FD3D12RayTracingDescriptorHeap(FD3D12RayTracingDescriptorHeap&&) = delete;
	FD3D12RayTracingDescriptorHeap(const FD3D12RayTracingDescriptorHeap&) = delete;
	FD3D12RayTracingDescriptorHeap& operator=(const FD3D12RayTracingDescriptorHeap&) = delete;
	FD3D12RayTracingDescriptorHeap& operator=(FD3D12RayTracingDescriptorHeap&&) = delete;

	FD3D12RayTracingDescriptorHeap(FD3D12Device* Device)
		: FD3D12DeviceChild(Device)
	{

	}

	~FD3D12RayTracingDescriptorHeap()
	{
		if (D3D12Heap)
		{
			GetParentDevice()->GetRayTracingDescriptorHeapCache()->ReleaseHeap(HeapCacheEntry);
		}
	}

	void Init(uint32 InMaxNumDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE Type)
	{
		check(D3D12Heap == nullptr);

		HeapCacheEntry = GetParentDevice()->GetRayTracingDescriptorHeapCache()->AllocateHeap(Type, InMaxNumDescriptors);

		MaxNumDescriptors = HeapCacheEntry.NumDescriptors;
		D3D12Heap = HeapCacheEntry.Heap;

		CPUBase = D3D12Heap->GetCPUDescriptorHandleForHeapStart();
		GPUBase = D3D12Heap->GetGPUDescriptorHandleForHeapStart();
		DescriptorSize = GetParentDevice()->GetDevice()->GetDescriptorHandleIncrementSize(Type);
	}

	bool CanAllocate(uint32 InNumDescriptors) const
	{
		return NumAllocatedDescriptors + InNumDescriptors <= MaxNumDescriptors;
	}

	uint32 Allocate(uint32 InNumDescriptors)
	{
		check(CanAllocate(InNumDescriptors));

		uint32 Result = NumAllocatedDescriptors;
		NumAllocatedDescriptors += InNumDescriptors;
		return Result;
	}

	D3D12_CPU_DESCRIPTOR_HANDLE GetDescriptorCPU(uint32 Index) const
	{
		checkSlow(Index < MaxNumDescriptors);
		D3D12_CPU_DESCRIPTOR_HANDLE Result = { CPUBase.ptr + Index * DescriptorSize };
		return Result;
	}

	D3D12_GPU_DESCRIPTOR_HANDLE GetDescriptorGPU(uint32 Index) const
	{
		checkSlow(Index < MaxNumDescriptors);
		D3D12_GPU_DESCRIPTOR_HANDLE Result = { GPUBase.ptr + Index * DescriptorSize };
		return Result;
	}

	void UpdateSyncPoint(FD3D12CommandListHandle CommandListHandle)
	{
		if (CommandListHandle.CurrentGeneration() > HeapCacheEntry.SyncPoint.GetGeneration())
		{
			HeapCacheEntry.SyncPoint = CommandListHandle;
		}
	}

	ID3D12DescriptorHeap* D3D12Heap = nullptr;
	uint32 MaxNumDescriptors = 0;
	uint32 NumAllocatedDescriptors = 0;

	uint32 DescriptorSize = 0;
	D3D12_CPU_DESCRIPTOR_HANDLE CPUBase = {};
	D3D12_GPU_DESCRIPTOR_HANDLE GPUBase = {};

	FD3D12RayTracingDescriptorHeapCache::Entry HeapCacheEntry;
};

class FD3D12RayTracingDescriptorCache : public FD3D12DeviceChild
{
	FD3D12RayTracingDescriptorCache(FD3D12RayTracingDescriptorCache&&) = delete;
	FD3D12RayTracingDescriptorCache(const FD3D12RayTracingDescriptorCache&) = delete;
	FD3D12RayTracingDescriptorCache& operator=(const FD3D12RayTracingDescriptorCache&) = delete;
	FD3D12RayTracingDescriptorCache& operator=(FD3D12RayTracingDescriptorCache&&) = delete;

public:

	FD3D12RayTracingDescriptorCache(FD3D12Device* Device)
		: FD3D12DeviceChild(Device)
		, ViewHeap(Device)
		, SamplerHeap(Device)
	{}

	void Init(uint32 NumViewDescriptors, uint32 NumSamplerDescriptors)
	{
		ViewHeap.Init(NumViewDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
		SamplerHeap.Init(NumSamplerDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
	}

	void UpdateSyncPoint(FD3D12CommandListHandle CommandListHandle)
	{
		ViewHeap.UpdateSyncPoint(CommandListHandle);
		SamplerHeap.UpdateSyncPoint(CommandListHandle);
	}

	void SetDescriptorHeaps(FD3D12CommandContext& CommandContext)
	{
		UpdateSyncPoint(CommandContext.CommandListHandle);

		ID3D12DescriptorHeap* Heaps[2] =
		{
			ViewHeap.D3D12Heap,
			SamplerHeap.D3D12Heap
		};

		ID3D12GraphicsCommandList* CommandList = CommandContext.CommandListHandle.GraphicsCommandList();
		CommandList->SetDescriptorHeaps(2, Heaps);
	}

	uint32 GetDescriptorTableBaseIndex(const D3D12_CPU_DESCRIPTOR_HANDLE* Descriptors, uint32 NumDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE Type)
	{
		checkSlow(Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		FD3D12RayTracingDescriptorHeap& Heap = (Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) ? ViewHeap : SamplerHeap;
		TMap<uint64, uint32>& Map = (Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) ? ViewDescriptorTableCache : SamplerDescriptorTableCache;

		const uint64 Key = CityHash64((const char*)Descriptors, sizeof(Descriptors[0]) * NumDescriptors);

		uint32 DescriptorTableBaseIndex = ~0u;
		const uint32* FoundDescriptorTableBaseIndex = Map.Find(Key);

		if (FoundDescriptorTableBaseIndex)
		{
			DescriptorTableBaseIndex = *FoundDescriptorTableBaseIndex;
		}
		else
		{
			DescriptorTableBaseIndex = Heap.Allocate(NumDescriptors);

			D3D12_CPU_DESCRIPTOR_HANDLE DestDescriptor = Heap.GetDescriptorCPU(DescriptorTableBaseIndex);
			GetParentDevice()->GetDevice()->CopyDescriptors(1, &DestDescriptor, &NumDescriptors, NumDescriptors, Descriptors, nullptr, Type);

			Map.Add(Key, DescriptorTableBaseIndex);
		}

		return DescriptorTableBaseIndex;
	}

	FD3D12RayTracingDescriptorHeap ViewHeap;
	FD3D12RayTracingDescriptorHeap SamplerHeap;

	TMap<uint64, uint32> ViewDescriptorTableCache;
	TMap<uint64, uint32> SamplerDescriptorTableCache;
};

class FD3D12RayTracingShaderTable : public FD3D12DeviceChild
{
	FD3D12RayTracingShaderTable(FD3D12RayTracingShaderTable&&) = delete;
	FD3D12RayTracingShaderTable(const FD3D12RayTracingShaderTable&) = delete;
	FD3D12RayTracingShaderTable& operator=(const FD3D12RayTracingShaderTable&) = delete;
	FD3D12RayTracingShaderTable& operator=(FD3D12RayTracingShaderTable&&) = delete;

private:

	void WriteData(uint32 WriteOffset, const void* InData, uint32 InDataSize)
	{
#if DO_CHECK && DO_GUARD_SLOW
		Data.RangeCheck(WriteOffset);
		Data.RangeCheck(WriteOffset + InDataSize - 1);
#endif // DO_CHECK && DO_GUARD_SLOW

		FMemory::Memcpy(Data.GetData() + WriteOffset, InData, InDataSize);

		bIsDirty = true;
	}

	void WriteHitGroupRecord(uint32 RecordIndex, uint32 OffsetWithinRecord, const void* InData, uint32 InDataSize)
	{
		checkfSlow(OffsetWithinRecord % 4 == 0, TEXT("SBT record parameters must be written on DWORD-aligned boundary"));
		checkfSlow(InDataSize % 4 == 0, TEXT("SBT record parameters must be DWORD-aligned"));
		checkfSlow(OffsetWithinRecord + InDataSize <= HitGroupRecordSizeUnaligned, TEXT("SBT record write request is out of bounds"));
		checkfSlow(RecordIndex < NumHitGroups, TEXT("SBT record write request is out of bounds"));

		const uint32 WriteOffset = HitGroupShaderTableOffset + HitGroupRecordStride * RecordIndex + OffsetWithinRecord;

		WriteData(WriteOffset, InData, InDataSize);
	}

public:

	FD3D12RayTracingShaderTable(FD3D12Device* Device)
		: FD3D12DeviceChild(Device)
		, DescriptorCache(Device)
	{
	}

	void Init(uint32 InNumRayGenShaders, uint32 InNumMissShaders, uint32 InNumHitGroups, uint32 LocalRootDataSize)
	{
		checkf(LocalRootDataSize <= 4096, TEXT("The maximum size of a local root signature is 4KB.")); // as per section 4.22.1 of DXR spec v1.0
		checkf(InNumRayGenShaders >= 1, TEXT("All shader tables must contain at least one raygen shader."));

		HitGroupRecordSizeUnaligned = ShaderIdentifierSize + LocalRootDataSize;
		HitGroupRecordStride = RoundUpToNextMultiple(HitGroupRecordSizeUnaligned, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);

		// Minimum number of descriptors required to support binding global resources (arbitrarily chosen)
		// #dxr_todo: Remove this when RT descriptors are sub-allocated from the global view descriptor heap.
		const uint32 MinNumViewDescriptors = 1024;
		const uint32 ApproximateDescriptorsPerRecord = 32; // #dxr_todo: calculate this based on shader reflection data

		// D3D12 is guaranteed to support 1M (D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1) descriptors in a CBV/SRV/UAV heap, so clamp the size to this.
		// https://docs.microsoft.com/en-us/windows/desktop/direct3d12/hardware-support
		const uint32 NumViewDescriptors = FMath::Max(MinNumViewDescriptors, FMath::Min<uint32>(InNumHitGroups * ApproximateDescriptorsPerRecord, D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_1));
		const uint32 NumSamplerDescriptors = D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE;

		DescriptorCache.Init(NumViewDescriptors, NumSamplerDescriptors);

		NumRayGenShaders = InNumRayGenShaders;
		NumMissShaders = InNumMissShaders;
		NumHitGroups = InNumHitGroups;

		uint32 TotalDataSize = 0;

		RayGenShaderTableOffset = TotalDataSize;
		TotalDataSize += NumRayGenShaders * RayGenRecordStride;
		TotalDataSize = RoundUpToNextMultiple(TotalDataSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		MissShaderTableOffset = TotalDataSize;
		TotalDataSize += NumMissShaders * MissRecordStride;
		TotalDataSize = RoundUpToNextMultiple(TotalDataSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		HitGroupShaderTableOffset = TotalDataSize;
		TotalDataSize += NumHitGroups * HitGroupRecordStride;
		TotalDataSize = RoundUpToNextMultiple(TotalDataSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		DefaultHitGroupShaderTableOffset = TotalDataSize;
		TotalDataSize += ShaderIdentifierSize;
		TotalDataSize = RoundUpToNextMultiple(TotalDataSize, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		Data.SetNumZeroed(TotalDataSize);

#if DO_CHECK && DO_GUARD_SLOW
		WrittenHitGroupRecords.Init(false, NumHitGroups);
#endif // DO_CHECK && DO_GUARD_SLOW
	}

	template <typename T>
	void SetHitGroupParameters(uint32 RecordIndex, uint32 InOffsetWithinRootSignature, const T& Parameters)
	{
		WriteHitGroupRecord(RecordIndex, ShaderIdentifierSize + InOffsetWithinRootSignature, &Parameters, sizeof(Parameters));
	}

	void SetHitGroupParameters(uint32 RecordIndex, uint32 InOffsetWithinRootSignature, const void* InData, uint32 InDataSize)
	{
		WriteHitGroupRecord(RecordIndex, ShaderIdentifierSize + InOffsetWithinRootSignature, InData, InDataSize);
	}

	void SetHitGroupIdentifier(uint32 RecordIndex, const void* ShaderIdentifierData, uint32 InShaderIdentifierSize)
	{
		checkSlow(InShaderIdentifierSize == ShaderIdentifierSize);

		WriteHitGroupRecord(RecordIndex, 0, ShaderIdentifierData, InShaderIdentifierSize);

#if DO_CHECK && DO_GUARD_SLOW
		if (!WrittenHitGroupRecords[RecordIndex])
		{
			WrittenHitGroupRecords[RecordIndex] = true;
			NumValidHitGroupRecords++;
		}
#endif // DO_CHECK && DO_GUARD_SLOW
	}

	void SetRayGenIdentifier(uint32 RecordIndex, const FD3D12ShaderIdentifier& ShaderIdentifier)
	{
		const uint32 WriteOffset = RayGenShaderTableOffset + RecordIndex * RayGenRecordStride;
		WriteData(WriteOffset, ShaderIdentifier.Data, ShaderIdentifierSize);
	}

	void SetMissIdentifier(uint32 RecordIndex, const FD3D12ShaderIdentifier& ShaderIdentifier)
	{
		const uint32 WriteOffset = MissShaderTableOffset + RecordIndex * MissRecordStride;
		WriteData(WriteOffset, ShaderIdentifier.Data, ShaderIdentifierSize);
	}

	void SetDefaultHitGroupIdentifier(const FD3D12ShaderIdentifier& ShaderIdentifier)
	{
		const uint32 WriteOffset = DefaultHitGroupShaderTableOffset;
		WriteData(WriteOffset, ShaderIdentifier.Data, ShaderIdentifierSize);
	}

	void SetHitGroupIdentifier(uint32 RecordIndex, const FD3D12ShaderIdentifier& ShaderIdentifier)
	{
		checkfSlow(ShaderIdentifier.IsValid(), TEXT("Shader identifier must be initialized FD3D12RayTracingPipelineState::GetShaderIdentifier() before use."));
		checkSlow(sizeof(ShaderIdentifier.Data) >= ShaderIdentifierSize);

		SetHitGroupIdentifier(RecordIndex, ShaderIdentifier.Data, ShaderIdentifierSize);
	}

	void CopyToGPU()
	{
		FD3D12Device* Device = GetParentDevice();

		checkf(Data.Num(), TEXT("Shader table is expected to be initialized before copying to GPU."));
		checkfSlow(NumValidHitGroupRecords == NumHitGroups, TEXT("Hit group shader table is expected to be fully populated before copying to GPU. Ensure that all shader records have been fille using WriteShaderIdentifier()."));

		FD3D12Adapter* Adapter = Device->GetParentAdapter();

		D3D12_RESOURCE_DESC BufferDesc = CD3DX12_RESOURCE_DESC::Buffer(Data.GetResourceDataSize(), D3D12_RESOURCE_FLAG_NONE, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);

		FRHIResourceCreateInfo CreateInfo;
		CreateInfo.ResourceArray = &Data;

		checkf(GNumExplicitGPUsForRendering == 1, TEXT("Ray tracing is not implemented for mGPU")); // #dxr_todo: implement mGPU support
		Buffer = Adapter->CreateRHIBuffer<FD3D12MemBuffer>(
			nullptr, BufferDesc, BufferDesc.Alignment,
			0, BufferDesc.Width, BUF_Static, CreateInfo,
			FRHIGPUMask::FromIndex(Device->GetGPUIndex()));

		bIsDirty = false;
	}

	D3D12_GPU_VIRTUAL_ADDRESS GetShaderTableAddress() const
	{
		return Buffer->ResourceLocation.GetGPUVirtualAddress();
	}

	D3D12_DISPATCH_RAYS_DESC GetDispatchRaysDesc(uint32 RayGenShaderIndex = 0, uint32 MissShaderBaseIndex = 0) const
	{
		D3D12_GPU_VIRTUAL_ADDRESS ShaderTableAddress = GetShaderTableAddress();

		D3D12_DISPATCH_RAYS_DESC Desc = {};

		Desc.RayGenerationShaderRecord.StartAddress = ShaderTableAddress + RayGenShaderTableOffset + RayGenShaderIndex * RayGenRecordStride;
		Desc.RayGenerationShaderRecord.SizeInBytes = RayGenRecordStride;

		Desc.MissShaderTable.StartAddress = ShaderTableAddress + MissShaderTableOffset + MissShaderBaseIndex * MissRecordStride;
		Desc.MissShaderTable.StrideInBytes = MissRecordStride;
		Desc.MissShaderTable.SizeInBytes = MissRecordStride;

		if (NumHitGroups)
		{
			Desc.HitGroupTable.StartAddress = ShaderTableAddress + HitGroupShaderTableOffset;
			Desc.HitGroupTable.StrideInBytes = HitGroupRecordStride;
			Desc.HitGroupTable.SizeInBytes = NumHitGroups * HitGroupRecordStride;
		}

		return Desc;
	}

	static constexpr uint32 ShaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	uint32 NumHitGroups = 0;
	uint32 NumRayGenShaders = 0;
	uint32 NumMissShaders = 0;

	uint32 RayGenShaderTableOffset = 0;
	uint32 MissShaderTableOffset = 0;
	uint32 HitGroupShaderTableOffset = 0;
	uint32 DefaultHitGroupShaderTableOffset = 0;

	// Note: TABLE_BYTE_ALIGNMENT is used instead of RECORD_BYTE_ALIGNMENT to allow arbitrary switching 
	// between multiple RayGen and Miss shaders within the same underlying table.
	static constexpr uint32 RayGenRecordStride = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
	static constexpr uint32 MissRecordStride = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

	uint32 HitGroupRecordSizeUnaligned = 0; // size of the shader identifier + local root parameters, not aligned to SHADER_RECORD_BYTE_ALIGNMENT (used for out-of-bounds access checks)
	uint32 HitGroupRecordStride = 0; // size of shader identifier + local root parameters, aligned to SHADER_RECORD_BYTE_ALIGNMENT
	TResourceArray<uint8, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT> Data;

#if DO_CHECK && DO_GUARD_SLOW
	TBitArray<FDefaultBitArrayAllocator> WrittenHitGroupRecords;
	uint32 NumValidHitGroupRecords = 0;
#endif // DO_CHECK

	bool bIsDirty = true;
	TRefCountPtr<FD3D12MemBuffer> Buffer;

	// SBTs have their own descriptor heaps
	FD3D12RayTracingDescriptorCache DescriptorCache;
};


template<typename ShaderType>
static FD3D12RayTracingShader* GetBuildInRayTracingShader()
{
	TShaderMap<FGlobalShaderType>* ShaderMap = GetGlobalShaderMap(ERHIFeatureLevel::SM5);
	auto Shader = ShaderMap->GetShader<ShaderType>();
	FD3D12RayTracingShader* RayTracingShader = static_cast<FD3D12RayTracingShader*>(Shader->GetRayTracingShader());
	return RayTracingShader;
}

template<typename ShaderType>
static void GetBuildInShaderLibrary(FDXILLibrary& ShaderLibrary)
{
	FD3D12RayTracingShader* RayTracingShader = GetBuildInRayTracingShader<ShaderType>();
	LPCWSTR EntryName[1] = { *RayTracingShader->EntryPoint };
	ShaderLibrary.InitFromDXIL(RayTracingShader->ShaderBytecode.GetShaderBytecode().pShaderBytecode, RayTracingShader->ShaderBytecode.GetShaderBytecode().BytecodeLength, EntryName, EntryName, 1);
}

void FD3D12Device::DestroyRayTracingDescriptorCache()
{
	delete RayTracingDescriptorHeapCache;
	RayTracingDescriptorHeapCache = nullptr;
}

class FD3D12RayTracingPipelineState : public FRHIRayTracingPipelineState
{
	FD3D12RayTracingPipelineState(FD3D12RayTracingPipelineState&&) = delete;
	FD3D12RayTracingPipelineState(const FD3D12RayTracingPipelineState&) = delete;
	FD3D12RayTracingPipelineState& operator=(const FD3D12RayTracingPipelineState&) = delete;
	FD3D12RayTracingPipelineState& operator=(FD3D12RayTracingPipelineState&&) = delete;

public:

	FD3D12RayTracingPipelineState(FD3D12Device* Device, const FRayTracingPipelineStateInitializer& Initializer)
		: DefaultShaderTable(Device)
		, DefaultLocalRootSignature(Device->GetParentAdapter())
	{
		FD3D12Adapter* Adapter = Device->GetParentAdapter();
		ID3D12Device5* RayTracingDevice = Device->GetRayTracingDevice();

		RayGenShader = FD3D12DynamicRHI::ResourceCast(Initializer.RayGenShaderRHI);

		LPCWSTR RayGenEntryName = *(RayGenShader->EntryPoint);
		RayGenShaderLibrary.InitFromDXIL(RayGenShader->ShaderBytecode, &RayGenEntryName, &RayGenEntryName, 1);

		const TArrayView<const FRayTracingHitGroupInitializer>& InitializerHitGroups = Initializer.GetHitGroups();

		TArray<ID3D12RootSignature*> LocalRootSignatures;
		LocalRootSignatures.Reserve(2 + InitializerHitGroups.Num()); // default empty signature + default hit group + one per custom hit group

		// Default empty local root signature

		const uint32 EmptyLocalRootSignatureIndex = LocalRootSignatures.Num();

		{
			D3D12_VERSIONED_ROOT_SIGNATURE_DESC LocalRootSignatureDesc = {};
			LocalRootSignatureDesc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
			LocalRootSignatureDesc.Desc_1_0.Flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
			DefaultLocalRootSignature.Init(LocalRootSignatureDesc);
			LocalRootSignatures.Add(DefaultLocalRootSignature.GetRootSignature());
		}

		// Miss shader

		LPCWSTR MissEntryName = nullptr;

		if (Initializer.MissShaderRHI)
		{
			MissShader = FD3D12DynamicRHI::ResourceCast(Initializer.MissShaderRHI);
			MissEntryName = *(MissShader->EntryPoint);
			MissShaderLibrary.InitFromDXIL(MissShader->ShaderBytecode, &MissEntryName, &MissEntryName, 1);
		}
		else
		{
			GetBuildInShaderLibrary<FDefaultMainMS>(MissShaderLibrary);
			MissEntryName = *(MissShaderLibrary.EntryNames[0]);
		}

		// Default closest hit shader

		LPCWSTR ClosestHitEntryName = nullptr;
		LPCWSTR DefaultHitGroupExportName = TEXT("DefaultHitGroup");

		MaxLocalRootSignatureSize = 0;

		const uint32 HitShaderRootSignatureBaseIndex = LocalRootSignatures.Num();
		if (Initializer.DefaultClosestHitShaderRHI)
		{
			ClosestHitShader = FD3D12DynamicRHI::ResourceCast(Initializer.DefaultClosestHitShaderRHI);
			ClosestHitEntryName = *(ClosestHitShader->EntryPoint);
			ClosestHitShaderLibrary.InitFromDXIL(ClosestHitShader->ShaderBytecode, &ClosestHitEntryName, &ClosestHitEntryName, 1);

			MaxLocalRootSignatureSize = FMath::Max(MaxLocalRootSignatureSize, ClosestHitShader->pRootSignature->GetTotalRootSignatureSizeInBytes());
			LocalRootSignatures.Add(ClosestHitShader->pRootSignature->GetRootSignature());
		}
		else
		{
			GetBuildInShaderLibrary<FDefaultMainCHS>(ClosestHitShaderLibrary);
			ClosestHitEntryName = *(ClosestHitShaderLibrary.EntryNames[0]);
		}

		// Initialize hit group shader libraries
		// #dxr_todo: hit group libraries could come from precompiled pipeline sub-objects
		HitGroupLibraries.Reserve(InitializerHitGroups.Num()); // #dxr_todo: reserve based on total number of shaders in the hit group list (each hit group can have up to 3)

		// Each shader within RTPSO must have a unique name, therefore we must rename original shader entry points.
		TArray<FString> RenamedHitGroupEntryPoints;
		RenamedHitGroupEntryPoints.Reserve(HitGroupLibraries.Num() * 3); // Up to 3 entry points may exist per hit group

		// Store root signatures per hit group bind shader root parameters later
		HitGroupShaders.Reserve(InitializerHitGroups.Num());

		struct FHitGroupEntryIndices
		{
			int32 ClosestHit   = INDEX_NONE;
			int32 AnyHit       = INDEX_NONE;
			int32 Intersection = INDEX_NONE;
		};

		TArray<FHitGroupEntryIndices> HitGroupEntryIndices;
		HitGroupEntryIndices.Reserve(InitializerHitGroups.Num());

		for (const FRayTracingHitGroupInitializer& HitGroupInitializer : InitializerHitGroups)
		{
			// #dxr_todo material library can have holes to keep the allocated indices stable and than those holes need to be filled with something, in this case the default.
			// this might hide bugs where the index was accidentally missing.
			FD3D12RayTracingShader* Shader = FD3D12DynamicRHI::ResourceCast(HitGroupInitializer.ShaderRHI ? HitGroupInitializer.ShaderRHI : Initializer.DefaultClosestHitShaderRHI);

			checkf(!Shader->ResourceCounts.bGlobalUniformBufferUsed, TEXT("Global uniform buffers are not implemented for ray tracing shaders"));

			HitGroupShaders.Add(Shader);

			MaxLocalRootSignatureSize = FMath::Max(MaxLocalRootSignatureSize, Shader->pRootSignature->GetTotalRootSignatureSizeInBytes());
			LocalRootSignatures.Add(Shader->pRootSignature->GetRootSignature());

			FDXILLibrary& Library = HitGroupLibraries.AddDefaulted_GetRef();

			LPCWSTR OriginalGroupEntryPoints[3];
			LPCWSTR RenamedGroupEntryPoints[3];
			uint32 NumGroupEntryPoints = 0;

			// #dxr_todo: A unique name for all ray tracing shaders could be auto-generated in the shader pipeline instead of run-time
			auto RegisterHitGroupEntry = 
				[&OriginalGroupEntryPoints, &RenamedGroupEntryPoints, &NumGroupEntryPoints, &RenamedHitGroupEntryPoints]
			(LPCWSTR EntryNameChars)
			{
				const int32 EntryIndex = RenamedHitGroupEntryPoints.Num();
				FString& RenamedEntryString = RenamedHitGroupEntryPoints.AddDefaulted_GetRef();
				RenamedEntryString = FString::Printf(TEXT("%s_%d"), EntryNameChars, EntryIndex);

				OriginalGroupEntryPoints[NumGroupEntryPoints] = EntryNameChars;
				RenamedGroupEntryPoints[NumGroupEntryPoints] = *RenamedEntryString;
				++NumGroupEntryPoints;

				return EntryIndex;
			};

			FHitGroupEntryIndices EntryIndices;

			EntryIndices.ClosestHit = RegisterHitGroupEntry(*(Shader->EntryPoint));

			if (!Shader->AnyHitEntryPoint.IsEmpty())
			{
				EntryIndices.AnyHit = RegisterHitGroupEntry(*(Shader->AnyHitEntryPoint));
			}

			if (!Shader->IntersectionEntryPoint.IsEmpty())
			{
				EntryIndices.Intersection = RegisterHitGroupEntry(*(Shader->IntersectionEntryPoint));
			}

			HitGroupEntryIndices.Add(EntryIndices);

			Library.InitFromDXIL(Shader->ShaderBytecode, OriginalGroupEntryPoints, RenamedGroupEntryPoints, NumGroupEntryPoints);
		}

		// Initialize StateObject

		{
			TArray<const FDXILLibrary*> Libraries;
			TArray<LPCWSTR> Exports;
			TArray<uint32> LocalRootSignatureAssociations;

			const int32 NumSystemShaders = 3; // Shaders that are guaranteed to be present in the RT pipeline (raygen, miss, default hit)

			// Reserve space for all custom hit groups + system shaders
			const int32 MaxNumShaders = HitGroupLibraries.Num() + NumSystemShaders;
			Libraries.Reserve(MaxNumShaders);

			const int32 MaxNumExports = RenamedHitGroupEntryPoints.Num() + NumSystemShaders;
			Exports.Reserve(MaxNumExports);
			LocalRootSignatureAssociations.Reserve(MaxNumExports); // One RS association per export

			// Ray generation shader

			Libraries.Add(&RayGenShaderLibrary);
			Exports.Add(RayGenEntryName);
			LocalRootSignatureAssociations.Add(EmptyLocalRootSignatureIndex); // RayGen shaders don't have parameters that come from SBT

			// Miss shader

			Libraries.Add(&MissShaderLibrary);
			Exports.Add(MissEntryName);
			LocalRootSignatureAssociations.Add(EmptyLocalRootSignatureIndex); // Miss shaders don't have parameters that come from SBT

			// Default custom hit shader

			uint32 CurrentHitGroupRootSignatureIndex = HitShaderRootSignatureBaseIndex;

			Libraries.Add(&ClosestHitShaderLibrary);
			Exports.Add(ClosestHitEntryName);

			if (Initializer.DefaultClosestHitShaderRHI)
			{
				// Add custom closest hit shader root signature association if it is provided
				LocalRootSignatureAssociations.Add(CurrentHitGroupRootSignatureIndex);
				++CurrentHitGroupRootSignatureIndex;
			}
			else
			{
				// Use default empty local root signature association if built-in closest hit shader is used
				LocalRootSignatureAssociations.Add(EmptyLocalRootSignatureIndex);
			}

			// Additional hit group shaders

			for (const FDXILLibrary& HitGroupLibrary : HitGroupLibraries)
			{
				Libraries.Add(&HitGroupLibrary);
				for (const FString& ExportName : HitGroupLibrary.ExportNames)
				{
					Exports.Add(*ExportName);

					// NOTE: Same local root signature is associated with all shaders in a hit group: closest hit, any hit and intersection (if they are present)
					LocalRootSignatureAssociations.Add(CurrentHitGroupRootSignatureIndex);
				}
				++CurrentHitGroupRootSignatureIndex;
			}

			// Hit groups

			TArray<D3D12_HIT_GROUP_DESC> HitGroups;
			TArray<FString> HitGroupNames;

			HitGroups.Reserve(1 + HitGroupLibraries.Num()); // N custom hit groups + 1 default (#dxr_todo: deprecate default hit shader and require explicit SBT binding at the high level)
			HitGroupNames.Reserve(HitGroups.Num());

			{
				// Add default hit group
				D3D12_HIT_GROUP_DESC HitGroup = {};
				HitGroup.HitGroupExport = DefaultHitGroupExportName;
				HitGroup.ClosestHitShaderImport = ClosestHitEntryName;
				HitGroups.Add(HitGroup);
			}

			for (const FRayTracingHitGroupInitializer& HitGroupInitializer : InitializerHitGroups)
			{
				const int32 HitGroupIndex = HitGroupNames.Num(); // #dxr_todo: this would need to be a unique index if we support pipeline sub-object linking
				FString& HitGroupName = HitGroupNames.AddDefaulted_GetRef();
				HitGroupName = FString::Printf(TEXT("HitGroup_%d"), HitGroupIndex);

				D3D12_HIT_GROUP_DESC HitGroup = {};
				HitGroup.HitGroupExport = *HitGroupName;

				const FHitGroupEntryIndices& EntryIndices = HitGroupEntryIndices[HitGroupIndex];

				HitGroup.ClosestHitShaderImport = *(RenamedHitGroupEntryPoints[EntryIndices.ClosestHit]);
				if (EntryIndices.AnyHit != INDEX_NONE)
				{
					HitGroup.AnyHitShaderImport = *(RenamedHitGroupEntryPoints[EntryIndices.AnyHit]);
				}
				if (EntryIndices.Intersection != INDEX_NONE)
				{
					HitGroup.IntersectionShaderImport = *(RenamedHitGroupEntryPoints[EntryIndices.Intersection]);
				}

				HitGroups.Add(HitGroup);
			}

			// Create the pipeline

			check(Libraries.Num() == MaxNumShaders); // Confirm that our memory reservation assumptions hold up
			check(Exports.Num() == MaxNumExports); // Confirm that our memory reservation assumptions hold up
			check(Exports.Num() == LocalRootSignatureAssociations.Num()); // Confirm that we have associated local root signatures with all shaders

			StateObject = CreateRayTracingStateObject(
				RayTracingDevice,
				Libraries,
				Exports,
				Initializer.MaxPayloadSizeInBytes,
				HitGroups,
				*RayGenShader->pRootSignature,
				LocalRootSignatures,
				LocalRootSignatureAssociations);

			VERIFYHRESULT(StateObject->QueryInterface(IID_PPV_ARGS(PipelineProperties.GetInitReference())));

			// Save hit group shader identifiers

			check(HitGroupNames.Num() == InitializerHitGroups.Num());

			HitGroupShaderIdentifiers.SetNumUninitialized(ShaderIdentifierSize * InitializerHitGroups.Num());
			for (int32 HitGroupIndex = 0; HitGroupIndex < HitGroupNames.Num(); ++HitGroupIndex)
			{
				LPCWSTR ExportNameChars = *HitGroupNames[HitGroupIndex];
				HitGroupShaderIdentifiers[HitGroupIndex] = GetShaderIdentifier(ExportNameChars);
			}
		}

		// Initialize shader identifiers and default ShaderTable

		RayGenShaderIdentifier     = GetShaderIdentifier(RayGenEntryName);
		MissShaderIdentifier       = GetShaderIdentifier(MissEntryName);
		DefaultHitGroupIdentifier  = GetShaderIdentifier(DefaultHitGroupExportName);

		const uint32 LocalRootDataSize = 0; // default hit shaders use an empty local root signature
		DefaultShaderTable.Init(1, 1, 0, LocalRootDataSize);

		DefaultShaderTable.SetRayGenIdentifier(0, RayGenShaderIdentifier);
		DefaultShaderTable.SetMissIdentifier(0, MissShaderIdentifier);
		DefaultShaderTable.SetDefaultHitGroupIdentifier(DefaultHitGroupIdentifier);

		DefaultShaderTable.CopyToGPU();

		DefaultDispatchDesc = DefaultShaderTable.GetDispatchRaysDesc();

		// Default dispatch desc refers to exactly one hit shader
		DefaultDispatchDesc.HitGroupTable.StartAddress = DefaultShaderTable.GetShaderTableAddress() + DefaultShaderTable.DefaultHitGroupShaderTableOffset;
		DefaultDispatchDesc.HitGroupTable.StrideInBytes = 0; // Zero stride effectively disables SBT indexing
		DefaultDispatchDesc.HitGroupTable.SizeInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT; // Minimal table with only one record
	}

	FD3D12ShaderIdentifier GetShaderIdentifier(LPCWSTR ExportName)
	{
		FD3D12ShaderIdentifier Result;

		const void* Data = PipelineProperties->GetShaderIdentifier(ExportName);
		checkf(Data, TEXT("Couldn't find requested export in the ray tracing shader pipeline"));

		if (Data)
		{
			check(sizeof(Result.Data) >= ShaderIdentifierSize);
			FMemory::Memcpy(Result.Data, Data, ShaderIdentifierSize);
		}

		return Result;
	}

	TRefCountPtr<FD3D12RayTracingShader> RayGenShader;
	FD3D12ShaderIdentifier RayGenShaderIdentifier;
	FDXILLibrary RayGenShaderLibrary;

	// #dxr_todo: deprecate default hit shader and require explicit SBT binding at the high level
	TRefCountPtr<FD3D12RayTracingShader> ClosestHitShader;
	FD3D12ShaderIdentifier ClosestHitShaderIdentifier;
	FDXILLibrary ClosestHitShaderLibrary;

	TRefCountPtr<FD3D12RayTracingShader> MissShader;
	FD3D12ShaderIdentifier MissShaderIdentifier;
	FDXILLibrary MissShaderLibrary;

	FD3D12ShaderIdentifier DefaultHitGroupIdentifier;
	TArray<FDXILLibrary> HitGroupLibraries;

	// Shader table that can be used to dispatch ray tracing work that doesn't require real SBT bindings.
	// This is useful for the case where user only provides default RayGen, Miss and HitGroup shaders.
	FD3D12RayTracingShaderTable DefaultShaderTable;

	// Default empty root signature used for default hit shaders.
	FD3D12RootSignature DefaultLocalRootSignature;

	TRefCountPtr<ID3D12StateObject> StateObject;
	TRefCountPtr<ID3D12StateObjectProperties> PipelineProperties;
	D3D12_DISPATCH_RAYS_DESC DefaultDispatchDesc = {};

	static constexpr uint32 ShaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	TArray<FD3D12ShaderIdentifier> HitGroupShaderIdentifiers;
	TArray<TRefCountPtr<FD3D12RayTracingShader>> HitGroupShaders;

	uint32 MaxLocalRootSignatureSize = 0;
};

class FD3D12BasicRayTracingPipeline
{
	FD3D12BasicRayTracingPipeline(FD3D12BasicRayTracingPipeline&&) = delete;
	FD3D12BasicRayTracingPipeline(const FD3D12BasicRayTracingPipeline&) = delete;
	FD3D12BasicRayTracingPipeline& operator=(const FD3D12BasicRayTracingPipeline&) = delete;
	FD3D12BasicRayTracingPipeline& operator=(FD3D12BasicRayTracingPipeline&&) = delete;

public:
	FD3D12BasicRayTracingPipeline(FD3D12Device* Device)
	{
		FRayTracingPipelineStateInitializer OcclusionInitializer;
		OcclusionInitializer.RayGenShaderRHI = GetBuildInRayTracingShader<FOcclusionMainRG>();
		OcclusionInitializer.MissShaderRHI = GetBuildInRayTracingShader<FOcclusionMainMS>();

		Occlusion = new FD3D12RayTracingPipelineState(Device, OcclusionInitializer);

		FRayTracingPipelineStateInitializer IntersectionInitializer;
		IntersectionInitializer.RayGenShaderRHI = GetBuildInRayTracingShader<FIntersectionMainRG>();
		IntersectionInitializer.MissShaderRHI = GetBuildInRayTracingShader<FIntersectionMainMS>();
		IntersectionInitializer.DefaultClosestHitShaderRHI = GetBuildInRayTracingShader<FIntersectionMainCHS>();

		Intersection = new FD3D12RayTracingPipelineState(Device, IntersectionInitializer);
	}

	~FD3D12BasicRayTracingPipeline()
	{
		delete Intersection;
		delete Occlusion;
	}

	FD3D12RayTracingPipelineState* Occlusion;
	FD3D12RayTracingPipelineState* Intersection;
};

void FD3D12Device::InitRayTracing()
{
	check(RayTracingDescriptorHeapCache == nullptr);
	RayTracingDescriptorHeapCache = new FD3D12RayTracingDescriptorHeapCache(this);

	check(BasicRayTracingPipeline == nullptr);
	BasicRayTracingPipeline = new FD3D12BasicRayTracingPipeline(this);
}

void FD3D12Device::CleanupRayTracing()
{
	delete BasicRayTracingPipeline;
	BasicRayTracingPipeline = nullptr;

	// Note: RayTracingDescriptorHeapCache is destroyed in ~FD3D12Device, after all deferred deletion is processed
}

FRayTracingPipelineStateRHIRef FD3D12DynamicRHI::RHICreateRayTracingPipelineState(const FRayTracingPipelineStateInitializer& Initializer)
{
	FD3D12RayTracingPipelineState* Result = new FD3D12RayTracingPipelineState(GetRHIDevice(), Initializer);

	return Result;
}

FRayTracingGeometryRHIRef FD3D12DynamicRHI::RHICreateRayTracingGeometry(const FRayTracingGeometryInitializer& Initializer)
{
	checkf(Initializer.PositionVertexBuffer, TEXT("Position vertex buffer is required for ray tracing geometry"));
	checkf(Initializer.VertexBufferStride, TEXT("Position vertex buffer is required for ray tracing geometry"));
	checkf(Initializer.VertexBufferStride % 4 == 0, TEXT("Position vertex buffer stride must be aligned to 4 bytes for ByteAddressBuffer loads to work"));

	// #dxr_todo VET_Half4 (DXGI_FORMAT_R16G16B16A16_FLOAT) is also supported by DXR. Should we support it?
	check(Initializer.VertexBufferElementType == VET_Float3 || Initializer.VertexBufferElementType == VET_Float2 || Initializer.VertexBufferElementType == VET_Half2);
	if (Initializer.IndexBuffer)
	{
		checkf(Initializer.IndexBuffer->GetStride() == 2 || Initializer.IndexBuffer->GetStride() == 4, TEXT("Index buffer must be 16 or 32 bit."));
	}

	checkf(Initializer.PrimitiveType == EPrimitiveType::PT_TriangleList, TEXT("Only TriangleList primitive type is currently supported."));

	// #dxr_todo: temporary constraints on vertex and index buffer formats (this will be relaxed when more flexible vertex/index fetching is implemented)
	checkf(Initializer.VertexBufferElementType == VET_Float3, TEXT("Only float3 vertex buffers are currently implemented.")); // #dxr_todo: support other vertex buffer formats
	checkf(Initializer.VertexBufferStride == 12, TEXT("Only deinterleaved float3 position vertex buffers are currently implemented.")); // #dxr_todo: support interleaved vertex buffers
	checkf(Initializer.BaseVertexIndex == 0, TEXT("BaseVertexIndex is not currently implemented")); // #dxr_todo: implement base vertex index for custom vertex fetch

	checkf(GNumExplicitGPUsForRendering == 1, TEXT("Ray tracing is not implemented for mGPU")); // #dxr_todo: implement mGPU support
	FD3D12RayTracingGeometry* Result = GetAdapter().CreateLinkedObject<FD3D12RayTracingGeometry>(FRHIGPUMask::All(), [&](FD3D12Device* Device)
	{
		FD3D12RayTracingGeometry* Mesh = new FD3D12RayTracingGeometry(Device);

		const uint32 GPUIndex = Device->GetGPUIndex();

		Mesh->IndexStride = Initializer.IndexBuffer ? Initializer.IndexBuffer->GetStride() : 0; // stride 0 means implicit triangle list for non-indexed geometry
		Mesh->VertexOffsetInBytes = (Initializer.BaseVertexIndex * Initializer.VertexBufferStride) + Initializer.VertexBufferByteOffset;
		Mesh->VertexStrideInBytes = Initializer.VertexBufferStride;
		Mesh->BaseVertexIndex = Initializer.BaseVertexIndex;
		Mesh->TotalPrimitiveCount = Initializer.TotalPrimitiveCount;

		if (Initializer.bFastBuild)
		{
			Mesh->BuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
		}
		else
		{
			Mesh->BuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		}
		if (Initializer.bAllowUpdate)
		{
			Mesh->BuildFlags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE;
		}


		if (Initializer.Segments.Num())
		{
			Mesh->Segments = TArray<FRayTracingGeometrySegment>(Initializer.Segments.GetData(), Initializer.Segments.Num());
		}
		else
		{
			FRayTracingGeometrySegment DefaultSegment;
			DefaultSegment.FirstPrimitive = 0;
			DefaultSegment.NumPrimitives = Initializer.TotalPrimitiveCount;
			Mesh->Segments.Add(DefaultSegment);
		}

#if DO_CHECK
		{
			uint32 ComputedPrimitiveCountForValidation = 0;
			for (const FRayTracingGeometrySegment& Segment : Mesh->Segments)
			{
				ComputedPrimitiveCountForValidation += Segment.NumPrimitives;
				check(Segment.FirstPrimitive + Segment.NumPrimitives <= Initializer.TotalPrimitiveCount);
			}
			check(ComputedPrimitiveCountForValidation == Initializer.TotalPrimitiveCount);
		}
#endif

		Mesh->VertexElemType = Initializer.VertexBufferElementType;

		Mesh->IndexBuffer = Initializer.IndexBuffer ? ResourceCast(Initializer.IndexBuffer.GetReference(), GPUIndex) : nullptr;
		Mesh->PositionVertexBuffer = ResourceCast(Initializer.PositionVertexBuffer.GetReference(), GPUIndex);

		Mesh->bIsAccelerationStructureDirty = true;

		return Mesh;
	});

	return Result;
}

FRayTracingSceneRHIRef FD3D12DynamicRHI::RHICreateRayTracingScene(const FRayTracingSceneInitializer& Initializer)
{
	FD3D12Adapter& Adapter = GetAdapter();

	return GetAdapter().CreateLinkedObject<FD3D12RayTracingScene>(FRHIGPUMask::All(), [&](FD3D12Device* Device)
	{
		FD3D12RayTracingScene* Result = new FD3D12RayTracingScene(Device);

		Result->Instances = TArray<FRayTracingGeometryInstance>(Initializer.Instances.GetData(), Initializer.Instances.Num());

		// Compute geometry segment count prefix sum to be later used in GetHitGroupIndex()
		Result->SegmentPrefixSum.Reserve(Result->Instances.Num());
		uint32 NumTotalSegments = 0;
		for (const FRayTracingGeometryInstance& Instance : Result->Instances)
		{
			FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(Instance.GeometryRHI);
			Result->SegmentPrefixSum.Add(NumTotalSegments);
			NumTotalSegments += Geometry->Segments.Num();
		}
		Result->NumTotalSegments = NumTotalSegments;

		return Result;
	});
}

FShaderResourceViewRHIParamRef FD3D12DynamicRHI::RHIGetAccelerationStructureShaderResourceView(FRayTracingSceneRHIParamRef InAccelerationStructure)
{
	return FD3D12DynamicRHI::ResourceCast(InAccelerationStructure)->AccelerationStructureView;
}

void FD3D12RayTracingGeometry::TransitionBuffers(FD3D12CommandContext& CommandContext)
{
	// Transition vertex and index resources..
	if (IndexBuffer && IndexBuffer.GetReference()->GetResource()->RequiresResourceStateTracking())
	{
		FD3D12DynamicRHI::TransitionResource(CommandContext.CommandListHandle, IndexBuffer.GetReference()->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0);
	}
	if (PositionVertexBuffer.GetReference()->GetResource()->RequiresResourceStateTracking())
	{
		FD3D12DynamicRHI::TransitionResource(CommandContext.CommandListHandle, PositionVertexBuffer.GetReference()->GetResource(), D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 0);
	}
}

static void CreateAccelerationStructureBuffers(TRefCountPtr<FD3D12MemBuffer>& AccelerationStructureBuffer, TRefCountPtr<FD3D12MemBuffer>&  ScratchBuffer, FD3D12Adapter* Adapter, uint32 GPUIndex, const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO& PrebuildInfo)
{
	FRHIResourceCreateInfo CreateInfo;

	D3D12_RESOURCE_DESC AccelerationStructureBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
		PrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	AccelerationStructureBuffer = Adapter->CreateRHIBuffer<FD3D12MemBuffer>(
		nullptr, AccelerationStructureBufferDesc, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT,
		0, AccelerationStructureBufferDesc.Width, BUF_AccelerationStructure, CreateInfo, FRHIGPUMask::FromIndex(GPUIndex));


	// #dxr_todo: scratch buffers can be pooled and reused for different scenes and geometries
	D3D12_RESOURCE_DESC ScratchBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
		FMath::Max(PrebuildInfo.UpdateScratchDataSizeInBytes, PrebuildInfo.ScratchDataSizeInBytes), D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);

	ScratchBuffer = Adapter->CreateRHIBuffer<FD3D12MemBuffer>(
		nullptr, ScratchBufferDesc, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT,
		0, ScratchBufferDesc.Width, BUF_UnorderedAccess, CreateInfo, FRHIGPUMask::FromIndex(GPUIndex));
}

void FD3D12RayTracingGeometry::BuildAccelerationStructure(FD3D12CommandContext& CommandContext, bool bIsUpdate)
{
	check(GNumExplicitGPUsForRendering == 1); // #dxr_todo: ensure that mGPU case is handled correctly!

	static constexpr uint32 IndicesPerPrimitive = 3; // Only triangle meshes are supported

	// Array of geometry descriptions, one per segment (single-segment geometry is a common case).
	TArray<D3D12_RAYTRACING_GEOMETRY_DESC, TInlineAllocator<1>> Descs;

	Descs.Reserve(Segments.Num());

	for (const FRayTracingGeometrySegment& Segment : Segments)
	{
		D3D12_RAYTRACING_GEOMETRY_DESC Desc = {};
		Desc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

		Desc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_NONE;

		if (!Segment.bAllowAnyHitShader)
		{
			// Deny anyhit shader invocations when this segment is hit
			Desc.Flags |= D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
		}

		if (!Segment.bAllowDuplicateAnyHitShaderInvocation)
		{
			// Allow only a single any-hit shader invocation per primitive
			Desc.Flags |= D3D12_RAYTRACING_GEOMETRY_FLAG_NO_DUPLICATE_ANYHIT_INVOCATION;
		}

		switch (VertexElemType)
		{
		case VET_Float3:
			Desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
			break;
		case VET_Float2:
			Desc.Triangles.VertexFormat = DXGI_FORMAT_R32G32_FLOAT;
			break;
		case VET_Half2:
			Desc.Triangles.VertexFormat = DXGI_FORMAT_R16G16_FLOAT;
			break;
		default:
			checkNoEntry();
			break;
		}

		Desc.Triangles.Transform3x4 = D3D12_GPU_VIRTUAL_ADDRESS(0);

		if (IndexBuffer)
		{
			Desc.Triangles.IndexFormat = IndexStride == 4 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
			Desc.Triangles.IndexCount = Segment.NumPrimitives * IndicesPerPrimitive;
			Desc.Triangles.IndexBuffer = IndexBuffer->ResourceLocation.GetGPUVirtualAddress() + IndexStride * Segment.FirstPrimitive * IndicesPerPrimitive;

			Desc.Triangles.VertexCount = PositionVertexBuffer->ResourceLocation.GetSize() / VertexStrideInBytes;
		}
		else
		{
			// Non-indexed geometry
			Desc.Triangles.IndexFormat = DXGI_FORMAT_UNKNOWN;
			Desc.Triangles.IndexCount = 0;
			Desc.Triangles.IndexBuffer = D3D12_GPU_VIRTUAL_ADDRESS(0);

			checkf(Segments.Num() == 1, TEXT("Non-indexed geometry with multiple segments is not implemented."));

			Desc.Triangles.VertexCount = FMath::Min<uint32>(PositionVertexBuffer->ResourceLocation.GetSize() / VertexStrideInBytes, TotalPrimitiveCount * 3);
		}

		Desc.Triangles.VertexBuffer.StartAddress = PositionVertexBuffer->ResourceLocation.GetGPUVirtualAddress() + VertexOffsetInBytes;
		Desc.Triangles.VertexBuffer.StrideInBytes = VertexStrideInBytes;

		Descs.Add(Desc);
	}

	checkf(GNumExplicitGPUsForRendering == 1, TEXT("Ray tracing is not implemented for mGPU")); // #dxr_todo: implement mGPU support

	const uint32 GPUIndex = CommandContext.GetGPUIndex(); // #dxr_todo: ensure that mGPU case is handled correctly!
	FD3D12Adapter* Adapter = CommandContext.GetParentAdapter();

	ID3D12Device5* RayTracingDevice = CommandContext.GetParentDevice()->GetRayTracingDevice();
	ID3D12GraphicsCommandList4* RayTracingCommandList = CommandContext.CommandListHandle.RayTracingCommandList();

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS LocalBuildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS(BuildFlags);

	if (bIsUpdate)
	{
		checkf(BuildFlags & D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_ALLOW_UPDATE,
			TEXT("Acceleration structure must be created with FRayTracingGeometryInitializer::bAllowUpdate=true to perform refit / update."));

		LocalBuildFlags |= D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PERFORM_UPDATE;
	}

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS PrebuildDescInputs = {};

	PrebuildDescInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	PrebuildDescInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	PrebuildDescInputs.NumDescs = Descs.Num();
	PrebuildDescInputs.pGeometryDescs = Descs.GetData();
	PrebuildDescInputs.Flags = LocalBuildFlags;

	if (!AccelerationStructureBuffer)
	{
		check(!bIsUpdate);

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo = {};

		RayTracingDevice->GetRaytracingAccelerationStructurePrebuildInfo(&PrebuildDescInputs, &PrebuildInfo);

		CreateAccelerationStructureBuffers(AccelerationStructureBuffer, ScratchBuffer, Adapter, GPUIndex, PrebuildInfo);

		// #dxr_todo: scratch buffers should be created in UAV state from the start
		FD3D12DynamicRHI::TransitionResource(CommandContext.CommandListHandle, ScratchBuffer.GetReference()->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0);
	}

	TransitionBuffers(CommandContext);
	CommandContext.CommandListHandle.FlushResourceBarriers();

	if (bIsAccelerationStructureDirty)
	{
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC BuildDesc = {};
		BuildDesc.Inputs = PrebuildDescInputs;
		BuildDesc.DestAccelerationStructureData = AccelerationStructureBuffer->ResourceLocation.GetGPUVirtualAddress();
		BuildDesc.ScratchAccelerationStructureData = ScratchBuffer->ResourceLocation.GetGPUVirtualAddress();
		BuildDesc.SourceAccelerationStructureData = bIsUpdate
			? AccelerationStructureBuffer->ResourceLocation.GetGPUVirtualAddress()
			: D3D12_GPU_VIRTUAL_ADDRESS(0);
		RayTracingCommandList->BuildRaytracingAccelerationStructure(&BuildDesc, 0, nullptr);
		bIsAccelerationStructureDirty = false;
	}
}

FD3D12RayTracingScene::~FD3D12RayTracingScene()
{
	for (auto Item : ShaderTables)
	{
		delete Item.Value;
	}
}

void FD3D12RayTracingScene::BuildAccelerationStructure(FD3D12CommandContext& CommandContext, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS BuildFlags)
{
	checkf(GNumExplicitGPUsForRendering == 1, TEXT("Ray tracing is not implemented for mGPU")); // #dxr_todo: implement mGPU support

	TRefCountPtr<FD3D12MemBuffer> InstanceBuffer;
	TRefCountPtr<FD3D12MemBuffer> ScratchBuffer;

	const uint32 GPUIndex = CommandContext.GetGPUIndex(); // #dxr_todo: ensure that mGPU case is handled correctly!
	FD3D12Adapter* Adapter = CommandContext.GetParentAdapter();
	ID3D12Device5* RayTracingDevice = CommandContext.GetParentDevice()->GetRayTracingDevice();
	ID3D12GraphicsCommandList4* RayTracingCommandList = CommandContext.CommandListHandle.RayTracingCommandList();

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS PrebuildDescInputs = {};

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo = {};
	PrebuildDescInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
	PrebuildDescInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	PrebuildDescInputs.NumDescs = Instances.Num();
	PrebuildDescInputs.Flags = BuildFlags;

	RayTracingDevice->GetRaytracingAccelerationStructurePrebuildInfo(&PrebuildDescInputs, &PrebuildInfo);

	CreateAccelerationStructureBuffers(AccelerationStructureBuffer, ScratchBuffer, Adapter, GPUIndex, PrebuildInfo);

	// #dxr_todo: scratch buffers should be created in UAV state from the start
	FD3D12DynamicRHI::TransitionResource(CommandContext.CommandListHandle, ScratchBuffer.GetReference()->GetResource(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0);

	{
		D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
		SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
		SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
		SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
		SRVDesc.RaytracingAccelerationStructure.Location = AccelerationStructureBuffer->ResourceLocation.GetGPUVirtualAddress();

		AccelerationStructureView = new FD3D12ShaderResourceView(
			AccelerationStructureBuffer->GetParentDevice(), 
			SRVDesc, AccelerationStructureBuffer->ResourceLocation, 4);
	}

	// Create and fill instance buffer

	if (Instances.Num())
	{
		FRHIResourceCreateInfo CreateInfo;
		D3D12_RESOURCE_DESC InstanceBufferDesc = CD3DX12_RESOURCE_DESC::Buffer(
			sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * Instances.Num(),
			D3D12_RESOURCE_FLAG_NONE, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT);

		// Create a temporary (volatile) buffer to hold instance data that we're about to upload.
		// The buffer does not need to persist for longer than one frame and can be discarded immediately
		// after the top level acceleration structure build is complete.
		InstanceBuffer = Adapter->CreateRHIBuffer<FD3D12MemBuffer>(
			nullptr, InstanceBufferDesc, D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT,
			0, InstanceBufferDesc.Width, BUF_Volatile, CreateInfo,
			FRHIGPUMask::FromIndex(GPUIndex));

		D3D12_RAYTRACING_INSTANCE_DESC* MappedData = (D3D12_RAYTRACING_INSTANCE_DESC*)Adapter->GetOwningRHI()->LockBuffer(
			nullptr, InstanceBuffer.GetReference(), 0, InstanceBufferDesc.Width, RLM_WriteOnly);

		check(MappedData);

		uint32 InstanceIndex = 0;
		for (const FRayTracingGeometryInstance& Instance : Instances)
		{
			FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(Instance.GeometryRHI);
			// #patrick todo: temporarily removing check in favor of on-demand BLAS build to diagnose an ordering issue.
			check(!Geometry->bIsAccelerationStructureDirty); // #dxr_todo: we could probably build BLAS here, if needed (though it may be best to have an explicit build API and just require things to be built at this point)

			D3D12_RAYTRACING_INSTANCE_DESC InstanceDesc = {};

			FMatrix TransformTransposed = Instance.Transform.GetTransposed();

			// Ensure the last row of the original Transform is <0,0,0,1>
			check((TransformTransposed.M[3][0] == 0)
				&& (TransformTransposed.M[3][1] == 0)
				&& (TransformTransposed.M[3][2] == 0)
				&& (TransformTransposed.M[3][3] == 1));

			FMemory::Memcpy(&InstanceDesc.Transform, &TransformTransposed.M[0][0], sizeof(InstanceDesc.Transform));

			InstanceDesc.InstanceID = Instance.UserData;
			InstanceDesc.InstanceMask = 0xFF; // Instance mask is currently unused
			InstanceDesc.InstanceContributionToHitGroupIndex = SegmentPrefixSum[InstanceIndex];
			InstanceDesc.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE; // #dxr_todo: convert cull mode based on instance mirroring or double-sidedness
			
			if (GRayTracingDebugForceOpaque)
			{
				InstanceDesc.Flags |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
			}

			if (GRayTracingDebugDisableTriangleCull)
			{
				InstanceDesc.Flags |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
			}

			InstanceDesc.AccelerationStructure = Geometry->AccelerationStructureBuffer->ResourceLocation.GetGPUVirtualAddress();

			MappedData[InstanceIndex] = InstanceDesc;
			++InstanceIndex;
		}

		Adapter->GetOwningRHI()->UnlockBuffer(nullptr, InstanceBuffer.GetReference());
	}

	// Build the actual acceleration structure

	const bool bIsUpdateMode = false; // #dxr_todo: we need an explicit public API to perform a refit/update

	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC BuildDesc = {};
	BuildDesc.Inputs = PrebuildDescInputs;
	BuildDesc.Inputs.InstanceDescs = InstanceBuffer ? InstanceBuffer->ResourceLocation.GetGPUVirtualAddress() : D3D12_GPU_VIRTUAL_ADDRESS(0);
	BuildDesc.DestAccelerationStructureData = AccelerationStructureBuffer->ResourceLocation.GetGPUVirtualAddress();
	BuildDesc.ScratchAccelerationStructureData = ScratchBuffer->ResourceLocation.GetGPUVirtualAddress();
	BuildDesc.SourceAccelerationStructureData = bIsUpdateMode
		? AccelerationStructureBuffer->ResourceLocation.GetGPUVirtualAddress()
		: D3D12_GPU_VIRTUAL_ADDRESS(0);

	// UAV barrier is used here to ensure that all bottom level acceleration structures are built
	CommandContext.CommandListHandle.AddUAVBarrier();
	CommandContext.CommandListHandle.FlushResourceBarriers();

	RayTracingCommandList->BuildRaytracingAccelerationStructure(&BuildDesc, 0, nullptr);

	// UAV barrier is used here to ensure that the acceleration structure build is complete before any rays are traced
	// #dxr_todo: these barriers should ideally be inserted by the high level code to allow more overlapped execution
	CommandContext.CommandListHandle.AddUAVBarrier();
}

FD3D12RayTracingShaderTable* FD3D12RayTracingScene::FindExistingShaderTable(const FD3D12RayTracingPipelineState* Pipeline) const
{
	FD3D12RayTracingShaderTable* const* FoundShaderTable = ShaderTables.Find(Pipeline);
	if (FoundShaderTable)
	{
		return *FoundShaderTable;
	}
	else
	{
		return nullptr;
	}
}

FD3D12RayTracingShaderTable* FD3D12RayTracingScene::FindOrCreateShaderTable(const FD3D12RayTracingPipelineState* Pipeline)
{
	FD3D12RayTracingShaderTable* FoundShaderTable = FindExistingShaderTable(Pipeline);
	if (FoundShaderTable)
	{
		return FoundShaderTable;
	}

	FD3D12RayTracingShaderTable* CreatedShaderTable = new FD3D12RayTracingShaderTable(GetParentDevice());
	ID3D12Device5* RayTracingDevice = GetParentDevice()->GetRayTracingDevice();

	// #dxr_todo: this needs to take into account multiple ray types (when those are supported)
	const uint32 NumHitGroupSlots = NumTotalSegments; // one hit group slot per geometry segment
	const uint32 LocalRootDataSize = Pipeline->MaxLocalRootSignatureSize;

	checkf(LocalRootDataSize >= sizeof(FHitGroupSystemParameters), TEXT("All local root signatures are expected to contain ray tracing system root parameters (2x root buffers + 1x root DWORD)"));

	CreatedShaderTable->Init(1, 1, NumHitGroupSlots, LocalRootDataSize);

	CreatedShaderTable->SetRayGenIdentifier(0, Pipeline->RayGenShaderIdentifier);
	CreatedShaderTable->SetMissIdentifier(0, Pipeline->MissShaderIdentifier);
	CreatedShaderTable->SetDefaultHitGroupIdentifier(Pipeline->DefaultHitGroupIdentifier);

	// Bind default hit group, index/vertex buffers and fetch parameters to all SBT entries (all segments of all mesh instances)

	const uint32 NumInstances = Instances.Num();
	for (uint32 InstanceIndex = 0; InstanceIndex < NumInstances; ++InstanceIndex)
	{
		const FRayTracingGeometryInstance& Instance = Instances[InstanceIndex];

		const FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(Instance.GeometryRHI);

		static constexpr uint32 IndicesPerPrimitive = 3; // Only triangle meshes are supported

		const uint32 IndexStride = Geometry->IndexStride;
		const D3D12_GPU_VIRTUAL_ADDRESS IndexBufferAddress = Geometry->IndexBuffer ? Geometry->IndexBuffer->ResourceLocation.GetGPUVirtualAddress() : 0;
		const D3D12_GPU_VIRTUAL_ADDRESS VertexBufferAddress = Geometry->PositionVertexBuffer->ResourceLocation.GetGPUVirtualAddress() + Geometry->VertexOffsetInBytes;

		const uint32 NumSegments = Geometry->Segments.Num();
		for (uint32 SegmentIndex = 0; SegmentIndex < NumSegments; ++SegmentIndex)
		{
			const FRayTracingGeometrySegment& Segment = Geometry->Segments[SegmentIndex];

			const uint32 RecordIndex = GetHitGroupIndex(InstanceIndex, SegmentIndex);
			CreatedShaderTable->SetHitGroupIdentifier(RecordIndex, Pipeline->DefaultHitGroupIdentifier);

			FHitGroupSystemParameters RootParameters = {};
			RootParameters.IndexBuffer = IndexBufferAddress;
			RootParameters.VertexBuffer = VertexBufferAddress;

			// #dxr_todo: support various vertex buffer layouts (fetch/decode based on vertex stride and format)
			checkf(Geometry->VertexElemType == VET_Float3, TEXT("Only VET_Float3 is currently implemented and tested. Other formats will be supported in the future."));
			RootParameters.FetchParameters.SetVertexAndIndexStride(Geometry->VertexStrideInBytes, IndexStride);
			RootParameters.FetchParameters.IndexBufferOffsetInBytes = IndexStride * Segment.FirstPrimitive * IndicesPerPrimitive;

			CreatedShaderTable->SetHitGroupParameters(RecordIndex, 0, RootParameters);
		}
	}

	ShaderTables.Add(Pipeline, CreatedShaderTable);

	return CreatedShaderTable;
}

void FD3D12CommandContext::RHIBuildAccelerationStructure(FRayTracingGeometryRHIParamRef InGeometry)
{
	FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(InGeometry);
	Geometry->TransitionBuffers(*this);
	CommandListHandle.FlushResourceBarriers();

	const bool bIsUpdate = false;
	Geometry->BuildAccelerationStructure(*this, bIsUpdate);
}

void FD3D12CommandContext::RHIUpdateAccelerationStructures(const TArrayView<const FAccelerationStructureUpdateParams> Params)
{
	// First batch up all barriers
	for (const FAccelerationStructureUpdateParams P : Params)
	{
		FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(P.Geometry);
		Geometry->PositionVertexBuffer = FD3D12DynamicRHI::ResourceCast(P.VertexBuffer);
		Geometry->TransitionBuffers(*this);
	}
	CommandListHandle.FlushResourceBarriers();

	// Then do all work
	for (const FAccelerationStructureUpdateParams P : Params)
	{
		FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(P.Geometry);
		Geometry->bIsAccelerationStructureDirty = true;

		const bool bIsUpdate = true;
		Geometry->BuildAccelerationStructure(*this, bIsUpdate);
	}
}

void FD3D12CommandContext::RHIBuildAccelerationStructures(const TArrayView<const FAccelerationStructureUpdateParams> Params)
{
	// First batch up all barriers
	for (const FAccelerationStructureUpdateParams P : Params)
	{
		FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(P.Geometry);
		Geometry->PositionVertexBuffer = FD3D12DynamicRHI::ResourceCast(P.VertexBuffer);
		Geometry->TransitionBuffers(*this);
	}
	CommandListHandle.FlushResourceBarriers();

	// Then do all work
	for (const FAccelerationStructureUpdateParams P : Params)
	{
		FD3D12RayTracingGeometry* Geometry = FD3D12DynamicRHI::ResourceCast(P.Geometry);
		Geometry->bIsAccelerationStructureDirty = true;

		const bool bIsUpdate = false;
		Geometry->BuildAccelerationStructure(*this, bIsUpdate);
	}
}

void FD3D12CommandContext::RHIBuildAccelerationStructure(FRayTracingSceneRHIParamRef InScene)
{
	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(InScene);
	Scene->BuildAccelerationStructure(*this, D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE);
}


struct FD3D12RayTracingGlobalResourceBinder
{
	FD3D12RayTracingGlobalResourceBinder(FD3D12CommandContext& InCommandContext)
		: CommandContext(InCommandContext)
	{
	}

	void SetRootCBV(uint32 BaseSlotIndex, uint32 DescriptorIndex, D3D12_GPU_VIRTUAL_ADDRESS Address)
	{
		CommandContext.CommandListHandle->SetComputeRootConstantBufferView(BaseSlotIndex + DescriptorIndex, Address);
	}

	void SetRootSRV(uint32 BaseSlotIndex, uint32 DescriptorIndex, D3D12_GPU_VIRTUAL_ADDRESS Address)
	{
		CommandContext.CommandListHandle->SetComputeRootShaderResourceView(BaseSlotIndex + DescriptorIndex, Address);
	}

	void SetRootDescriptorTable(uint32 SlotIndex, D3D12_GPU_DESCRIPTOR_HANDLE DescriptorTable)
	{
		CommandContext.CommandListHandle->SetComputeRootDescriptorTable(SlotIndex, DescriptorTable);
	}

	FD3D12CommandContext& CommandContext;
};

struct FD3D12RayTracingLocalResourceBinder
{
	FD3D12RayTracingLocalResourceBinder(FD3D12CommandContext& InCommandContext, FD3D12RayTracingShaderTable* InShaderTable, const FD3D12RootSignature* InRootSignature, uint32 InRecordIndex)
		: ShaderTable(InShaderTable)
		, RootSignature(InRootSignature)
		, RecordIndex(InRecordIndex)
	{
		check(RecordIndex != ~0u);
	}

	void SetRootDescriptor(uint32 BaseSlotIndex, uint32 DescriptorIndex, D3D12_GPU_VIRTUAL_ADDRESS Address)
	{
		const uint32 BindOffsetBase = RootSignature->GetBindSlotOffsetInBytes(BaseSlotIndex);
		const uint32 DescriptorSize = uint32(sizeof(D3D12_GPU_VIRTUAL_ADDRESS));
		const uint32 CurrentOffset = BindOffsetBase + DescriptorIndex * DescriptorSize;
		ShaderTable->SetHitGroupParameters(RecordIndex, CurrentOffset, Address);
	}

	void SetRootCBV(uint32 BaseSlotIndex, uint32 DescriptorIndex, D3D12_GPU_VIRTUAL_ADDRESS Address)
	{
		SetRootDescriptor(BaseSlotIndex, DescriptorIndex, Address);
	}

	void SetRootSRV(uint32 BaseSlotIndex, uint32 DescriptorIndex, D3D12_GPU_VIRTUAL_ADDRESS Address)
	{
		SetRootDescriptor(BaseSlotIndex, DescriptorIndex, Address);
	}

	void SetRootDescriptorTable(uint32 SlotIndex, D3D12_GPU_DESCRIPTOR_HANDLE DescriptorTable)
	{
		const uint32 BindOffset = RootSignature->GetBindSlotOffsetInBytes(SlotIndex);
		ShaderTable->SetHitGroupParameters(RecordIndex, BindOffset, DescriptorTable);
	}

	FD3D12RayTracingShaderTable* ShaderTable = nullptr;
	const FD3D12RootSignature* RootSignature = nullptr;
	uint32 RecordIndex = ~0u;
};

template <typename ResourceBinderType>
static void SetRayTracingShaderResources(
	FD3D12CommandContext& CommandContext,
	const FD3D12RayTracingShader* Shader,
	const FRayTracingShaderBindings& ResourceBindings,
	FD3D12RayTracingDescriptorCache& DescriptorCache,
	ResourceBinderType& Binder)
{
	ID3D12Device* Device = CommandContext.GetParentDevice()->GetDevice();

	const FD3D12RootSignature* RootSignature = Shader->pRootSignature;

	FD3D12UniformBuffer*        LocalCBVs[MAX_CBS];
	D3D12_CPU_DESCRIPTOR_HANDLE LocalSRVs[MAX_SRVS];
	D3D12_CPU_DESCRIPTOR_HANDLE LocalUAVs[MAX_UAVS];
	D3D12_CPU_DESCRIPTOR_HANDLE LocalSamplers[MAX_SAMPLERS];

	uint64 BoundSRVMask = 0;
	uint64 BoundCBVMask = 0;
	uint64 BoundUAVMask = 0;
	uint64 BoundSamplerMask = 0;

	for (uint32 SRVIndex = 0; SRVIndex < ARRAY_COUNT(ResourceBindings.Textures); ++SRVIndex)
	{
		FTextureRHIParamRef Resource = ResourceBindings.Textures[SRVIndex];
		if (Resource)
		{
			LocalSRVs[SRVIndex] = CommandContext.RetrieveTextureBase(Resource)->GetShaderResourceView()->GetView();
			BoundSRVMask |= 1ull << SRVIndex;
		}
	}

	for (uint32 SRVIndex = 0; SRVIndex < ARRAY_COUNT(ResourceBindings.SRVs); ++SRVIndex)
	{
		FShaderResourceViewRHIParamRef Resource = ResourceBindings.SRVs[SRVIndex];
		if (Resource)
		{
			LocalSRVs[SRVIndex] = FD3D12DynamicRHI::ResourceCast(Resource)->GetView();
			BoundSRVMask |= 1ull << SRVIndex;
		}
	}

	for (uint32 CBVIndex = 0; CBVIndex < ARRAY_COUNT(ResourceBindings.UniformBuffers); ++CBVIndex)
	{
		FUniformBufferRHIParamRef Resource = ResourceBindings.UniformBuffers[CBVIndex];
		if (Resource)
		{
			LocalCBVs[CBVIndex] = FD3D12DynamicRHI::ResourceCast(Resource);
			BoundCBVMask |= 1ull << CBVIndex;
		}
	}

	for (uint32 SamplerIndex = 0; SamplerIndex < ARRAY_COUNT(ResourceBindings.Samplers); ++SamplerIndex)
	{
		FSamplerStateRHIParamRef Resource = ResourceBindings.Samplers[SamplerIndex];
		if (Resource)
		{
			LocalSamplers[SamplerIndex] = FD3D12DynamicRHI::ResourceCast(Resource)->Descriptor;
			BoundSamplerMask |= 1ull << SamplerIndex;
		}
	}

	for (uint32 UAVIndex = 0; UAVIndex < ARRAY_COUNT(ResourceBindings.UAVs); ++UAVIndex)
	{
		FUnorderedAccessViewRHIParamRef Resource = ResourceBindings.UAVs[UAVIndex];
		if (Resource)
		{
			LocalUAVs[UAVIndex] = FD3D12DynamicRHI::ResourceCast(Resource)->GetView();
			BoundUAVMask |= 1ull << UAVIndex;
		}
	}

	const FD3D12ShaderResourceTable& ShaderResourceTable = Shader->ShaderResourceTable;

	uint32 DirtyBits = ShaderResourceTable.ResourceTableBits;

	while (DirtyBits)
	{
		// Scan for the lowest set bit, compute its index, clear it in the set of dirty bits.
		const uint32 LowestBitMask = (DirtyBits)& (-(int32)DirtyBits);
		const int32 BufferIndex = FMath::FloorLog2(LowestBitMask); // todo: This has a branch on zero, we know it could never be zero...
		DirtyBits ^= LowestBitMask;
		check(BufferIndex < ARRAY_COUNT(ResourceBindings.UniformBuffers));
		FD3D12UniformBuffer* Buffer = FD3D12DynamicRHI::ResourceCast(ResourceBindings.UniformBuffers[BufferIndex]);
		check(Buffer);
		check(BufferIndex < ShaderResourceTable.ResourceTableLayoutHashes.Num());
		check(Buffer->GetLayout().GetHash() == ShaderResourceTable.ResourceTableLayoutHashes[BufferIndex]);

		// #dxr_todo: could implement all 3 loops using a common template function (and ideally share this with regular dx12 rhi code)

		// Textures

		{
			const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
			const TArray<uint32>& ResourceMap = ShaderResourceTable.TextureMap;
			const uint32 BufferOffset = ResourceMap[BufferIndex];
			if (BufferOffset > 0)
			{
				const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
				uint32 ResourceInfo = *ResourceInfos++;
				do
				{
					checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
					const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
					const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

					FD3D12ShaderResourceView* SRV = CommandContext.RetrieveTextureBase((FRHITexture*)Resources[ResourceIndex].GetReference())->GetShaderResourceView();
					LocalSRVs[BindIndex] = SRV->GetView();
					BoundSRVMask |= 1ull << BindIndex;

					ResourceInfo = *ResourceInfos++;
				} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			}
		}

		// SRVs

		{
			const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
			const TArray<uint32>& ResourceMap = ShaderResourceTable.ShaderResourceViewMap;
			const uint32 BufferOffset = ResourceMap[BufferIndex];
			if (BufferOffset > 0)
			{
				const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
				uint32 ResourceInfo = *ResourceInfos++;
				do
				{
					checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
					const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
					const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

					FD3D12ShaderResourceView* SRV = CommandContext.RetrieveObject<FD3D12ShaderResourceView>((FRHIShaderResourceView*)(Resources[ResourceIndex].GetReference()));
					LocalSRVs[BindIndex] = SRV->GetView();
					BoundSRVMask |= 1ull << BindIndex;

					ResourceInfo = *ResourceInfos++;
				} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			}
		}

		// Samplers

		{
			const TRefCountPtr<FRHIResource>* RESTRICT Resources = Buffer->ResourceTable.GetData();
			const TArray<uint32>& ResourceMap = ShaderResourceTable.SamplerMap;
			const uint32 BufferOffset = ResourceMap[BufferIndex];
			if (BufferOffset > 0)
			{
				const uint32* RESTRICT ResourceInfos = &ResourceMap[BufferOffset];
				uint32 ResourceInfo = *ResourceInfos++;
				do
				{
					checkSlow(FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
					const uint16 ResourceIndex = FRHIResourceTableEntry::GetResourceIndex(ResourceInfo);
					const uint8 BindIndex = FRHIResourceTableEntry::GetBindIndex(ResourceInfo);

					FD3D12SamplerState* Sampler = CommandContext.RetrieveObject<FD3D12SamplerState>((FRHISamplerState*)(Resources[ResourceIndex].GetReference()));
					LocalSamplers[BindIndex] = Sampler->Descriptor;
					BoundSamplerMask |= 1ull << BindIndex;

					ResourceInfo = *ResourceInfos++;
				} while (FRHIResourceTableEntry::GetUniformBufferIndex(ResourceInfo) == BufferIndex);
			}
		}
	}

	auto IsCompleteBinding = [](uint32 ExpectedCount, uint64 BoundMask)
	{
		if (ExpectedCount > 64) return false; // Bound resource mask can't be represented by uint64

		// All bits of the mask [0..ExpectedCount) are expected to be set
		uint64 ExpectedMask = ExpectedCount == 64 ? ~0ull : ((1ull << ExpectedCount) - 1);
		return (ExpectedMask & BoundMask) == ExpectedMask;
	};
	check(IsCompleteBinding(Shader->ResourceCounts.NumSRVs, BoundSRVMask));
	check(IsCompleteBinding(Shader->ResourceCounts.NumUAVs, BoundUAVMask));
	check(IsCompleteBinding(Shader->ResourceCounts.NumCBs, BoundCBVMask));
	check(IsCompleteBinding(Shader->ResourceCounts.NumSamplers, BoundSamplerMask));

	const uint32 NumSRVs = Shader->ResourceCounts.NumSRVs;
	if (NumSRVs)
	{
		const uint32 DescriptorTableBaseIndex = DescriptorCache.GetDescriptorTableBaseIndex(LocalSRVs, NumSRVs, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		const uint32 BindSlot = RootSignature->SRVRDTBindSlot(SF_Compute);
		check(BindSlot != 0xFF);

		const D3D12_GPU_DESCRIPTOR_HANDLE ResourceDescriptorTableBaseGPU = DescriptorCache.ViewHeap.GetDescriptorGPU(DescriptorTableBaseIndex);
		Binder.SetRootDescriptorTable(BindSlot, ResourceDescriptorTableBaseGPU);
	}

	const uint32 NumUAVs = Shader->ResourceCounts.NumUAVs;
	if (NumUAVs)
	{
		const uint32 DescriptorTableBaseIndex = DescriptorCache.GetDescriptorTableBaseIndex(LocalUAVs, NumUAVs, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		const uint32 BindSlot = RootSignature->UAVRDTBindSlot(SF_Compute);
		check(BindSlot != 0xFF);

		const D3D12_GPU_DESCRIPTOR_HANDLE ResourceDescriptorTableBaseGPU = DescriptorCache.ViewHeap.GetDescriptorGPU(DescriptorTableBaseIndex);
		Binder.SetRootDescriptorTable(BindSlot, ResourceDescriptorTableBaseGPU);
	}

	if (Shader->ResourceCounts.NumCBs)
	{
		// #dxr_todo: make sure that root signature only uses root CBVs (this is currently checked in D3D12RootSignature.cpp)

		const uint32 BindSlot = RootSignature->CBVRDBaseBindSlot(SF_Compute);
		check(BindSlot != 0xFF);

		for (uint32 i = 0; i < Shader->ResourceCounts.NumCBs; ++i)
		{
			D3D12_GPU_VIRTUAL_ADDRESS BufferAddress = LocalCBVs[i]->ResourceLocation.GetGPUVirtualAddress();
			Binder.SetRootCBV(BindSlot, i, BufferAddress);
		}
	}

	// Bind samplers

	const uint32 NumSamplers = Shader->ResourceCounts.NumSamplers;
	if (NumSamplers)
	{
		const uint32 DescriptorTableBaseIndex = DescriptorCache.GetDescriptorTableBaseIndex(LocalSamplers, NumSamplers, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

		const uint32 BindSlot = RootSignature->SamplerRDTBindSlot(SF_Compute);
		check(BindSlot != 0xFF);

		const D3D12_GPU_DESCRIPTOR_HANDLE ResourceDescriptorTableBaseGPU = DescriptorCache.SamplerHeap.GetDescriptorGPU(DescriptorTableBaseIndex);
		Binder.SetRootDescriptorTable(BindSlot, ResourceDescriptorTableBaseGPU);
	}
}

static void DispatchRays(FD3D12CommandContext& CommandContext,
	const FRayTracingShaderBindings& GlobalBindings,
	const FD3D12RayTracingPipelineState* Pipeline,
	FD3D12RayTracingShaderTable* OptShaderTable,
	const D3D12_DISPATCH_RAYS_DESC& DispatchDesc)
{
	// Setup state for RT dispatch

	ID3D12GraphicsCommandList* CommandList = CommandContext.CommandListHandle.GraphicsCommandList();
	ID3D12GraphicsCommandList4* RayTracingCommandList = CommandContext.CommandListHandle.RayTracingCommandList();

	// #dxr_todo: RT and non-RT descriptors should use the same global heap that's dynamically sub-allocated.
	// This requires a major refactor of descriptor heap management. In the short term, RT work uses a dedicated heap
	// that's temporarily set for the duration of RT dispatch.
	ID3D12DescriptorHeap* PreviousHeaps[2] =
	{
		CommandContext.StateCache.GetDescriptorCache()->GetCurrentViewHeap()->GetHeap(),
		CommandContext.StateCache.GetDescriptorCache()->GetCurrentSamplerHeap()->GetHeap(),
	};

	// Invalidate state cache to ensure all root parameters for regular shaders are reset when non-RT work is dispatched later.
	CommandContext.StateCache.TransitionComputeState(D3D12PT_RayTracing);

	CommandList->SetComputeRootSignature(Pipeline->RayGenShader->pRootSignature->GetRootSignature());

	if (OptShaderTable)
	{
		OptShaderTable->DescriptorCache.SetDescriptorHeaps(CommandContext);
		FD3D12RayTracingGlobalResourceBinder ResourceBinder(CommandContext);
		SetRayTracingShaderResources(CommandContext, Pipeline->RayGenShader, GlobalBindings, OptShaderTable->DescriptorCache, ResourceBinder);
	}
	else
	{
		FD3D12RayTracingDescriptorCache TransientDescriptorCache(CommandContext.GetParentDevice());
		TransientDescriptorCache.Init(MAX_SRVS + MAX_UAVS, MAX_SAMPLERS);
		TransientDescriptorCache.SetDescriptorHeaps(CommandContext);
		FD3D12RayTracingGlobalResourceBinder ResourceBinder(CommandContext);
		SetRayTracingShaderResources(CommandContext, Pipeline->RayGenShader, GlobalBindings, TransientDescriptorCache, ResourceBinder);
	}

	CommandContext.CommandListHandle.FlushResourceBarriers();

	ID3D12StateObject* RayTracingStateObject = Pipeline->StateObject.GetReference();
	RayTracingCommandList->SetPipelineState1(RayTracingStateObject);
	RayTracingCommandList->DispatchRays(&DispatchDesc);

	if (CommandContext.IsDefaultContext())
	{
		CommandContext.GetParentDevice()->RegisterGPUWork(1);
	}

	// Restore old global descriptor heaps
	CommandList->SetDescriptorHeaps(2, PreviousHeaps);
}


void FD3D12CommandContext::RHIRayTraceOcclusion(FRayTracingSceneRHIParamRef InScene,
	FShaderResourceViewRHIParamRef InRays,
	FUnorderedAccessViewRHIParamRef InOutput,
	uint32 NumRays)
{
	checkf(GetParentDevice()->GetBasicRayTracingPipeline(), TEXT("Ray tracing support is not initialized for this device. Ensure that InitRayTracing() is called before issuing any ray tracing work."));

	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(InScene);
	FD3D12ShaderResourceView* Rays = FD3D12DynamicRHI::ResourceCast(InRays);
	FD3D12UnorderedAccessView* Output = FD3D12DynamicRHI::ResourceCast(InOutput);

	const FD3D12RayTracingPipelineState* Pipeline = GetParentDevice()->GetBasicRayTracingPipeline()->Occlusion;
	const FD3D12RayTracingShaderTable& ShaderTable = Pipeline->DefaultShaderTable;

	ID3D12GraphicsCommandList4* RayTracingCommandList = CommandListHandle.RayTracingCommandList();

	D3D12_GPU_VIRTUAL_ADDRESS ShaderTableAddress = ShaderTable.Buffer->ResourceLocation.GetGPUVirtualAddress();

	D3D12_DISPATCH_RAYS_DESC DispatchDesc = ShaderTable.GetDispatchRaysDesc();

	DispatchDesc.Width = NumRays;
	DispatchDesc.Height = 1;
	DispatchDesc.Depth = 1;

	FRayTracingShaderBindings Bindings;
	Bindings.SRVs[0] = Scene->AccelerationStructureView.GetReference();
	Bindings.SRVs[1] = Rays;
	Bindings.UAVs[0] = Output;

	DispatchRays(*this, Bindings, Pipeline, nullptr, DispatchDesc);
}

void FD3D12CommandContext::RHIRayTraceIntersection(FRayTracingSceneRHIParamRef InScene,
	FShaderResourceViewRHIParamRef InRays,
	FUnorderedAccessViewRHIParamRef InOutput,
	uint32 NumRays)
{
	checkf(GetParentDevice()->GetBasicRayTracingPipeline(), TEXT("Ray tracing support is not initialized for this device. Ensure that InitRayTracing() is called before issuing any ray tracing work."));

	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(InScene);
	FD3D12ShaderResourceView* Rays = FD3D12DynamicRHI::ResourceCast(InRays);
	FD3D12UnorderedAccessView* Output = FD3D12DynamicRHI::ResourceCast(InOutput);

	const FD3D12RayTracingPipelineState* Pipeline = GetParentDevice()->GetBasicRayTracingPipeline()->Intersection;
	const FD3D12RayTracingShaderTable& ShaderTable = Pipeline->DefaultShaderTable;

	ID3D12GraphicsCommandList4* RayTracingCommandList = CommandListHandle.RayTracingCommandList();

	D3D12_GPU_VIRTUAL_ADDRESS ShaderTableAddress = ShaderTable.Buffer->ResourceLocation.GetGPUVirtualAddress();

	D3D12_DISPATCH_RAYS_DESC DispatchDesc = ShaderTable.GetDispatchRaysDesc();

	DispatchDesc.HitGroupTable.StartAddress = ShaderTableAddress + ShaderTable.DefaultHitGroupShaderTableOffset;
	DispatchDesc.HitGroupTable.StrideInBytes = 0; // Zero stride effectively disables SBT indexing
	DispatchDesc.HitGroupTable.SizeInBytes = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT; // Minimal table with only one record

	DispatchDesc.Width = NumRays;
	DispatchDesc.Height = 1;
	DispatchDesc.Depth = 1;

	FRayTracingShaderBindings Bindings;
	Bindings.SRVs[0] = Scene->AccelerationStructureView.GetReference();
	Bindings.SRVs[1] = Rays;
	// #dxr_todo: intersection and occlusion shaders should be split into separate files to avoid resource slot collisions.
	// Workaround for now is to bind a valid UAV to slots 0 and 1, even though only slot 1 is referenced.
	Bindings.UAVs[0] = Output;
	Bindings.UAVs[1] = Output;

	DispatchRays(*this, Bindings, Pipeline, nullptr, DispatchDesc);
}

void FD3D12CommandContext::RHIRayTraceDispatch(FRayTracingPipelineStateRHIParamRef InRayTracingPipelineState,
	FRayTracingSceneRHIParamRef InScene, // #dxr_todo: replace this with explicit shader table parameter
	const FRayTracingShaderBindings& GlobalResourceBindings,
	uint32 Width, uint32 Height)
{
	const FD3D12RayTracingPipelineState* Pipeline = FD3D12DynamicRHI::ResourceCast(InRayTracingPipelineState);
	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(InScene);

	FD3D12RayTracingShaderTable* ShaderTable = Scene->FindOrCreateShaderTable(Pipeline);

	// #dxr_todo: shader table update should be an explicit operation and this code should only perform validation.
	if (ShaderTable->bIsDirty)
	{
		ShaderTable->CopyToGPU();
	}

	D3D12_DISPATCH_RAYS_DESC DispatchDesc = ShaderTable->GetDispatchRaysDesc();

	DispatchDesc.Width = Width;
	DispatchDesc.Height = Height;
	DispatchDesc.Depth = 1;

	DispatchRays(*this, GlobalResourceBindings, Pipeline, ShaderTable, DispatchDesc);
}

void FD3D12CommandContext::RHIRayTraceDispatch(FRayTracingPipelineStateRHIParamRef InRayTracingPipelineState,
	const FRayTracingShaderBindings& GlobalResourceBindings,
	uint32 Width, uint32 Height)
{
	const FD3D12RayTracingPipelineState* Pipeline = FD3D12DynamicRHI::ResourceCast(InRayTracingPipelineState);

	D3D12_DISPATCH_RAYS_DESC DispatchDesc = Pipeline->DefaultDispatchDesc;

	DispatchDesc.Width = Width;
	DispatchDesc.Height = Height;
	DispatchDesc.Depth = 1;

	DispatchRays(*this, GlobalResourceBindings, Pipeline, nullptr, DispatchDesc);
}

void FD3D12CommandContext::RHISetRayTracingHitGroup(
	FRayTracingSceneRHIParamRef InScene, uint32 InstanceIndex, uint32 SegmentIndex,
	FRayTracingPipelineStateRHIParamRef InPipeline, uint32 HitGroupIndex,
	const FRayTracingShaderBindings& ResourceBindings)
{
	FD3D12RayTracingScene* Scene = FD3D12DynamicRHI::ResourceCast(InScene);
	FD3D12RayTracingPipelineState* Pipeline = FD3D12DynamicRHI::ResourceCast(InPipeline);
	
	FD3D12RayTracingShaderTable* ShaderTable = Scene->FindOrCreateShaderTable(Pipeline);

	const uint32 RecordIndex = Scene->GetHitGroupIndex(InstanceIndex, SegmentIndex);
	ShaderTable->SetHitGroupIdentifier(RecordIndex, Pipeline->HitGroupShaderIdentifiers[HitGroupIndex]);

	const FD3D12RayTracingShader* Shader = Pipeline->HitGroupShaders[HitGroupIndex];

	FD3D12RayTracingLocalResourceBinder ResourceBinder(*this, ShaderTable, Shader->pRootSignature, RecordIndex);
	SetRayTracingShaderResources(*this, Shader, ResourceBindings, ShaderTable->DescriptorCache, ResourceBinder);
}

#undef VERIFYHRESULT

#endif // D3D12_RHI_RAYTRACING
