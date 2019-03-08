// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "D3D12RHIPrivate.h"
#include "D3D12CommandList.h"

void FD3D12CommandListHandle::AddTransitionBarrier(FD3D12Resource* pResource, D3D12_RESOURCE_STATES Before, D3D12_RESOURCE_STATES After, uint32 Subresource)
{
	check(CommandListData);
	CommandListData->ResourceBarrierBatcher.AddTransition(pResource->GetResource(), Before, After, Subresource);
	CommandListData->CurrentOwningContext->numBarriers++;

	pResource->UpdateResidency(*this);
}

void FD3D12CommandListHandle::AddUAVBarrier()
{
	check(CommandListData);
	CommandListData->ResourceBarrierBatcher.AddUAV();
	CommandListData->CurrentOwningContext->numBarriers++;
}

void FD3D12CommandListHandle::AddAliasingBarrier(FD3D12Resource* pResource)
{
	check(CommandListData);
	CommandListData->ResourceBarrierBatcher.AddAliasingBarrier(pResource->GetResource());
	CommandListData->CurrentOwningContext->numBarriers++;
}

void FD3D12CommandListHandle::Create(FD3D12Device* ParentDevice, D3D12_COMMAND_LIST_TYPE CommandListType, FD3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager)
{
	check(!CommandListData);

	CommandListData = new FD3D12CommandListData(ParentDevice, CommandListType, CommandAllocator, InCommandListManager);

	CommandListData->AddRef();
}

FD3D12CommandListHandle::FD3D12CommandListData::FD3D12CommandListData(FD3D12Device* ParentDevice, D3D12_COMMAND_LIST_TYPE InCommandListType, FD3D12CommandAllocator& CommandAllocator, FD3D12CommandListManager* InCommandListManager)
	: FD3D12DeviceChild(ParentDevice)
	, FD3D12SingleNodeGPUObject(ParentDevice->GetGPUMask())
	, CommandListManager(InCommandListManager)
	, CurrentOwningContext(nullptr)
	, CommandListType(InCommandListType)
	, CurrentCommandAllocator(&CommandAllocator)
	, CurrentGeneration(1)
	, LastCompleteGeneration(0)
	, IsClosed(false)
	, bShouldTrackStartEndTime(false)
	, PendingResourceBarriers()
	, ResidencySet(nullptr)
#if WITH_PROFILEGPU
	, StartTimeQueryIdx(INDEX_NONE)
#endif
{
	VERIFYD3D12RESULT(ParentDevice->GetDevice()->CreateCommandList((uint32)GetGPUMask(), CommandListType, CommandAllocator, nullptr, IID_PPV_ARGS(CommandList.GetInitReference())));
	INC_DWORD_STAT(STAT_D3D12NumCommandLists);

#if PLATFORM_WINDOWS
	// Optionally obtain the ID3D12GraphicsCommandList1 interface, we don't check the HRESULT.
	CommandList->QueryInterface(IID_PPV_ARGS(CommandList1.GetInitReference()));
#endif

#if D3D12_RHI_RAYTRACING
	// Obtain ID3D12CommandListRaytracingPrototype if parent device supports ray tracing and this is a compatible command list type (compute or graphics).
	if (ParentDevice->GetRayTracingDevice() && (InCommandListType == D3D12_COMMAND_LIST_TYPE_DIRECT || InCommandListType == D3D12_COMMAND_LIST_TYPE_COMPUTE))
	{
		VERIFYD3D12RESULT(CommandList->QueryInterface(IID_PPV_ARGS(RayTracingCommandList.GetInitReference())));
	}
#endif // D3D12_RHI_RAYTRACING

#if NAME_OBJECTS
	TArray<FStringFormatArg> Args;
	Args.Add(LexToString(ParentDevice->GetGPUIndex()));
	FString Name = FString::Format(TEXT("FD3D12CommandListData (GPU {0})"), Args);
	SetName(CommandList, Name.GetCharArray().GetData());
#endif

#if NV_AFTERMATH
	AftermathHandle = nullptr;

	if (GDX12NVAfterMathEnabled)
	{
		GFSDK_Aftermath_Result Result = GFSDK_Aftermath_DX12_CreateContextHandle(CommandList, &AftermathHandle);

		check(Result == GFSDK_Aftermath_Result_Success);
		ParentDevice->GetParentAdapter()->GetGPUProfiler().RegisterCommandList(AftermathHandle);
	}
#endif

	// Initially start with all lists closed.  We'll open them as we allocate them.
	Close();

	PendingResourceBarriers.Reserve(256);

	ResidencySet = D3DX12Residency::CreateResidencySet(ParentDevice->GetResidencyManager());
}

FD3D12CommandListHandle::FD3D12CommandListData::~FD3D12CommandListData()
{
#if NV_AFTERMATH
	if (AftermathHandle)
	{
		GetParentDevice()->GetParentAdapter()->GetGPUProfiler().UnregisterCommandList(AftermathHandle);

		GFSDK_Aftermath_Result Result = GFSDK_Aftermath_ReleaseContextHandle(AftermathHandle);

		check(Result == GFSDK_Aftermath_Result_Success);
	}
#endif

	CommandList.SafeRelease();
	DEC_DWORD_STAT(STAT_D3D12NumCommandLists);

	D3DX12Residency::DestroyResidencySet(GetParentDevice()->GetResidencyManager(), ResidencySet);
}

void FD3D12CommandListHandle::FD3D12CommandListData::Close()
{
	if (!IsClosed)
	{
		FlushResourceBarriers();
		if (bShouldTrackStartEndTime)
		{
			FinishTrackingCommandListTime();
		}
		VERIFYD3D12RESULT(CommandList->Close());

		D3DX12Residency::Close(ResidencySet);
		IsClosed = true;
	}
}

void FD3D12CommandListHandle::FD3D12CommandListData::Reset(FD3D12CommandAllocator& CommandAllocator, bool bTrackExecTime)
{
	VERIFYD3D12RESULT(CommandList->Reset(CommandAllocator, nullptr));

	CurrentCommandAllocator = &CommandAllocator;
	IsClosed = false;

	// Indicate this command allocator is being used.
	CurrentCommandAllocator->IncrementPendingCommandLists();

	CleanupActiveGenerations();

	// Remove all pendering barriers from the command list
	PendingResourceBarriers.Reset();

	// Empty tracked resource state for this command list
	TrackedResourceState.Empty();

	// If this fails there are too many concurrently open residency sets. Increase the value of MAX_NUM_CONCURRENT_CMD_LISTS
	// in the residency manager. Beware, this will increase the CPU memory usage of every tracked resource.
	D3DX12Residency::Open(ResidencySet);

	// If this fails then some previous resource barriers were never submitted.
	check(ResourceBarrierBatcher.GetBarriers().Num() == 0);

#if DEBUG_RESOURCE_STATES
	ResourceBarriers.Reset();
#endif

	if (bTrackExecTime)
	{
		StartTrackingCommandListTime();
	}
}

int32 FD3D12CommandListHandle::FD3D12CommandListData::CreateAndInsertTimestampQuery()
{	
	FD3D12LinearQueryHeap* QueryHeap = GetParentDevice()->GetCmdListExecTimeQueryHeap();
	check(QueryHeap);
	return QueryHeap->EndQuery(this);
}

void FD3D12CommandListHandle::FD3D12CommandListData::StartTrackingCommandListTime()
{
#if WITH_PROFILEGPU
	check(!IsClosed && !bShouldTrackStartEndTime && StartTimeQueryIdx == INDEX_NONE);
	bShouldTrackStartEndTime = true;
	StartTimeQueryIdx = CreateAndInsertTimestampQuery();
#endif
}

void FD3D12CommandListHandle::FD3D12CommandListData::FinishTrackingCommandListTime()
{
#if WITH_PROFILEGPU
	check(!IsClosed && bShouldTrackStartEndTime && StartTimeQueryIdx != INDEX_NONE);
	bShouldTrackStartEndTime = false;
	const int32 EndTimeQueryIdx = CreateAndInsertTimestampQuery();
	CommandListManager->AddCommandListTimingPair(StartTimeQueryIdx, EndTimeQueryIdx);
	StartTimeQueryIdx = INDEX_NONE;
#endif
}

void inline FD3D12CommandListHandle::FD3D12CommandListData::FCommandListResourceState::ConditionalInitalize(FD3D12Resource* pResource, CResourceState& ResourceState)
{
	// If there is no entry, all subresources should be in the resource's TBD state.
	// This means we need to have pending resource barrier(s).
	if (!ResourceState.CheckResourceStateInitalized())
	{
		ResourceState.Initialize(pResource->GetSubresourceCount());
		check(ResourceState.CheckResourceState(D3D12_RESOURCE_STATE_TBD));
	}

	check(ResourceState.CheckResourceStateInitalized());
}

CResourceState& FD3D12CommandListHandle::FD3D12CommandListData::FCommandListResourceState::GetResourceState(FD3D12Resource* pResource)
{
	// Only certain resources should use this
	check(pResource->RequiresResourceStateTracking());

	CResourceState& ResourceState = ResourceStates.FindOrAdd(pResource);
	ConditionalInitalize(pResource, ResourceState);
	return ResourceState;
}

void FD3D12CommandListHandle::FD3D12CommandListData::FCommandListResourceState::Empty()
{
	ResourceStates.Empty();
}

void FD3D12CommandListHandle::Execute(bool WaitForCompletion)
{
	check(CommandListData);
	CommandListData->CommandListManager->ExecuteCommandList(*this, WaitForCompletion);
}

FD3D12CommandAllocator::FD3D12CommandAllocator(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType)
	: PendingCommandListCount(0)
{
	Init(InDevice, InType);
}

FD3D12CommandAllocator::~FD3D12CommandAllocator()
{
	CommandAllocator.SafeRelease();
	DEC_DWORD_STAT(STAT_D3D12NumCommandAllocators);
}

void FD3D12CommandAllocator::Init(ID3D12Device* InDevice, const D3D12_COMMAND_LIST_TYPE& InType)
{
	check(CommandAllocator.GetReference() == nullptr);
	VERIFYD3D12RESULT(InDevice->CreateCommandAllocator(InType, IID_PPV_ARGS(CommandAllocator.GetInitReference())));
	INC_DWORD_STAT(STAT_D3D12NumCommandAllocators);
}
