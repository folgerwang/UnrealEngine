// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
D3D12Adapter.cpp:D3D12 Adapter implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"

static TAutoConsoleVariable<int32> CVarTransientUniformBufferAllocatorSizeKB(
	TEXT("D3D12.TransientUniformBufferAllocatorSizeKB"),
	2 * 1024,
	TEXT(""),
	ECVF_ReadOnly
);

#if ENABLE_RESIDENCY_MANAGEMENT
bool GEnableResidencyManagement = true;
static TAutoConsoleVariable<int32> CVarResidencyManagement(
	TEXT("D3D12.ResidencyManagement"),
	1,
	TEXT("Controls whether D3D12 resource residency management is active (default = on)."),
	ECVF_ReadOnly
);
#endif // ENABLE_RESIDENCY_MANAGEMENT

struct FRHICommandSignalFrameFence final : public FRHICommand<FRHICommandSignalFrameFence>
{
	ED3D12CommandQueueType QueueType;
	FD3D12ManualFence* const Fence;
	const uint64 Value;
	FORCEINLINE_DEBUGGABLE FRHICommandSignalFrameFence(ED3D12CommandQueueType InQueueType, FD3D12ManualFence* InFence, uint64 InValue)
		: QueueType(InQueueType)
		, Fence(InFence)
		, Value(InValue)
	{ 
	}

	void Execute(FRHICommandListBase& CmdList)
	{
		Fence->Signal(QueueType, Value);
		check(Fence->GetLastSignaledFence() == Value);
	}
};

FD3D12Adapter::FD3D12Adapter(FD3D12AdapterDesc& DescIn)
	: OwningRHI(nullptr)
	, bDepthBoundsTestSupported(false)
	, bDeviceRemoved(false)
	, Desc(DescIn)
	, RootSignatureManager(this)
	, PipelineStateCache(this)
	, FenceCorePool(this)
	, DeferredDeletionQueue(this)
	, DefaultContextRedirector(this, true, false)
	, DefaultAsyncComputeContextRedirector(this, false, true)
	, GPUProfilingData(this)
	, DebugFlags(0)
{
	FMemory::Memzero(&UploadHeapAllocator, sizeof(UploadHeapAllocator));
	FMemory::Memzero(&Devices, sizeof(Devices));

	uint32 MaxGPUCount = 1; // By default, multi-gpu is disabled.
#if WITH_MGPU
	if (!FParse::Value(FCommandLine::Get(), TEXT("MaxGPUCount="), MaxGPUCount))
	{
		// If there is a mode token in the command line, enable multi-gpu.
		if (FParse::Param(FCommandLine::Get(), TEXT("AFR")))
		{
			MaxGPUCount = MAX_NUM_GPUS;
		}
	}
#endif
	Desc.NumDeviceNodes = FMath::Min3<uint32>(Desc.NumDeviceNodes, MaxGPUCount, MAX_NUM_GPUS);
}

void FD3D12Adapter::Initialize(FD3D12DynamicRHI* RHI)
{
	OwningRHI = RHI;
}

void FD3D12Adapter::CreateRootDevice(bool bWithDebug)
{
	CreateDXGIFactory();

	// QI for the Adapter
	TRefCountPtr<IDXGIAdapter> TempAdapter;
	DxgiFactory->EnumAdapters(Desc.AdapterIndex, TempAdapter.GetInitReference());
	VERIFYD3D12RESULT(TempAdapter->QueryInterface(IID_PPV_ARGS(DxgiAdapter.GetInitReference())));

	// In Direct3D 11, if you are trying to create a hardware or a software device, set pAdapter != NULL which constrains the other inputs to be:
	//		DriverType must be D3D_DRIVER_TYPE_UNKNOWN 
	//		Software must be NULL. 
	D3D_DRIVER_TYPE DriverType = D3D_DRIVER_TYPE_UNKNOWN;

#if PLATFORM_WINDOWS
	if (bWithDebug)
	{
		TRefCountPtr<ID3D12Debug> DebugController;
		VERIFYD3D12RESULT(D3D12GetDebugInterface(IID_PPV_ARGS(DebugController.GetInitReference())));
		DebugController->EnableDebugLayer();

		// TODO: MSFT: BEGIN TEMPORARY WORKAROUND for a debug layer issue with the Editor creating lots of viewports (swapchains).
		// Without this you could see this error: D3D12 ERROR: ID3D12CommandQueue::ExecuteCommandLists: Up to 8 swapchains can be written to by a single command queue. Present must be called on one of the swapchains to enable a command queue to execute command lists that write to more.  [ STATE_SETTING ERROR #906: COMMAND_QUEUE_TOO_MANY_SWAPCHAIN_REFERENCES]
		if (GIsEditor)
		{
			TRefCountPtr<ID3D12Debug1> DebugController1;
			const HRESULT HrDebugController1 = D3D12GetDebugInterface(IID_PPV_ARGS(DebugController1.GetInitReference()));
			if (DebugController1.GetReference())
			{
				DebugController1->SetEnableSynchronizedCommandQueueValidation(false);
				UE_LOG(LogD3D12RHI, Warning, TEXT("Disabling the debug layer's Synchronized Command Queue Validation. This means many debug layer features won't do anything. This code should be removed as soon as possible with an update debug layer."));
			}
		}
		// END TEMPORARY WORKAROUND

		bool bD3d12gpuvalidation = false;
		if (FParse::Param(FCommandLine::Get(), TEXT("d3d12gpuvalidation")))
		{
			TRefCountPtr<ID3D12Debug1> DebugController1;
			VERIFYD3D12RESULT(DebugController->QueryInterface(IID_PPV_ARGS(DebugController1.GetInitReference())));
			DebugController1->SetEnableGPUBasedValidation(true);
			bD3d12gpuvalidation = true;
		}

		UE_LOG(LogD3D12RHI, Log, TEXT("InitD3DDevice: -D3DDebug = %s -D3D12GPUValidation = %s"), bWithDebug ? TEXT("on") : TEXT("off"), bD3d12gpuvalidation ? TEXT("on") : TEXT("off") );
	}
#endif // PLATFORM_WINDOWS

#if USE_PIX
	UE_LOG(LogD3D12RHI, Log, TEXT("Emitting draw events for PIX profiling."));
	SetEmitDrawEvents(true);
#endif
	const bool bIsPerfHUD = !FCString::Stricmp(GetD3DAdapterDesc().Description, TEXT("NVIDIA PerfHUD"));

	if (bIsPerfHUD)
	{
		DriverType = D3D_DRIVER_TYPE_REFERENCE;
	}

	// Creating the Direct3D device.
	VERIFYD3D12RESULT(D3D12CreateDevice(
		GetAdapter(),
		GetFeatureLevel(),
		IID_PPV_ARGS(RootDevice.GetInitReference())
	));

	// Detect availability of shader model 6.0 wave operations
	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS1 Features = {};
		RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS1, &Features, sizeof(Features));
		GRHISupportsWaveOperations = Features.WaveOps;
	}

#if ENABLE_RESIDENCY_MANAGEMENT
	if (!CVarResidencyManagement.GetValueOnAnyThread())
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("D3D12 resource residency management is disabled."));
		GEnableResidencyManagement = false;
	}
#endif // ENABLE_RESIDENCY_MANAGEMENT


#if D3D12_RHI_RAYTRACING
	bool bRayTracingSupported = false;

	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 Features = {};
		if (SUCCEEDED(RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &Features, sizeof(Features)))
			&& Features.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0)
		{
			bRayTracingSupported = true;
		}
	}

	auto GetRayTracingCVarValue = []()
	{
		auto CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("r.RayTracing"));
		return CVar && CVar->GetInt() > 0;
	};

 	if (bRayTracingSupported && GetRayTracingCVarValue() && !FParse::Param(FCommandLine::Get(), TEXT("noraytracing")))
	{
		RootDevice->QueryInterface(IID_PPV_ARGS(RootRayTracingDevice.GetInitReference()));
		if (RootRayTracingDevice)
		{
			UE_LOG(LogD3D12RHI, Log, TEXT("D3D12 ray tracing enabled."));
		}
		else
		{
			bRayTracingSupported = false;
		}
	}
#endif // D3D12_RHI_RAYTRACING

#if NV_AFTERMATH
	// Two ways to enable aftermath, command line or the r.GPUCrashDebugging variable
	// Note: If intending to change this please alert game teams who use this for user support.
	if (FParse::Param(FCommandLine::Get(), TEXT("gpucrashdebugging")))
	{
		GDX12NVAfterMathEnabled = true;
	}
	else
	{
		static IConsoleVariable* GPUCrashDebugging = IConsoleManager::Get().FindConsoleVariable(TEXT("r.GPUCrashDebugging"));
		if (GPUCrashDebugging)
		{
			GDX12NVAfterMathEnabled = GPUCrashDebugging->GetInt();
		}
	}

	if (GDX12NVAfterMathEnabled)
	{
		if (IsRHIDeviceNVIDIA())
		{
			GFSDK_Aftermath_Result Result = GFSDK_Aftermath_DX12_Initialize(GFSDK_Aftermath_Version_API, GFSDK_Aftermath_FeatureFlags_Maximum, RootDevice);
			if (Result == GFSDK_Aftermath_Result_Success)
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("[Aftermath] Aftermath enabled and primed"));
				SetEmitDrawEvents(true);
			}
			else
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("[Aftermath] Aftermath enabled but failed to initialize (%x)"), Result);
				GDX12NVAfterMathEnabled = 0;
			}
		}
		else
		{
			GDX12NVAfterMathEnabled = 0;
			UE_LOG(LogD3D12RHI, Warning, TEXT("[Aftermath] Skipping aftermath initialization on non-Nvidia device"));
		}
	}
#endif


#if UE_BUILD_DEBUG	&& PLATFORM_WINDOWS
	//break on debug
	TRefCountPtr<ID3D12Debug> d3dDebug;
	if (SUCCEEDED(RootDevice->QueryInterface(__uuidof(ID3D12Debug), (void**)d3dDebug.GetInitReference())))
	{
		TRefCountPtr<ID3D12InfoQueue> d3dInfoQueue;
		if (SUCCEEDED(d3dDebug->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)d3dInfoQueue.GetInitReference())))
		{
			d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
			//d3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
		}
	}
#endif

#if !(UE_BUILD_SHIPPING && WITH_EDITOR) && PLATFORM_WINDOWS
	// Add some filter outs for known debug spew messages (that we don't care about)
	if (bWithDebug)
	{
		ID3D12InfoQueue *pd3dInfoQueue = nullptr;
		VERIFYD3D12RESULT(RootDevice->QueryInterface(__uuidof(ID3D12InfoQueue), (void**)&pd3dInfoQueue));
		if (pd3dInfoQueue)
		{
			D3D12_INFO_QUEUE_FILTER NewFilter;
			FMemory::Memzero(&NewFilter, sizeof(NewFilter));

			// Turn off info msgs as these get really spewy
			D3D12_MESSAGE_SEVERITY DenySeverity = D3D12_MESSAGE_SEVERITY_INFO;
			NewFilter.DenyList.NumSeverities = 1;
			NewFilter.DenyList.pSeverityList = &DenySeverity;

			// Be sure to carefully comment the reason for any additions here!  Someone should be able to look at it later and get an idea of whether it is still necessary.
			TArray<D3D12_MESSAGE_ID, TInlineAllocator<16>> DenyIds = {
				// OMSETRENDERTARGETS_INVALIDVIEW - d3d will complain if depth and color targets don't have the exact same dimensions, but actually
				//	if the color target is smaller then things are ok.  So turn off this error.  There is a manual check in FD3D12DynamicRHI::SetRenderTarget
				//	that tests for depth smaller than color and MSAA settings to match.
				D3D12_MESSAGE_ID_OMSETRENDERTARGETS_INVALIDVIEW,

				// QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS - The RHI exposes the interface to make and issue queries and a separate interface to use that data.
				//		Currently there is a situation where queries are issued and the results may be ignored on purpose.  Filtering out this message so it doesn't
				//		swarm the debug spew and mask other important warnings
				//D3D12_MESSAGE_ID_QUERY_BEGIN_ABANDONING_PREVIOUS_RESULTS,
				//D3D12_MESSAGE_ID_QUERY_END_ABANDONING_PREVIOUS_RESULTS,

				// D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT - This is a warning that gets triggered if you use a null vertex declaration,
				//       which we want to do when the vertex shader is generating vertices based on ID.
				D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT,

				// D3D12_MESSAGE_ID_COMMAND_LIST_DRAW_INDEX_BUFFER_TOO_SMALL - This warning gets triggered by Slate draws which are actually using a valid index range.
				//		The invalid warning seems to only happen when VS 2012 is installed.  Reported to MS.  
				//		There is now an assert in DrawIndexedPrimitive to catch any valid errors reading from the index buffer outside of range.
				D3D12_MESSAGE_ID_COMMAND_LIST_DRAW_INDEX_BUFFER_TOO_SMALL,

				// D3D12_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET - This warning gets triggered by shadow depth rendering because the shader outputs
				//		a color but we don't bind a color render target. That is safe as writes to unbound render targets are discarded.
				//		Also, batched elements triggers it when rendering outside of scene rendering as it outputs to the GBuffer containing normals which is not bound.
				//(D3D12_MESSAGE_ID)3146081, // D3D12_MESSAGE_ID_DEVICE_DRAW_RENDERTARGETVIEW_NOT_SET,
				// BUGBUG: There is a D3D12_MESSAGE_ID_DEVICE_DRAW_DEPTHSTENCILVIEW_NOT_SET, why not one for RT?

				// D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE/D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE - 
				//      This warning gets triggered by ClearDepthStencilView/ClearRenderTargetView because when the resource was created
				//      it wasn't passed an optimized clear color (see CreateCommitedResource). This shows up a lot and is very noisy.
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,

				// D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED - This warning gets triggered by ExecuteCommandLists.
				//		if it contains a readback resource that still has mapped subresources when executing a command list that performs a copy operation to the resource.
				//		This may be ok if any data read from the readback resources was flushed by calling Unmap() after the resourcecopy operation completed.
				//		We intentionally keep the readback resources persistently mapped.
				D3D12_MESSAGE_ID_EXECUTECOMMANDLISTS_GPU_WRITTEN_READBACK_RESOURCE_MAPPED,

				// Note message ID doesn't exist in the current header (yet, should be available in the RS2 header) for now just mute by the ID number.
				// RESOURCE_BARRIER_DUPLICATE_SUBRESOURCE_TRANSITIONS - This shows up a lot and is very noisy. It would require changes to the resource tracking system
				// but will hopefully be resolved when the RHI switches to use the engine's resource tracking system.
				(D3D12_MESSAGE_ID)1008,

				// This error gets generated on the first run when you install a new driver. The code handles this error properly and resets the PipelineLibrary,
				// so we can safely ignore this message. It could possibly be avoided by adding driver version to the PSO cache filename, but an average user is unlikely
				// to be interested in keeping PSO caches associated with old drivers around on disk, so it's better to just reset.
				D3D12_MESSAGE_ID_CREATEPIPELINELIBRARY_DRIVERVERSIONMISMATCH,

#if ENABLE_RESIDENCY_MANAGEMENT
				// TODO: Remove this when the debug layers work for executions which are guarded by a fence
				D3D12_MESSAGE_ID_INVALID_USE_OF_NON_RESIDENT_RESOURCE,
#endif
			};

#if D3D12_RHI_RAYTRACING
			if (bRayTracingSupported)
			{
				// When the debug layer is enabled and ray tracing is supported, this error is triggered after a CopyDescriptors
				// call in the DescriptorCache even when ray tracing device is never used. This workaround is still required as of 2018-12-17.
				DenyIds.Add(D3D12_MESSAGE_ID_COPY_DESCRIPTORS_INVALID_RANGES);
			}
#endif // D3D12_RHI_RAYTRACING

			NewFilter.DenyList.NumIDs = DenyIds.Num();
			NewFilter.DenyList.pIDList = DenyIds.GetData();

			pd3dInfoQueue->PushStorageFilter(&NewFilter);

			// Break on D3D debug errors.
			pd3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);

			// Enable this to break on a specific id in order to quickly get a callstack
			//pd3dInfoQueue->SetBreakOnID(D3D12_MESSAGE_ID_DEVICE_DRAW_CONSTANT_BUFFER_TOO_SMALL, true);

			if (FParse::Param(FCommandLine::Get(), TEXT("d3dbreakonwarning")))
			{
				pd3dInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
			}

			pd3dInfoQueue->Release();
		}
	}
#endif

#if WITH_MGPU
	GNumExplicitGPUsForRendering = 1;
	if (Desc.NumDeviceNodes > 1)
	{
		if (GIsEditor)
		{
			UE_LOG(LogD3D12RHI, Log, TEXT("Multi-GPU is available, but skipping due to editor mode."));
		}
		else
		{
			GNumExplicitGPUsForRendering = Desc.NumDeviceNodes;
			UE_LOG(LogD3D12RHI, Log, TEXT("Enabling multi-GPU with %d nodes"), Desc.NumDeviceNodes);
		}
	}
#endif
}

void FD3D12Adapter::InitializeDevices()
{
	check(IsInGameThread());

	// Wait for the rendering thread to go idle.
	SCOPED_SUSPEND_RENDERING_THREAD(false);

	// If the device we were using has been removed, release it and the resources we created for it.
	if (bDeviceRemoved)
	{
		check(RootDevice);

		HRESULT hRes = RootDevice->GetDeviceRemovedReason();

		const TCHAR* Reason = TEXT("?");
		switch (hRes)
		{
		case DXGI_ERROR_DEVICE_HUNG:			Reason = TEXT("HUNG"); break;
		case DXGI_ERROR_DEVICE_REMOVED:			Reason = TEXT("REMOVED"); break;
		case DXGI_ERROR_DEVICE_RESET:			Reason = TEXT("RESET"); break;
		case DXGI_ERROR_DRIVER_INTERNAL_ERROR:	Reason = TEXT("INTERNAL_ERROR"); break;
		case DXGI_ERROR_INVALID_CALL:			Reason = TEXT("INVALID_CALL"); break;
		}

		bDeviceRemoved = false;

		Cleanup();

		// We currently don't support removed devices because FTexture2DResource can't recreate its RHI resources from scratch.
		// We would also need to recreate the viewport swap chains from scratch.
		UE_LOG(LogD3D12RHI, Fatal, TEXT("The Direct3D 12 device that was being used has been removed (Error: %d '%s').  Please restart the game."), hRes, Reason);
	}

	// Use a debug device if specified on the command line.
	bool bWithD3DDebug = D3D12RHI_ShouldCreateWithD3DDebug();

	// If we don't have a device yet, either because this is the first viewport, or the old device was removed, create a device.
	if (!RootDevice)
	{
		CreateRootDevice(bWithD3DDebug);

		// See if we can get any newer device interfaces (to use newer D3D12 features).
		if (D3D12RHI_ShouldForceCompatibility())
		{
			UE_LOG(LogD3D12RHI, Log, TEXT("Forcing D3D12 compatibility."));
		}
		else
		{
			if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(RootDevice1.GetInitReference()))))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("The system supports ID3D12Device1."));
			}

	#if PLATFORM_WINDOWS
			if (SUCCEEDED(RootDevice->QueryInterface(IID_PPV_ARGS(RootDevice2.GetInitReference()))))
			{
				UE_LOG(LogD3D12RHI, Log, TEXT("The system supports ID3D12Device2."));
			}
	#endif
		}
		D3D12_FEATURE_DATA_D3D12_OPTIONS D3D12Caps;
		FMemory::Memzero(&D3D12Caps, sizeof(D3D12Caps));
		VERIFYD3D12RESULT(RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &D3D12Caps, sizeof(D3D12Caps)));
		ResourceHeapTier = D3D12Caps.ResourceHeapTier;
		ResourceBindingTier = D3D12Caps.ResourceBindingTier;

#if PLATFORM_WINDOWS
		D3D12_FEATURE_DATA_D3D12_OPTIONS2 D3D12Caps2 = {};
		if (FAILED(RootDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS2, &D3D12Caps2, sizeof(D3D12Caps2))))
		{
			D3D12Caps2.DepthBoundsTestSupported = false;
			D3D12Caps2.ProgrammableSamplePositionsTier = D3D12_PROGRAMMABLE_SAMPLE_POSITIONS_TIER_NOT_SUPPORTED;
		}
		bDepthBoundsTestSupported = !!D3D12Caps2.DepthBoundsTestSupported;
#endif

		D3D12_FEATURE_DATA_ROOT_SIGNATURE D3D12RootSignatureCaps = {};
		D3D12RootSignatureCaps.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;	// This is the highest version we currently support. If CheckFeatureSupport succeeds, the HighestVersion returned will not be greater than this.
		if (FAILED(RootDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &D3D12RootSignatureCaps, sizeof(D3D12RootSignatureCaps))))
		{
			D3D12RootSignatureCaps.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
		}
		RootSignatureVersion = D3D12RootSignatureCaps.HighestVersion;

		FrameFence = new FD3D12ManualFence(this, FRHIGPUMask::All(), L"Adapter Frame Fence");
		FrameFence->CreateFence();

		StagingFence = new FD3D12Fence(this, FRHIGPUMask::All(), L"Staging Fence");
		StagingFence->CreateFence();

		CreateSignatures();

		//Create all of the FD3D12Devices
		for (uint32 GPUIndex : FRHIGPUMask::All())
		{
			Devices[GPUIndex] = new FD3D12Device(FRHIGPUMask::FromIndex(GPUIndex), this);
			Devices[GPUIndex]->Initialize();

			// The redirectors allow to broadcast to any GPU set
			DefaultContextRedirector.SetPhysicalContext(&Devices[GPUIndex]->GetDefaultCommandContext());
			if (GEnableAsyncCompute)
			{
				DefaultAsyncComputeContextRedirector.SetPhysicalContext(&Devices[GPUIndex]->GetDefaultAsyncComputeContext());
			}
		}

		DefaultContextRedirector.SetGPUMask(FRHIGPUMask::All());
		DefaultAsyncComputeContextRedirector.SetGPUMask(FRHIGPUMask::All());

		// Initialize the immediate command list GPU mask now that everything is set.
		FRHICommandListExecutor::GetImmediateCommandList().SetGPUMask(FRHIGPUMask::All());
		FRHICommandListExecutor::GetImmediateAsyncComputeCommandList().SetGPUMask(FRHIGPUMask::All());

		GPUProfilingData.Init();

		const FString Name(L"Upload Buffer Allocator");

		for (uint32 GPUIndex : FRHIGPUMask::All())
		{
			// Safe to init as we have a device;
			UploadHeapAllocator[GPUIndex] = new FD3D12DynamicHeapAllocator(this,
				Devices[GPUIndex],
				Name,
				kManualSubAllocationStrategy,
				DEFAULT_CONTEXT_UPLOAD_POOL_MAX_ALLOC_SIZE,
				DEFAULT_CONTEXT_UPLOAD_POOL_SIZE,
				DEFAULT_CONTEXT_UPLOAD_POOL_ALIGNMENT);

			UploadHeapAllocator[GPUIndex]->Init();
		}


		// ID3D12Device1::CreatePipelineLibrary() requires each blob to be specific to the given adapter. To do this we create a unique file name with from the adpater desc. 
		// Note that : "The uniqueness of an LUID is guaranteed only until the system is restarted" according to windows doc and thus can not be reused.
		const FString UniqueDeviceCachePath = FString::Printf(TEXT("V%d_D%d_S%d_R%d.ushaderprecache"), Desc.Desc.VendorId, Desc.Desc.DeviceId, Desc.Desc.SubSysId, Desc.Desc.Revision);
		FString GraphicsCacheFile = PIPELINE_STATE_FILE_LOCATION / FString::Printf(TEXT("D3DGraphics_%s"), *UniqueDeviceCachePath);
	    FString ComputeCacheFile = PIPELINE_STATE_FILE_LOCATION / FString::Printf(TEXT("D3DCompute_%s"), *UniqueDeviceCachePath);
		FString DriverBlobFilename = PIPELINE_STATE_FILE_LOCATION / FString::Printf(TEXT("D3DDriverByteCodeBlob_%s"), *UniqueDeviceCachePath);

		PipelineStateCache.Init(GraphicsCacheFile, ComputeCacheFile, DriverBlobFilename);

		ID3D12RootSignature* StaticGraphicsRS = (GetStaticGraphicsRootSignature()) ? GetStaticGraphicsRootSignature()->GetRootSignature() : nullptr;
		ID3D12RootSignature* StaticComputeRS = (GetStaticComputeRootSignature()) ? GetStaticComputeRootSignature()->GetRootSignature() : nullptr;

		// #dxr_todo: verify that disk cache works correctly with DXR
		PipelineStateCache.RebuildFromDiskCache(StaticGraphicsRS, StaticComputeRS);
	}
}

void FD3D12Adapter::InitializeRayTracing()
{
#if D3D12_RHI_RAYTRACING
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		if (Devices[GPUIndex]->GetRayTracingDevice())
		{
			Devices[GPUIndex]->InitRayTracing();
		}
	}
#endif // D3D12_RHI_RAYTRACING
}

void FD3D12Adapter::CreateSignatures()
{
	ID3D12Device* Device = GetD3DDevice();

	// ExecuteIndirect command signatures
	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.NumArgumentDescs = 1;
	commandSignatureDesc.ByteStride = 20;
	commandSignatureDesc.NodeMask = (uint32)FRHIGPUMask::All();

	D3D12_INDIRECT_ARGUMENT_DESC indirectParameterDesc[1] = {};
	commandSignatureDesc.pArgumentDescs = indirectParameterDesc;

	indirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
	commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_ARGUMENTS);
	VERIFYD3D12RESULT(Device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(DrawIndirectCommandSignature.GetInitReference())));

	indirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;
	commandSignatureDesc.ByteStride = sizeof(D3D12_DRAW_INDEXED_ARGUMENTS);
	VERIFYD3D12RESULT(Device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(DrawIndexedIndirectCommandSignature.GetInitReference())));

	indirectParameterDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
	commandSignatureDesc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
	VERIFYD3D12RESULT(Device->CreateCommandSignature(&commandSignatureDesc, nullptr, IID_PPV_ARGS(DispatchIndirectCommandSignature.GetInitReference())));
}


void FD3D12Adapter::Cleanup()
{
	// Reset the RHI initialized flag.
	GIsRHIInitialized = false;

	for (auto& Viewport : Viewports)
	{
		Viewport->IssueFrameEvent();
		Viewport->WaitForFrameEventCompletion();
	}

#if D3D12_RHI_RAYTRACING
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		Devices[GPUIndex]->CleanupRayTracing();
	}
#endif // D3D12_RHI_RAYTRACING

	// Manually destroy the effects as we can't do it in their destructor.
	for (auto& Effect : TemporalEffectMap)
	{
		Effect.Value.Destroy();
	}

	// Ask all initialized FRenderResources to release their RHI resources.
	for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList()); ResourceIt; ResourceIt.Next())
	{
		FRenderResource* Resource = *ResourceIt;
		check(Resource->IsInitialized());
		Resource->ReleaseRHI();
	}

	for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList()); ResourceIt; ResourceIt.Next())
	{
		ResourceIt->ReleaseDynamicRHI();
	}

	TransientUniformBufferAllocator.Destroy();

	FRHIResource::FlushPendingDeletes();

	// Clean up the asnyc texture thread allocators
	for (int32 i = 0; i < GetOwningRHI()->NumThreadDynamicHeapAllocators; i++)
	{
		GetOwningRHI()->ThreadDynamicHeapAllocatorArray[i]->Destroy<FD3D12ScopeLock>();
		delete(GetOwningRHI()->ThreadDynamicHeapAllocatorArray[i]);
	}

	// Cleanup resources
	DeferredDeletionQueue.Clear();

	// First clean up everything before deleting as there are shared resource location between devices.
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		Devices[GPUIndex]->Cleanup();
	}	
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		delete(Devices[GPUIndex]);
		Devices[GPUIndex] = nullptr;
	}

	// Release buffered timestamp queries
	GPUProfilingData.FrameTiming.ReleaseResource();

	Viewports.Empty();
	DrawingViewport = nullptr;

	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		UploadHeapAllocator[GPUIndex]->Destroy();
		delete(UploadHeapAllocator[GPUIndex]);
		UploadHeapAllocator[GPUIndex] = nullptr;
	}

	if (FrameFence)
	{
		FrameFence->Destroy();
		FrameFence.SafeRelease();
	}

	if (StagingFence)
	{
		StagingFence->Destroy();
		StagingFence.SafeRelease();
	}


	PipelineStateCache.Close();
	RootSignatureManager.Destroy();

	DrawIndirectCommandSignature.SafeRelease();
	DrawIndexedIndirectCommandSignature.SafeRelease();
	DispatchIndirectCommandSignature.SafeRelease();

	FenceCorePool.Destroy();
}

void FD3D12Adapter::EndFrame()
{
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		GetUploadHeapAllocator(GPUIndex).CleanUpAllocations();
	}
	GetDeferredDeletionQueue().ReleaseResources();
}

void FD3D12Adapter::SignalFrameFence_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());
	check(RHICmdList.IsImmediate());

	// Increment the current fence (on render thread timeline).
	const uint64 PreviousFence = FrameFence->IncrementCurrentFence();

	// Queue a command to signal the frame fence is complete on the GPU (on the RHI thread timeline if using an RHI thread).
	if (RHICmdList.Bypass() || !IsRunningRHIInSeparateThread())
	{
		FRHICommandSignalFrameFence Cmd(ED3D12CommandQueueType::Default, FrameFence, PreviousFence);
		Cmd.Execute(RHICmdList);
	}
	else
	{
		ALLOC_COMMAND_CL(RHICmdList, FRHICommandSignalFrameFence)(ED3D12CommandQueueType::Default, FrameFence, PreviousFence);
	}
}

FD3D12TemporalEffect* FD3D12Adapter::GetTemporalEffect(const FName& EffectName)
{
	FD3D12TemporalEffect* Effect = TemporalEffectMap.Find(EffectName);

	if (Effect == nullptr)
	{
		Effect = &TemporalEffectMap.Emplace(EffectName, FD3D12TemporalEffect(this, EffectName));
		Effect->Init();
	}

	check(Effect);
	return Effect;
}

FD3D12FastConstantAllocator& FD3D12Adapter::GetTransientUniformBufferAllocator()
{
	// Multi-GPU support : is using device 0 always appropriate here?
	return *TransientUniformBufferAllocator.GetObjectForThisThread([this]() -> FD3D12FastConstantAllocator*
	{
		FD3D12FastConstantAllocator* Alloc = new FD3D12FastConstantAllocator(Devices[0], FRHIGPUMask::All(), CVarTransientUniformBufferAllocatorSizeKB.GetValueOnAnyThread() * 1024);
		Alloc->Init();
		return Alloc;
	});
}

void FD3D12Adapter::GetLocalVideoMemoryInfo(DXGI_QUERY_VIDEO_MEMORY_INFO* LocalVideoMemoryInfo)
{
#if PLATFORM_WINDOWS
	TRefCountPtr<IDXGIAdapter3> Adapter3;
	VERIFYD3D12RESULT(GetAdapter()->QueryInterface(IID_PPV_ARGS(Adapter3.GetInitReference())));

	VERIFYD3D12RESULT(Adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, LocalVideoMemoryInfo));

	for (uint32 Index = 1; Index < GNumExplicitGPUsForRendering; ++Index)
	{
		DXGI_QUERY_VIDEO_MEMORY_INFO TempVideoMemoryInfo;
		VERIFYD3D12RESULT(Adapter3->QueryVideoMemoryInfo(Index, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &TempVideoMemoryInfo));
		LocalVideoMemoryInfo->Budget = FMath::Min(LocalVideoMemoryInfo->Budget, TempVideoMemoryInfo.Budget);
		LocalVideoMemoryInfo->Budget = FMath::Min(LocalVideoMemoryInfo->CurrentUsage, TempVideoMemoryInfo.CurrentUsage);
	}
#endif
}

void FD3D12Adapter::BlockUntilIdle()
{
	for (uint32 GPUIndex : FRHIGPUMask::All())
	{
		GetDevice(GPUIndex)->BlockUntilIdle();
	}
}
