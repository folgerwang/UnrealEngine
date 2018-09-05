// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Render/DisplayClusterRenderManager.h"
#include "Config/IPDisplayClusterConfigManager.h"

#include "Engine/GameViewportClient.h"
#include "Engine/GameEngine.h"
#include "Misc/DisplayClusterLog.h"
#include "DisplayClusterStrings.h"
#include "DisplayClusterOperationMode.h"

#include "Render/Devices/DisplayClusterNativePresentHandler.h"
#include "Render/Devices/Debug/DisplayClusterDeviceDebug.h"
#include "Render/Devices/Monoscopic/DisplayClusterDeviceMonoscopicOpenGL.h"
#include "Render/Devices/Monoscopic/DisplayClusterDeviceMonoscopicD3D11.h"
#include "Render/Devices/Monoscopic/DisplayClusterDeviceMonoscopicD3D12.h"
#include "Render/Devices/QuadBufferStereo/DisplayClusterDeviceQuadBufferStereoOpenGL.h"
#include "Render/Devices/QuadBufferStereo/DisplayClusterDeviceQuadBufferStereoD3D11.h"
#include "Render/Devices/QuadBufferStereo/DisplayClusterDeviceQuadBufferStereoD3D12.h"
#include "Render/Devices/SideBySide/DisplayClusterDeviceSideBySide.h"
#include "Render/Devices/TopBottom/DisplayClusterDeviceTopBottom.h"

#include "UnrealClient.h"


FDisplayClusterRenderManager::FDisplayClusterRenderManager()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
}

FDisplayClusterRenderManager::~FDisplayClusterRenderManager()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IPDisplayClusterManager
//////////////////////////////////////////////////////////////////////////////////////////////
bool FDisplayClusterRenderManager::Init(EDisplayClusterOperationMode OperationMode)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	CurrentOperationMode = OperationMode;

	return true;
}

void FDisplayClusterRenderManager::Release()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	//@note: No need to release our device. It will be released in safe way by TSharedPtr.
}

bool FDisplayClusterRenderManager::StartSession(const FString& configPath, const FString& nodeId)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	ConfigPath = configPath;
	ClusterNodeId = nodeId;

	if (!GEngine)
	{
#if !WITH_EDITOR
		UE_LOG(LogDisplayClusterRender, Error, TEXT("GEngine variable not set"));
#endif
		return false;
	}

	UE_LOG(LogDisplayClusterRender, Log, TEXT("Instantiating stereo device..."));

	FDisplayClusterDeviceBase* pDev = CreateStereoDevice();
	if (pDev)
	{
		// Store ptr for internal usage
		Device = static_cast<IDisplayClusterStereoDevice*>(pDev);
		// Set new device in the engine
		GEngine->StereoRenderingDevice = TSharedPtr<IStereoRendering, ESPMode::ThreadSafe>(static_cast<IStereoRendering*>(pDev));
	}

	// When session is starting in Editor the device won't be initialized so we avoid nullptr access here.
	return (Device ? static_cast<FDisplayClusterDeviceBase*>(Device)->Initialize() : true);
}

void FDisplayClusterRenderManager::EndSession()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);
}


//////////////////////////////////////////////////////////////////////////////////////////////
// IDisplayClusterRenderManager
//////////////////////////////////////////////////////////////////////////////////////////////
IDisplayClusterStereoDevice* FDisplayClusterRenderManager::GetStereoDevice() const
{
	return Device;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterRenderManager
//////////////////////////////////////////////////////////////////////////////////////////////
FDisplayClusterDeviceBase* FDisplayClusterRenderManager::CreateStereoDevice()
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	FDisplayClusterDeviceBase* pDevice = nullptr;

	if (CurrentOperationMode == EDisplayClusterOperationMode::Cluster || CurrentOperationMode == EDisplayClusterOperationMode::Standalone)
	{
		if (GDynamicRHI == nullptr)
		{
			UE_LOG(LogDisplayClusterRender, Error, TEXT("GDynamicRHI is null. Cannot detect RHI name."));
			return nullptr;
		}

		// Depending on RHI name we will be using non-RHI-agnostic rendering devices
		const FString RHIName = GDynamicRHI->GetName();
		UE_LOG(LogDisplayClusterRender, Log, TEXT("Running %s RHI"), *RHIName);

		// Debug stereo device is RHI agnostic
		if (FParse::Param(FCommandLine::Get(), DisplayClusterStrings::args::dev::Debug))
		{
			UE_LOG(LogDisplayClusterRender, Log, TEXT("Instantiating debug stereo device..."));
			pDevice = new FDisplayClusterDeviceDebug;
		}
		// Side-by-side device is RHI agnostic
		else if (FParse::Param(FCommandLine::Get(), DisplayClusterStrings::args::dev::SbS))
		{
			UE_LOG(LogDisplayClusterRender, Log, TEXT("Instantiating side-by-side stereo device..."));
			pDevice = new FDisplayClusterDeviceSideBySide;
		}
		// Top-bottom device is RHI agnostic
		else if (FParse::Param(FCommandLine::Get(), DisplayClusterStrings::args::dev::TB))
		{
			UE_LOG(LogDisplayClusterRender, Log, TEXT("Instantiating top-bottom stereo device..."));
			pDevice = new FDisplayClusterDeviceTopBottom;
		}
		// Quad buffer stereo
		else if (FParse::Param(FCommandLine::Get(), DisplayClusterStrings::args::dev::QBS))
		{
			if (RHIName.Compare(DisplayClusterStrings::rhi::OpenGL, ESearchCase::IgnoreCase) == 0)
			{
				UE_LOG(LogDisplayClusterRender, Log, TEXT("Instantiating OpenGL quad buffer stereo device..."));
				pDevice = new FDisplayClusterDeviceQuadBufferStereoOpenGL;
			}
			else if (RHIName.Compare(DisplayClusterStrings::rhi::D3D11, ESearchCase::IgnoreCase) == 0)
			{
				UE_LOG(LogDisplayClusterRender, Log, TEXT("Instantiating D3D11 quad buffer stereo device..."));
				pDevice = new FDisplayClusterDeviceQuadBufferStereoD3D11;
			}
			else if (RHIName.Compare(DisplayClusterStrings::rhi::D3D12, ESearchCase::IgnoreCase) == 0)
			{
				UE_LOG(LogDisplayClusterRender, Log, TEXT("Instantiating D3D12 quad buffer stereo device..."));
				pDevice = new FDisplayClusterDeviceQuadBufferStereoD3D12;
			}
		}
		// Monoscopic
		else if (FParse::Param(FCommandLine::Get(), DisplayClusterStrings::args::dev::Mono))
		{
			if (RHIName.Compare(DisplayClusterStrings::rhi::OpenGL, ESearchCase::IgnoreCase) == 0)
			{
				UE_LOG(LogDisplayClusterRender, Log, TEXT("Instantiating OpenGL monoscopic device..."));
				pDevice = new FDisplayClusterDeviceMonoscopicOpenGL;
			}
			else if (RHIName.Compare(DisplayClusterStrings::rhi::D3D11, ESearchCase::IgnoreCase) == 0)
			{
				UE_LOG(LogDisplayClusterRender, Log, TEXT("Instantiating DX11 monoscopic device..."));
				pDevice = new FDisplayClusterDeviceMonoscopicD3D11;
			}
			else if (RHIName.Compare(DisplayClusterStrings::rhi::D3D12, ESearchCase::IgnoreCase) == 0)
			{
				UE_LOG(LogDisplayClusterRender, Log, TEXT("Instantiating DX12 monoscopic device..."));
				pDevice = new FDisplayClusterDeviceMonoscopicD3D12;
			}
		}
		// Leave native render but inject custom present for cluster synchronization
		else
		{
			UGameViewportClient::OnViewportCreated().AddRaw(this, &FDisplayClusterRenderManager::OnViewportCreatedHandler);
		}

		if (pDevice == nullptr)
		{
			UE_LOG(LogDisplayClusterRender, Error, TEXT("No stereo device created"));
		}
	}
	else if (CurrentOperationMode == EDisplayClusterOperationMode::Editor)
	{
		// No stereo in editor
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("DisplayCluster stereo devices for editor mode are not allowed currently"));
	}
	else if (CurrentOperationMode == EDisplayClusterOperationMode::Disabled)
	{
		// Stereo device is not needed
		UE_LOG(LogDisplayClusterRender, Log, TEXT("No need to instantiate stereo device"));
	}
	else
	{
		UE_LOG(LogDisplayClusterRender, Warning, TEXT("Unknown operation mode"));
	}

	return pDevice;
}

void FDisplayClusterRenderManager::OnViewportCreatedHandler()
{
	if (GEngine && GEngine->GameViewport)
	{
		if (!GEngine->GameViewport->Viewport->GetViewportRHI().IsValid())
		{
			GEngine->GameViewport->OnBeginDraw().AddRaw(this, &FDisplayClusterRenderManager::OnBeginDrawHandler);
		}
	}
}

void FDisplayClusterRenderManager::OnBeginDrawHandler()
{
	//@todo: this is fast solution for prototype. We shouldn't use raw handlers to be able to unsubscribe from the event.
	static bool initialized = false;
	if (!initialized && GEngine->GameViewport->Viewport->GetViewportRHI().IsValid())
	{
		NativePresentHandler  = new FDisplayClusterNativePresentHandler;
		GEngine->GameViewport->Viewport->GetViewportRHI().GetReference()->SetCustomPresent(NativePresentHandler);
		initialized = true;
	}
}

void FDisplayClusterRenderManager::PreTick(float DeltaSeconds)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	// Adjust position and size of game window to match window config.
	// This needs to happen after UGameEngine::SwitchGameWindowToUseGameViewport
	// is called. In practice that happens from FEngineLoop::Init after a call
	// to UGameEngine::Start - therefore this is done in PreTick on the first frame.
	if (!bWindowAdjusted)
	{
		bWindowAdjusted = true;

//#ifdef  DISPLAY_CLUSTER_USE_DEBUG_STANDALONE_CONFIG
#if 0
		if (GDisplayCluster->GetPrivateConfigMgr()->IsRunningDebugAuto())
		{
			UE_LOG(LogDisplayClusterRender, Log, TEXT("Running in debug auto mode. Adjusting window..."));
			ResizeWindow(DisplayClusterConstants::misc::DebugAutoWinX, DisplayClusterConstants::misc::DebugAutoWinY, DisplayClusterConstants::misc::DebugAutoResX, DisplayClusterConstants::misc::DebugAutoResY);
			return;
		}
#endif

		if (FParse::Param(FCommandLine::Get(), TEXT("windowed")))
		{
			int32 WinX = 0;
			int32 WinY = 0;
			int32 ResX = 0;
			int32 ResY = 0;

			if (FParse::Value(FCommandLine::Get(), TEXT("WinX="), WinX) &&
				FParse::Value(FCommandLine::Get(), TEXT("WinY="), WinY) &&
				FParse::Value(FCommandLine::Get(), TEXT("ResX="), ResX) &&
				FParse::Value(FCommandLine::Get(), TEXT("ResY="), ResY))
			{
				ResizeWindow(WinX, WinY, ResX, ResY);
			}
			else
			{
				UE_LOG(LogDisplayClusterRender, Error, TEXT("Wrong window pos/size arguments"));
			}
		}
	}
}

void FDisplayClusterRenderManager::ResizeWindow(int32 WinX, int32 WinY, int32 ResX, int32 ResY)
{
	DISPLAY_CLUSTER_FUNC_TRACE(LogDisplayClusterRender);

	UGameEngine* engine = Cast<UGameEngine>(GEngine);
	TSharedPtr<SWindow> window = engine->GameViewportWindow.Pin();
	check(window.IsValid());

	UE_LOG(LogDisplayClusterRender, Log, TEXT("Adjusting game window: pos [%d, %d],  size [%d x %d]"), WinX, WinY, ResX, ResY);

	// Adjust window position/size
	window->ReshapeWindow(FVector2D(WinX, WinY), FVector2D(ResX, ResY));
}
