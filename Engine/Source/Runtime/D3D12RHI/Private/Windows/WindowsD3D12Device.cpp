// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	WindowsD3D12Device.cpp: Windows D3D device RHI implementation.
=============================================================================*/

#include "D3D12RHIPrivate.h"
#include "Modules/ModuleManager.h"
#include "Windows/AllowWindowsPlatformTypes.h"
	#include <delayimp.h>
	#include "amd_ags.h"
#include "Windows/HideWindowsPlatformTypes.h"

#include "HardwareInfo.h"
#include "Runtime/HeadMountedDisplay/Public/IHeadMountedDisplayModule.h"
#include "GenericPlatform/GenericPlatformDriver.h"			// FGPUDriverInfo

#include "ShaderCompiler.h"

#pragma comment(lib, "d3d12.lib")

IMPLEMENT_MODULE(FD3D12DynamicRHIModule, D3D12RHI);

extern bool D3D12RHI_ShouldCreateWithD3DDebug();
extern bool D3D12RHI_ShouldCreateWithWarp();
extern bool D3D12RHI_ShouldAllowAsyncResourceCreation();
extern bool D3D12RHI_ShouldForceCompatibility();

static TAutoConsoleVariable<int32> CVarGraphicsAdapter(
	TEXT("D3D12.GraphicsAdapter"),
	-1,
	TEXT("User request to pick a specific graphics adapter (e.g. when using an integrated graphics card with a discrete one)\n")
	TEXT(" -2: Take the first one that fulfills the criteria\n")
	TEXT(" -1: Favor discrete because they are usually faster (default)\n")
	TEXT("  0: Adapter #0\n")
	TEXT("  1: Adapter #1, ..."),
	ECVF_RenderThreadSafe);

#if NV_AFTERMATH
// Disabled by default since introduces stalls between render and driver threads
int32 GDX12NVAfterMathEnabled = 0;
static FAutoConsoleVariableRef CVarDX12NVAfterMathBufferSize(
	TEXT("r.DX12NVAfterMathEnabled"),
	GDX12NVAfterMathEnabled,
	TEXT("Use NV Aftermath for GPU crash analysis in D3D12"),
	ECVF_ReadOnly
);
#endif

static inline int D3D12RHI_PreferAdapterVendor()
{
	if (FParse::Param(FCommandLine::Get(), TEXT("preferAMD")))
	{
		return 0x1002;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("preferIntel")))
	{
		return 0x8086;
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("preferNvidia")))
	{
		return 0x10DE;
	}

	return -1;
}

using namespace D3D12RHI;

static bool bIsQuadBufferStereoEnabled = false;

/** This function is used as a SEH filter to catch only delay load exceptions. */
static bool IsDelayLoadException(PEXCEPTION_POINTERS ExceptionPointers)
{
#if WINVER > 0x502	// Windows SDK 7.1 doesn't define VcppException
	switch (ExceptionPointers->ExceptionRecord->ExceptionCode)
	{
	case VcppException(ERROR_SEVERITY_ERROR, ERROR_MOD_NOT_FOUND):
	case VcppException(ERROR_SEVERITY_ERROR, ERROR_PROC_NOT_FOUND):
		return EXCEPTION_EXECUTE_HANDLER;
	default:
		return EXCEPTION_CONTINUE_SEARCH;
	}
#else
	return EXCEPTION_EXECUTE_HANDLER;
#endif
}

/**
 * Since CreateDXGIFactory is a delay loaded import from the DXGI DLL, if the user
 * doesn't have Vista/DX10, calling CreateDXGIFactory will throw an exception.
 * We use SEH to detect that case and fail gracefully.
 */
static void SafeCreateDXGIFactory(IDXGIFactory4** DXGIFactory)
{
#if !defined(D3D12_CUSTOM_VIEWPORT_CONSTRUCTOR) || !D3D12_CUSTOM_VIEWPORT_CONSTRUCTOR
	__try
	{
		bIsQuadBufferStereoEnabled = FParse::Param(FCommandLine::Get(), TEXT("quad_buffer_stereo"));

		CreateDXGIFactory(__uuidof(IDXGIFactory4), (void**)DXGIFactory);
	}
	__except (IsDelayLoadException(GetExceptionInformation()))
	{
		// We suppress warning C6322: Empty _except block. Appropriate checks are made upon returning. 
		CA_SUPPRESS(6322);
	}
#endif	//!D3D12_CUSTOM_VIEWPORT_CONSTRUCTOR
}

/**
 * Returns the minimum D3D feature level required to create based on
 * command line parameters.
 */
static D3D_FEATURE_LEVEL GetRequiredD3DFeatureLevel()
{
	return D3D_FEATURE_LEVEL_11_0;
}

/**
 * Attempts to create a D3D12 device for the adapter using at minimum MinFeatureLevel.
 * If creation is successful, true is returned and the max supported feature level is set in OutMaxFeatureLevel.
 */
static bool SafeTestD3D12CreateDevice(IDXGIAdapter* Adapter, D3D_FEATURE_LEVEL MinFeatureLevel, D3D_FEATURE_LEVEL& OutMaxFeatureLevel, uint32& OutNumDeviceNodes)
{
	const D3D_FEATURE_LEVEL FeatureLevels[] =
	{
		// Add new feature levels that the app supports here.
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0
	};

	__try
	{
		ID3D12Device* pDevice = nullptr;
		if (SUCCEEDED(D3D12CreateDevice(Adapter, MinFeatureLevel, IID_PPV_ARGS(&pDevice))))
		{
			// Determine the max feature level supported by the driver and hardware.
			D3D_FEATURE_LEVEL MaxFeatureLevel = MinFeatureLevel;
			D3D12_FEATURE_DATA_FEATURE_LEVELS FeatureLevelCaps = {};
			FeatureLevelCaps.pFeatureLevelsRequested = FeatureLevels;
			FeatureLevelCaps.NumFeatureLevels = ARRAY_COUNT(FeatureLevels);	
			if (SUCCEEDED(pDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &FeatureLevelCaps, sizeof(FeatureLevelCaps))))
			{
				MaxFeatureLevel = FeatureLevelCaps.MaxSupportedFeatureLevel;
			}

			OutMaxFeatureLevel = MaxFeatureLevel;
			OutNumDeviceNodes = pDevice->GetNodeCount();

			pDevice->Release();
			return true;
		}
	}
	__except (IsDelayLoadException(GetExceptionInformation()))
	{
		// We suppress warning C6322: Empty _except block. Appropriate checks are made upon returning. 
		CA_SUPPRESS(6322);
	}

	return false;
}

static bool SupportsDepthBoundsTest(FD3D12DynamicRHI* D3DRHI)
{
	// Determines if the primary adapter supports depth bounds test
	check(D3DRHI && D3DRHI->GetNumAdapters() >= 1);

	return D3DRHI->GetAdapter().IsDepthBoundsTestSupported();
}

static bool SupportsHDROutput(FD3D12DynamicRHI* D3DRHI)
{
	// Determines if any displays support HDR
	check(D3DRHI && D3DRHI->GetNumAdapters() >= 1);

	bool bSupportsHDROutput = false;
	const int32 NumAdapters = D3DRHI->GetNumAdapters();
	for (int32 AdapterIndex = 0; AdapterIndex < NumAdapters; ++AdapterIndex)
	{
		FD3D12Adapter& Adapter = D3DRHI->GetAdapter(AdapterIndex);
		IDXGIAdapter* DXGIAdapter = Adapter.GetAdapter();

		for (uint32 DisplayIndex = 0; true; ++DisplayIndex)
		{
			TRefCountPtr<IDXGIOutput> DXGIOutput;
			if (S_OK != DXGIAdapter->EnumOutputs(DisplayIndex, DXGIOutput.GetInitReference()))
			{
				break;
			}

			TRefCountPtr<IDXGIOutput6> Output6;
			if (SUCCEEDED(DXGIOutput->QueryInterface(IID_PPV_ARGS(Output6.GetInitReference()))))
			{
				DXGI_OUTPUT_DESC1 OutputDesc;
				VERIFYD3D12RESULT(Output6->GetDesc1(&OutputDesc));

				// Check for HDR support on the display.
				const bool bDisplaySupportsHDROutput = (OutputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
				if (bDisplaySupportsHDROutput)
				{
					UE_LOG(LogD3D12RHI, Log, TEXT("HDR output is supported on adapter %i, display %u:"), AdapterIndex, DisplayIndex);
					UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMinLuminance = %f"), OutputDesc.MinLuminance);
					UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMaxLuminance = %f"), OutputDesc.MaxLuminance);
					UE_LOG(LogD3D12RHI, Log, TEXT("\t\tMaxFullFrameLuminance = %f"), OutputDesc.MaxFullFrameLuminance);

					bSupportsHDROutput = true;
				}
			}
		}
	}

	return bSupportsHDROutput;
}

bool FD3D12DynamicRHIModule::IsSupported()
{
	// If not computed yet
	if (ChosenAdapters.Num() == 0)
	{
		FindAdapter();
	}

	// The hardware must support at least 11.0.
	return ChosenAdapters.Num() > 0
		&& ChosenAdapters[0]->GetDesc().IsValid()
		&& ChosenAdapters[0]->GetDesc().MaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_11_0;
}

namespace D3D12RHI
{
	const TCHAR* GetFeatureLevelString(D3D_FEATURE_LEVEL FeatureLevel)
	{
		switch (FeatureLevel)
		{
		case D3D_FEATURE_LEVEL_9_1:		return TEXT("9_1");
		case D3D_FEATURE_LEVEL_9_2:		return TEXT("9_2");
		case D3D_FEATURE_LEVEL_9_3:		return TEXT("9_3");
		case D3D_FEATURE_LEVEL_10_0:	return TEXT("10_0");
		case D3D_FEATURE_LEVEL_10_1:	return TEXT("10_1");
		case D3D_FEATURE_LEVEL_11_0:	return TEXT("11_0");
		case D3D_FEATURE_LEVEL_11_1:	return TEXT("11_1");
		case D3D_FEATURE_LEVEL_12_0:	return TEXT("12_0");
		case D3D_FEATURE_LEVEL_12_1:	return TEXT("12_1");
		}
		return TEXT("X_X");
	}
}

static uint32 CountAdapterOutputs(TRefCountPtr<IDXGIAdapter>& Adapter)
{
	uint32 OutputCount = 0;
	for (;;)
	{
		TRefCountPtr<IDXGIOutput> Output;
		HRESULT hr = Adapter->EnumOutputs(OutputCount, Output.GetInitReference());
		if (FAILED(hr))
		{
			break;
		}
		++OutputCount;
	}

	return OutputCount;
}

void FD3D12DynamicRHIModule::FindAdapter()
{
	// Once we've chosen one we don't need to do it again.
	check(ChosenAdapters.Num() == 0);

	// Try to create the DXGIFactory.  This will fail if we're not running Vista.
	TRefCountPtr<IDXGIFactory4> DXGIFactory;
	SafeCreateDXGIFactory(DXGIFactory.GetInitReference());
	if (!DXGIFactory)
	{
		return;
	}

	bool bAllowPerfHUD = true;

#if UE_BUILD_SHIPPING || UE_BUILD_TEST
	bAllowPerfHUD = false;
#endif

	// Allow HMD to override which graphics adapter is chosen, so we pick the adapter where the HMD is connected
	uint64 HmdGraphicsAdapterLuid = IHeadMountedDisplayModule::IsAvailable() ? IHeadMountedDisplayModule::Get().GetGraphicsAdapterLuid() : 0;
	int32 CVarExplicitAdapterValue = HmdGraphicsAdapterLuid == 0 ? CVarGraphicsAdapter.GetValueOnGameThread() : -2;

	const bool bFavorNonIntegrated = CVarExplicitAdapterValue == -1;

	TRefCountPtr<IDXGIAdapter> TempAdapter;
	const D3D_FEATURE_LEVEL MinRequiredFeatureLevel = GetRequiredD3DFeatureLevel();

	FD3D12AdapterDesc FirstWithoutIntegratedAdapter;
	FD3D12AdapterDesc FirstAdapter;

	bool bIsAnyAMD = false;
	bool bIsAnyIntel = false;
	bool bIsAnyNVIDIA = false;
	bool bRequestedWARP = D3D12RHI_ShouldCreateWithWarp();

	int PreferredVendor = D3D12RHI_PreferAdapterVendor();
	// Enumerate the DXGIFactory's adapters.
	for (uint32 AdapterIndex = 0; DXGIFactory->EnumAdapters(AdapterIndex, TempAdapter.GetInitReference()) != DXGI_ERROR_NOT_FOUND; ++AdapterIndex)
	{
		// Check that if adapter supports D3D12.
		if (TempAdapter)
		{
			D3D_FEATURE_LEVEL MaxSupportedFeatureLevel = static_cast<D3D_FEATURE_LEVEL>(0);
			uint32 NumNodes = 0;
			if (SafeTestD3D12CreateDevice(TempAdapter, MinRequiredFeatureLevel, MaxSupportedFeatureLevel, NumNodes))
			{
				check(NumNodes > 0);
				// Log some information about the available D3D12 adapters.
				DXGI_ADAPTER_DESC AdapterDesc;
				VERIFYD3D12RESULT(TempAdapter->GetDesc(&AdapterDesc));
				uint32 OutputCount = CountAdapterOutputs(TempAdapter);

				UE_LOG(LogD3D12RHI, Log,
					TEXT("Found D3D12 adapter %u: %s (Max supported Feature Level %s)"),
					AdapterIndex,
					AdapterDesc.Description,
					GetFeatureLevelString(MaxSupportedFeatureLevel)
					);
				UE_LOG(LogD3D12RHI, Log,
					TEXT("Adapter has %uMB of dedicated video memory, %uMB of dedicated system memory, and %uMB of shared system memory, %d output[s]"),
					(uint32)(AdapterDesc.DedicatedVideoMemory / (1024*1024)),
					(uint32)(AdapterDesc.DedicatedSystemMemory / (1024*1024)),
					(uint32)(AdapterDesc.SharedSystemMemory / (1024*1024)),
					OutputCount
					);


				bool bIsAMD = AdapterDesc.VendorId == 0x1002;
				bool bIsIntel = AdapterDesc.VendorId == 0x8086;
				bool bIsNVIDIA = AdapterDesc.VendorId == 0x10DE;
				bool bIsWARP = AdapterDesc.VendorId == 0x1414;

				if (bIsAMD) bIsAnyAMD = true;
				if (bIsIntel) bIsAnyIntel = true;
				if (bIsNVIDIA) bIsAnyNVIDIA = true;

				// Simple heuristic but without profiling it's hard to do better
				const bool bIsIntegrated = bIsIntel;
				// PerfHUD is for performance profiling
				const bool bIsPerfHUD = !FCString::Stricmp(AdapterDesc.Description, TEXT("NVIDIA PerfHUD"));

				FD3D12AdapterDesc CurrentAdapter(AdapterDesc, AdapterIndex, MaxSupportedFeatureLevel, NumNodes);
				
				// Requested WARP, reject all other adapters.
				const bool bSkipRequestedWARP = bRequestedWARP && !bIsWARP;

				// we don't allow the PerfHUD adapter
				const bool bSkipPerfHUDAdapter = bIsPerfHUD && !bAllowPerfHUD;

				// the HMD wants a specific adapter, not this one
				const bool bSkipHmdGraphicsAdapter = HmdGraphicsAdapterLuid != 0 && FMemory::Memcmp(&HmdGraphicsAdapterLuid, &AdapterDesc.AdapterLuid, sizeof(LUID)) != 0;

				// the user wants a specific adapter, not this one
				const bool bSkipExplicitAdapter = CVarExplicitAdapterValue >= 0 && AdapterIndex != CVarExplicitAdapterValue;

				const bool bSkipAdapter = bSkipRequestedWARP || bSkipPerfHUDAdapter || bSkipHmdGraphicsAdapter || bSkipExplicitAdapter;

				if (!bSkipAdapter)
				{
					if (!bIsIntegrated && !FirstWithoutIntegratedAdapter.IsValid())
					{
						FirstWithoutIntegratedAdapter = CurrentAdapter;
					}
					else if (PreferredVendor == AdapterDesc.VendorId && FirstWithoutIntegratedAdapter.IsValid())
					{
						FirstWithoutIntegratedAdapter = CurrentAdapter;
					}

					if (!FirstAdapter.IsValid())
					{
						FirstAdapter = CurrentAdapter;
					}
					else if (PreferredVendor == AdapterDesc.VendorId && FirstAdapter.IsValid())
					{
						FirstAdapter = CurrentAdapter;
					}
				}
			}
		}
	}

	TSharedPtr<FD3D12Adapter> NewAdapter;
	if (bFavorNonIntegrated && (bIsAnyAMD || bIsAnyNVIDIA))
	{
		// We assume Intel is integrated graphics (slower than discrete) than NVIDIA or AMD cards and rather take a different one
		if (FirstWithoutIntegratedAdapter.IsValid())
		{
			NewAdapter = TSharedPtr<FD3D12Adapter>(new FD3D12Adapter(FirstWithoutIntegratedAdapter));
			ChosenAdapters.Add(NewAdapter);
		}
		else
		{
			NewAdapter = TSharedPtr<FD3D12Adapter>(new FD3D12Adapter(FirstAdapter));
			ChosenAdapters.Add(NewAdapter);
		}
	}
	else
	{
		NewAdapter = TSharedPtr<FD3D12Adapter>(new FD3D12Adapter(FirstAdapter));
		ChosenAdapters.Add(NewAdapter);
	}

	if (ChosenAdapters.Num() > 0 && ChosenAdapters[0]->GetDesc().IsValid())
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("Chosen D3D12 Adapter Id = %u"), ChosenAdapters[0]->GetAdapterIndex());
	}
	else
	{
		UE_LOG(LogD3D12RHI, Error, TEXT("Failed to choose a D3D12 Adapter."));
	}
}

FDynamicRHI* FD3D12DynamicRHIModule::CreateRHI(ERHIFeatureLevel::Type RequestedFeatureLevel)
{
	ERHIFeatureLevel::Type PreviewFeatureLevel;
	if (!GIsEditor && RHIGetPreviewFeatureLevel(PreviewFeatureLevel))
	{
		check(PreviewFeatureLevel == ERHIFeatureLevel::ES2 || PreviewFeatureLevel == ERHIFeatureLevel::ES3_1);

		// ES2/3.1 feature level emulation in D3D
		GMaxRHIFeatureLevel = PreviewFeatureLevel;
		if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES2)
		{
			GMaxRHIShaderPlatform = SP_PCD3D_ES2;
		}
		else if (GMaxRHIFeatureLevel == ERHIFeatureLevel::ES3_1)
		{
			GMaxRHIShaderPlatform = SP_PCD3D_ES3_1;
		}
	}
	else if (RequestedFeatureLevel == ERHIFeatureLevel::SM4)
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM4;
		GMaxRHIShaderPlatform = SP_PCD3D_SM4;
	}
	else
	{
		GMaxRHIFeatureLevel = ERHIFeatureLevel::SM5;
		GMaxRHIShaderPlatform = SP_PCD3D_SM5;
	}

	TArray<FD3D12Adapter*> RawPointers;
	for (int32 i = 0; i < ChosenAdapters.Num(); i++)
	{
		RawPointers.Add(ChosenAdapters[i].Get());
	}

	return new FD3D12DynamicRHI(RawPointers);
}

void FD3D12DynamicRHIModule::StartupModule()
{
#if NV_AFTERMATH
	// Note - can't check device type here, we'll check for that before actually initializing Aftermath
	FString AftermathBinariesRoot = FPaths::EngineDir() / TEXT("Binaries/ThirdParty/NVIDIA/NVaftermath/Win64/");
	if (LoadLibraryW(*(AftermathBinariesRoot + "GFSDK_Aftermath_Lib.x64.dll")) == nullptr)
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("Failed to load GFSDK_Aftermath_Lib.x64.dll"));
		GDX12NVAfterMathEnabled = 0;
		return;
	}
	else
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("Aftermath initialized"));
		GDX12NVAfterMathEnabled = 1;
	}
#endif

#if USE_PIX
	static FString WindowsPixDllRelativePath = FPaths::Combine(*FPaths::EngineDir(), TEXT("Binaries/ThirdParty/Windows/DirectX/x64"));
	static FString WindowsPixDll("WinPixEventRuntime.dll");
	UE_LOG(LogD3D12RHI, Log, TEXT("Loading %s for PIX profiling (from %s)."), WindowsPixDll.GetCharArray().GetData(), WindowsPixDllRelativePath.GetCharArray().GetData());
	WindowsPixDllHandle = FPlatformProcess::GetDllHandle(*FPaths::Combine(*WindowsPixDllRelativePath, *WindowsPixDll));
	if (WindowsPixDllHandle == nullptr)
	{
		const int32 ErrorNum = FPlatformMisc::GetLastError();
		TCHAR ErrorMsg[1024];
		FPlatformMisc::GetSystemErrorMessage(ErrorMsg, 1024, ErrorNum);
		UE_LOG(LogD3D12RHI, Error, TEXT("Failed to get %s handle: %s (%d)"), WindowsPixDll.GetCharArray().GetData(), ErrorMsg, ErrorNum);
	}
#endif
}

void FD3D12DynamicRHIModule::ShutdownModule()
{
#if USE_PIX
	if (WindowsPixDllHandle)
	{
		FPlatformProcess::FreeDllHandle(WindowsPixDllHandle);
		WindowsPixDllHandle = nullptr;
	}
#endif
}

void FD3D12DynamicRHI::Init()
{
	for (FD3D12Adapter*& Adapter : ChosenAdapters)
	{
		Adapter->Initialize(this);
	}

#if UE_BUILD_DEBUG	
	SubmissionLockStalls = 0;
	DrawCount = 0;
	PresentCount = 0;
#endif

	check(!GIsRHIInitialized);

	const DXGI_ADAPTER_DESC& AdapterDesc = GetAdapter().GetD3DAdapterDesc();

	// Need to set GRHIVendorId before calling IsRHIDevice* functions
	GRHIVendorId = AdapterDesc.VendorId;

	// Initialize the AMD AGS utility library, when running on an AMD device
	if (IsRHIDeviceAMD())
	{
		check(AmdAgsContext == nullptr);
		// agsInit should be called before D3D device creation
		agsInit(&AmdAgsContext, nullptr, nullptr);
	}

	// Create a device chain for each of the adapters we have choosen. This could be a single discrete card,
	// a set discrete cards linked together (i.e. SLI/Crossfire) an Integrated device or any combination of the above
	for (FD3D12Adapter*& Adapter : ChosenAdapters)
	{
		check(Adapter->GetDesc().IsValid());
		Adapter->InitializeDevices();
	}

	uint32 AmdSupportedExtensionFlags = 0;
	if (AmdAgsContext)
	{
		// Initialize AMD driver extensions
		agsDriverExtensionsDX12_Init(AmdAgsContext, GetAdapter().GetD3DDevice(), &AmdSupportedExtensionFlags);
	}

	// Warn if we are trying to use RGP frame markers but are either running on a non-AMD device
	// or using an older AMD driver without RGP marker support
	if (GEmitRgpFrameMarkers && !IsRHIDeviceAMD())
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("Attempting to use RGP frame markers on a non-AMD device."));
	}
	else if (GEmitRgpFrameMarkers && (AmdSupportedExtensionFlags & AGS_DX12_EXTENSION_USER_MARKERS) == 0)
	{
		UE_LOG(LogD3D12RHI, Warning, TEXT("Attempting to use RGP frame markers without driver support. Update AMD driver."));
	}

	GTexturePoolSize = 0;

	GRHIAdapterName = AdapterDesc.Description;
	GRHIDeviceId = AdapterDesc.DeviceId;
	GRHIDeviceRevision = AdapterDesc.Revision;

	UE_LOG(LogD3D12RHI, Log, TEXT("    GPU DeviceId: 0x%x (for the marketing name, search the web for \"GPU Device Id\")"),
		AdapterDesc.DeviceId);

	// get driver version (todo: share with other RHIs)
	{
		FGPUDriverInfo GPUDriverInfo = FPlatformMisc::GetGPUDriverInfo(GRHIAdapterName);

		GRHIAdapterUserDriverVersion = GPUDriverInfo.UserDriverVersion;
		GRHIAdapterInternalDriverVersion = GPUDriverInfo.InternalDriverVersion;
		GRHIAdapterDriverDate = GPUDriverInfo.DriverDate;

		UE_LOG(LogD3D12RHI, Log, TEXT("    Adapter Name: %s"), *GRHIAdapterName);
		UE_LOG(LogD3D12RHI, Log, TEXT("  Driver Version: %s (internal:%s, unified:%s)"), *GRHIAdapterUserDriverVersion, *GRHIAdapterInternalDriverVersion, *GPUDriverInfo.GetUnifiedDriverVersion());
		UE_LOG(LogD3D12RHI, Log, TEXT("     Driver Date: %s"), *GRHIAdapterDriverDate);
	}

	// Issue: 32bit windows doesn't report 64bit value, we take what we get.
	FD3D12GlobalStats::GDedicatedVideoMemory = int64(AdapterDesc.DedicatedVideoMemory);
	FD3D12GlobalStats::GDedicatedSystemMemory = int64(AdapterDesc.DedicatedSystemMemory);
	FD3D12GlobalStats::GSharedSystemMemory = int64(AdapterDesc.SharedSystemMemory);

	// Total amount of system memory, clamped to 8 GB
	int64 TotalPhysicalMemory = FMath::Min(int64(FPlatformMemory::GetConstants().TotalPhysicalGB), 8ll) * (1024ll * 1024ll * 1024ll);

	// Consider 50% of the shared memory but max 25% of total system memory.
	int64 ConsideredSharedSystemMemory = FMath::Min(FD3D12GlobalStats::GSharedSystemMemory / 2ll, TotalPhysicalMemory / 4ll);

	TRefCountPtr<IDXGIAdapter3> DxgiAdapter3;
	VERIFYD3D12RESULT(GetAdapter().GetAdapter()->QueryInterface(IID_PPV_ARGS(DxgiAdapter3.GetInitReference())));
	DXGI_QUERY_VIDEO_MEMORY_INFO LocalVideoMemoryInfo;
	VERIFYD3D12RESULT(DxgiAdapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &LocalVideoMemoryInfo));
	const int64 TargetBudget = LocalVideoMemoryInfo.Budget * 0.90f;	// Target using 90% of our budget to account for some fragmentation.
	FD3D12GlobalStats::GTotalGraphicsMemory = TargetBudget;

	if (sizeof(SIZE_T) < 8)
	{
		// Clamp to 1 GB if we're less than 64-bit
		FD3D12GlobalStats::GTotalGraphicsMemory = FMath::Min(FD3D12GlobalStats::GTotalGraphicsMemory, 1024ll * 1024ll * 1024ll);
	}

	if (GPoolSizeVRAMPercentage > 0)
	{
		float PoolSize = float(GPoolSizeVRAMPercentage) * 0.01f * float(FD3D12GlobalStats::GTotalGraphicsMemory);

		// Truncate GTexturePoolSize to MB (but still counted in bytes)
		GTexturePoolSize = int64(FGenericPlatformMath::TruncToFloat(PoolSize / 1024.0f / 1024.0f)) * 1024 * 1024;

		UE_LOG(LogRHI, Log, TEXT("Texture pool is %llu MB (%d%% of %llu MB)"),
			GTexturePoolSize / 1024 / 1024,
			GPoolSizeVRAMPercentage,
			FD3D12GlobalStats::GTotalGraphicsMemory / 1024 / 1024);
	}

	RequestedTexturePoolSize = GTexturePoolSize;

	VERIFYD3D12RESULT(DxgiAdapter3->SetVideoMemoryReservation(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, FMath::Min((int64)LocalVideoMemoryInfo.AvailableForReservation, FD3D12GlobalStats::GTotalGraphicsMemory)));

#if (UE_BUILD_SHIPPING && WITH_EDITOR) && PLATFORM_WINDOWS && !PLATFORM_64BITS
	// Disable PIX for windows in the shipping editor builds
	D3DPERF_SetOptions(1);
#endif

	// Multi-threaded resource creation is always supported in DX12, but allow users to disable it.
	GRHISupportsAsyncTextureCreation = D3D12RHI_ShouldAllowAsyncResourceCreation();
	if (GRHISupportsAsyncTextureCreation)
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("Async texture creation enabled"));
	}
	else
	{
		UE_LOG(LogD3D12RHI, Log, TEXT("Async texture creation disabled: %s"),
			D3D12RHI_ShouldAllowAsyncResourceCreation() ? TEXT("no driver support") : TEXT("disabled by user"));
	}

	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES2] = SP_PCD3D_ES2;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::ES3_1] = SP_PCD3D_ES3_1;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM4] = SP_PCD3D_SM4;
	GShaderPlatformForFeatureLevel[ERHIFeatureLevel::SM5] = SP_PCD3D_SM5;

	GSupportsEfficientAsyncCompute = GRHISupportsParallelRHIExecute && IsRHIDeviceAMD();

	GSupportsDepthBoundsTest = SupportsDepthBoundsTest(this);

	GRHICommandList.GetImmediateCommandList().SetContext(GDynamicRHI->RHIGetDefaultContext());
	GRHICommandList.GetImmediateAsyncComputeCommandList().SetComputeContext(GDynamicRHI->RHIGetDefaultAsyncComputeContext());

	// Notify all initialized FRenderResources that there's a valid RHI device to create their RHI resources for now.
	for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList()); ResourceIt; ResourceIt.Next())
	{
		ResourceIt->InitRHI();
	}
	// Dynamic resources can have dependencies on static resources (with uniform buffers) and must initialized last!
	for (TLinkedList<FRenderResource*>::TIterator ResourceIt(FRenderResource::GetResourceList()); ResourceIt; ResourceIt.Next())
	{
		ResourceIt->InitDynamicRHI();
	}

	{
		GRHISupportsHDROutput = SupportsHDROutput(this);

		// Specify the desired HDR pixel format.
		// Possible values are:
		//	1) PF_FloatRGBA - FP16 format that allows for linear gamma. This is the current engine default.
		//					r.HDR.Display.ColorGamut = 2 (Rec2020 / BT2020)
		//					r.HDR.Display.OutputDevice = 5 or 6 (ScRGB)
		//	2) PF_A2B10G10R10 - Save memory vs FP16 as well as allow for possible performance improvements 
		//						in fullscreen by avoiding format conversions.
		//					r.HDR.Display.ColorGamut = 2 (Rec2020 / BT2020)
		//					r.HDR.Display.OutputDevice = 3 or 4 (ST-2084)
#if WITH_EDITOR
		GRHIHDRDisplayOutputFormat = PF_FloatRGBA;
#else
		GRHIHDRDisplayOutputFormat = PF_A2B10G10R10;
#endif
	}

	FHardwareInfo::RegisterHardwareInfo(NAME_RHI, TEXT("D3D12"));

	GRHISupportsTextureStreaming = true;
	GRHISupportsFirstInstance = true;

	// Indicate that the RHI needs to use the engine's deferred deletion queue.
	GRHINeedsExtraDeletionLatency = true;

	// There is no need to defer deletion of streaming textures
	// - Suballocated ones are defer-deleted by their allocators
	// - Standalones are added to the deferred deletion queue of its parent FD3D12Adapter
	GRHIForceNoDeletionLatencyForStreamingTextures = !!PLATFORM_WINDOWS;

#if D3D12_RHI_RAYTRACING
	GRHISupportsRayTracing = GetAdapter().GetD3DRayTracingDevice() != nullptr;
#endif

	// Set the RHI initialized flag.
	GIsRHIInitialized = true;
}

void FD3D12DynamicRHI::PostInit()
{
	if (!FPlatformProperties::RequiresCookedData() && (GRHISupportsRayTracing || GRHISupportsRHIThread))
	{
		// Make sure all global shaders are complete at this point
		extern RENDERCORE_API const int32 GlobalShaderMapId;

		TArray<int32> ShaderMapIds;
		ShaderMapIds.Add(GlobalShaderMapId);

		GShaderCompilingManager->FinishCompilation(TEXT("Global"), ShaderMapIds);
	}

	if (GRHISupportsRayTracing)
	{
		for (FD3D12Adapter*& Adapter : ChosenAdapters)
		{
			Adapter->InitializeRayTracing();
		}
	}

	if (GRHISupportsRHIThread)
	{
		SetupRecursiveResources();
	}
}

bool FD3D12DynamicRHI::IsQuadBufferStereoEnabled() const
{
	return bIsQuadBufferStereoEnabled;
}

void FD3D12DynamicRHI::DisableQuadBufferStereo()
{
	bIsQuadBufferStereoEnabled = false;
}

void FD3D12Device::Initialize()
{
	check(IsInGameThread());

#if ENABLE_RESIDENCY_MANAGEMENT
	IDXGIAdapter3* DxgiAdapter3 = nullptr;
	VERIFYD3D12RESULT(GetParentAdapter()->GetAdapter()->QueryInterface(IID_PPV_ARGS(&DxgiAdapter3)));
	D3DX12Residency::InitializeResidencyManager(ResidencyManager, GetDevice(), GetGPUIndex(), DxgiAdapter3, RESIDENCY_PIPELINE_DEPTH);
#endif // ENABLE_RESIDENCY_MANAGEMENT

	SetupAfterDeviceCreation();

}

void FD3D12Device::InitPlatformSpecific()
{
	CommandListManager = new FD3D12CommandListManager(this, D3D12_COMMAND_LIST_TYPE_DIRECT, ED3D12CommandQueueType::Default);
	CopyCommandListManager = new FD3D12CommandListManager(this, D3D12_COMMAND_LIST_TYPE_COPY, ED3D12CommandQueueType::Copy);
	AsyncCommandListManager = new FD3D12CommandListManager(this, D3D12_COMMAND_LIST_TYPE_COMPUTE, ED3D12CommandQueueType::Async);
}

void FD3D12Device::CreateSamplerInternal(const D3D12_SAMPLER_DESC& Desc, D3D12_CPU_DESCRIPTOR_HANDLE Descriptor)
{
	GetDevice()->CreateSampler(&Desc, Descriptor);
}

/**
 *	Retrieve available screen resolutions.
 *
 *	@param	Resolutions			TArray<FScreenResolutionRHI> parameter that will be filled in.
 *	@param	bIgnoreRefreshRate	If true, ignore refresh rates.
 *
 *	@return	bool				true if successfully filled the array
 */
bool FD3D12DynamicRHI::RHIGetAvailableResolutions(FScreenResolutionArray& Resolutions, bool bIgnoreRefreshRate)
{
	int32 MinAllowableResolutionX = 0;
	int32 MinAllowableResolutionY = 0;
	int32 MaxAllowableResolutionX = 10480;
	int32 MaxAllowableResolutionY = 10480;
	int32 MinAllowableRefreshRate = 0;
	int32 MaxAllowableRefreshRate = 10480;

	if (MaxAllowableResolutionX == 0) //-V547
	{
		MaxAllowableResolutionX = 10480;
	}
	if (MaxAllowableResolutionY == 0) //-V547
	{
		MaxAllowableResolutionY = 10480;
	}
	if (MaxAllowableRefreshRate == 0) //-V547
	{
		MaxAllowableRefreshRate = 10480;
	}

	FD3D12Adapter& ChosenAdapter = GetAdapter();

	HRESULT HResult = S_OK;
	TRefCountPtr<IDXGIAdapter> Adapter;
	//TODO: should only be called on display out device
	HResult = ChosenAdapter.GetDXGIFactory()->EnumAdapters(ChosenAdapter.GetAdapterIndex(), Adapter.GetInitReference());

	if (DXGI_ERROR_NOT_FOUND == HResult)
	{
		return false;
	}
	if (FAILED(HResult))
	{
		return false;
	}

	// get the description of the adapter
	DXGI_ADAPTER_DESC AdapterDesc;
	if (FAILED(Adapter->GetDesc(&AdapterDesc)))
	{
		return false;
	}

	int32 CurrentOutput = 0;
	do
	{
		TRefCountPtr<IDXGIOutput> Output;
		HResult = Adapter->EnumOutputs(CurrentOutput, Output.GetInitReference());
		if (DXGI_ERROR_NOT_FOUND == HResult)
		{
			break;
		}
		if (FAILED(HResult))
		{
			return false;
		}

		// TODO: GetDisplayModeList is a terribly SLOW call.  It can take up to a second per invocation.
		//  We might want to work around some DXGI badness here.
		DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		uint32 NumModes = 0;
		HResult = Output->GetDisplayModeList(Format, 0, &NumModes, nullptr);
		if (HResult == DXGI_ERROR_NOT_FOUND)
		{
			continue;
		}
		else if (HResult == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE)
		{
			UE_LOG(LogD3D12RHI, Fatal,
				TEXT("This application cannot be run over a remote desktop configuration")
				);
			return false;
		}

		checkf(NumModes > 0, TEXT("No display modes found for the standard format DXGI_FORMAT_R8G8B8A8_UNORM!"));

		DXGI_MODE_DESC* ModeList = new DXGI_MODE_DESC[NumModes];
		VERIFYD3D12RESULT(Output->GetDisplayModeList(Format, 0, &NumModes, ModeList));

		for (uint32 m = 0; m < NumModes; m++)
		{
			CA_SUPPRESS(6385);
			if (((int32)ModeList[m].Width >= MinAllowableResolutionX) &&
				((int32)ModeList[m].Width <= MaxAllowableResolutionX) &&
				((int32)ModeList[m].Height >= MinAllowableResolutionY) &&
				((int32)ModeList[m].Height <= MaxAllowableResolutionY)
				)
			{
				bool bAddIt = true;
				if (bIgnoreRefreshRate == false)
				{
					if (((int32)ModeList[m].RefreshRate.Numerator < MinAllowableRefreshRate * ModeList[m].RefreshRate.Denominator) ||
						((int32)ModeList[m].RefreshRate.Numerator > MaxAllowableRefreshRate * ModeList[m].RefreshRate.Denominator)
						)
					{
						continue;
					}
				}
				else
				{
					// See if it is in the list already
					for (int32 CheckIndex = 0; CheckIndex < Resolutions.Num(); CheckIndex++)
					{
						FScreenResolutionRHI& CheckResolution = Resolutions[CheckIndex];
						if ((CheckResolution.Width == ModeList[m].Width) &&
							(CheckResolution.Height == ModeList[m].Height))
						{
							// Already in the list...
							bAddIt = false;
							break;
						}
					}
				}

				if (bAddIt)
				{
					// Add the mode to the list
					int32 Temp2Index = Resolutions.AddZeroed();
					FScreenResolutionRHI& ScreenResolution = Resolutions[Temp2Index];

					ScreenResolution.Width = ModeList[m].Width;
					ScreenResolution.Height = ModeList[m].Height;
					ScreenResolution.RefreshRate = ModeList[m].RefreshRate.Numerator / ModeList[m].RefreshRate.Denominator;
				}
			}
		}

		delete[] ModeList;

		++CurrentOutput;

	// TODO: Cap at 1 for default output
	} while (CurrentOutput < 1);

	return true;
}
