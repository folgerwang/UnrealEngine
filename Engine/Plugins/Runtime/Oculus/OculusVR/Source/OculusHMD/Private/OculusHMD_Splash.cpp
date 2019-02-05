// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_Splash.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD.h"
#include "RenderingThread.h"
#include "Misc/ScopeLock.h"
#include "OculusHMDRuntimeSettings.h"
#include "StereoLayerFunctionLibrary.h"
#if PLATFORM_ANDROID
#include "Android/AndroidJNI.h"
#include "Android/AndroidEGL.h"
#include "Android/AndroidApplication.h"
#include "OculusHMDTypes.h"
#endif

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FSplash
//-------------------------------------------------------------------------------------------------

FSplash::FSplash(FOculusHMD* InOculusHMD) :
	OculusHMD(InOculusHMD),
	CustomPresent(InOculusHMD->GetCustomPresent_Internal()),
	FramesOutstanding(0),
	NextLayerId(1),
	bInitialized(false),
	bTickable(false),
	bIsShown(false),
	SystemDisplayInterval(1 / 90.0f)
{
	// Create empty quad layer for black frame
	{
		IStereoLayers::FLayerDesc LayerDesc;
		LayerDesc.QuadSize = FVector2D(0.01f, 0.01f);
		LayerDesc.Priority = 0;
		LayerDesc.PositionType = IStereoLayers::TrackerLocked;
		LayerDesc.ShapeType = IStereoLayers::QuadLayer;
		LayerDesc.Texture = GBlackTexture->TextureRHI;
		BlackLayer = MakeShareable(new FLayer(NextLayerId++, LayerDesc));
	}

	// Create empty quad layer for black frame
	{
		IStereoLayers::FLayerDesc LayerDesc;
		LayerDesc.QuadSize = FVector2D(0.01f, 0.01f);
		LayerDesc.Priority = 0;
		LayerDesc.PositionType = IStereoLayers::TrackerLocked;
		LayerDesc.ShapeType = IStereoLayers::QuadLayer;
		LayerDesc.Texture = nullptr;
		UELayer = MakeShareable(new FLayer(NextLayerId++, LayerDesc));
	}
}


FSplash::~FSplash()
{
	// Make sure RenTicker is freed in Shutdown
	check(!Ticker.IsValid())
}

void FSplash::Tick_RenderThread(float DeltaTime)
{
	CheckInRenderThread();

	if (FramesOutstanding > 0)
	{
		UE_LOG(LogHMD, VeryVerbose, TEXT("Splash skipping frame; too many frames outstanding"));
		return;
	}

	const double TimeInSeconds = FPlatformTime::Seconds();
	const double DeltaTimeInSeconds = TimeInSeconds - LastTimeInSeconds;

	if (DeltaTimeInSeconds > 2.f * SystemDisplayInterval)
	{
		for (int32 SplashLayerIndex = 0; SplashLayerIndex < SplashLayers.Num(); SplashLayerIndex++)
		{
			FSplashLayer& SplashLayer = SplashLayers[SplashLayerIndex];

			if (SplashLayer.Layer.IsValid() && !SplashLayer.Desc.DeltaRotation.Equals(FQuat::Identity))
			{
				FScopeLock ScopeLock(&RenderThreadLock);

				IStereoLayers::FLayerDesc LayerDesc = SplashLayer.Layer->GetDesc();
				LayerDesc.Transform.SetRotation(SplashLayer.Desc.DeltaRotation * LayerDesc.Transform.GetRotation());

				FLayer* Layer = new FLayer(*SplashLayer.Layer);
				Layer->SetDesc(LayerDesc);
				SplashLayer.Layer = MakeShareable(Layer);

			}
		}
	}

	RenderFrame_RenderThread(FRHICommandListExecutor::GetImmediateCommandList());
	LastTimeInSeconds = TimeInSeconds;
}

void FSplash::LoadSettings()
{
	UOculusHMDRuntimeSettings* HMDSettings = GetMutableDefault<UOculusHMDRuntimeSettings>();
	check(HMDSettings);
	ClearSplashes();
	for (const FOculusSplashDesc& SplashDesc : HMDSettings->SplashDescs)
	{
		AddSplash(SplashDesc);
	}
	UStereoLayerFunctionLibrary::EnableAutoLoadingSplashScreen(HMDSettings->bAutoEnabled);
}

void FSplash::Startup()
{
	CheckInGameThread();

	if (!bInitialized)
	{
		Settings = OculusHMD->CreateNewSettings();
		Frame = OculusHMD->CreateNewGameFrame();
		// keep units in meters rather than UU (because UU make not much sense).
		Frame->WorldToMetersScale = 1.0f;

		float SystemDisplayFrequency;
		if (OVRP_SUCCESS(ovrp_GetSystemDisplayFrequency2(&SystemDisplayFrequency)))
		{
			SystemDisplayInterval = 1.0f / SystemDisplayFrequency;
		}

		LoadSettings();

		Ticker = MakeShareable(new FTicker(this));

		ExecuteOnRenderThread_DoNotWait([this]()
		{
			Ticker->Register();
		});

		bInitialized = true;
	}
}

void FSplash::StopTicker()
{
	FScopeLock ScopeLock(&RenderThreadLock);

	if (!IsShown())
	{
		bTickable = false;
		UnloadTextures();
	}
}

void FSplash::RenderFrame_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	CheckInRenderThread();

	FScopeLock ScopeLock(&RenderThreadLock);

	// RenderFrame
	FSettingsPtr XSettings = Settings->Clone();
	FGameFramePtr XFrame = Frame->Clone();
	XFrame->FrameNumber = OculusHMD->NextFrameNumber;
	XFrame->ShowFlags.Rendering = true;
	TArray<FLayerPtr> XLayers = Layers_RenderThread_Input;

	if (XLayers.Num()==0)
	{
		XLayers.Add(BlackLayer->Clone());
	}

	ovrpResult Result;
	UE_LOG(LogHMD, Verbose, TEXT("Splash ovrp_WaitToBeginFrame %u"), XFrame->FrameNumber);
	if (OVRP_FAILURE(Result = ovrp_WaitToBeginFrame(XFrame->FrameNumber)))
	{
		UE_LOG(LogHMD, Error, TEXT("Splash ovrp_WaitToBeginFrame %u failed (%d)"), XFrame->FrameNumber, Result);
		XFrame->ShowFlags.Rendering = false;
	}
	else
	{
		OculusHMD->NextFrameNumber++;
		FPlatformAtomics::InterlockedIncrement(&FramesOutstanding);
	}

	if (XFrame->ShowFlags.Rendering)
	{
		if (OVRP_FAILURE(Result = ovrp_Update3(ovrpStep_Render, XFrame->FrameNumber, 0.0)))
		{
			UE_LOG(LogHMD, Error, TEXT("Splash ovrp_Update3 %u failed (%d)"), XFrame->FrameNumber, Result);
		}
	}

	{
		int32 LayerIndex = 0;
		int32 LayerIndex_RenderThread = 0;

		while(LayerIndex < XLayers.Num() && LayerIndex_RenderThread < Layers_RenderThread.Num())
		{
			uint32 LayerIdA = XLayers[LayerIndex]->GetId();
			uint32 LayerIdB = Layers_RenderThread[LayerIndex_RenderThread]->GetId();

			if (LayerIdA < LayerIdB)
			{
				XLayers[LayerIndex++]->Initialize_RenderThread(XSettings.Get(), CustomPresent, RHICmdList);
			}
			else if (LayerIdA > LayerIdB)
			{
				LayerIndex_RenderThread++;
			}
			else
			{
				XLayers[LayerIndex++]->Initialize_RenderThread(XSettings.Get(), CustomPresent, RHICmdList, Layers_RenderThread[LayerIndex_RenderThread++].Get());
			}
		}

		while(LayerIndex < XLayers.Num())
		{
			XLayers[LayerIndex++]->Initialize_RenderThread(XSettings.Get(), CustomPresent, RHICmdList);
		}
	}

	Layers_RenderThread = XLayers;

	for (int32 LayerIndex = 0; LayerIndex < Layers_RenderThread.Num(); LayerIndex++)
	{
		Layers_RenderThread[LayerIndex]->UpdateTexture_RenderThread(CustomPresent, RHICmdList);
	}

	// RHIFrame
	for (int32 LayerIndex = 0; LayerIndex < XLayers.Num(); LayerIndex++)
	{
		XLayers[LayerIndex] = XLayers[LayerIndex]->Clone();
	}

	ExecuteOnRHIThread_DoNotWait([this, XSettings, XFrame, XLayers]()
	{
		ovrpResult ResultT;

		if (XFrame->ShowFlags.Rendering)
		{
			UE_LOG(LogHMD, Verbose, TEXT("Splash ovrp_BeginFrame4 %u"), XFrame->FrameNumber);
			if (OVRP_FAILURE(ResultT = ovrp_BeginFrame4(XFrame->FrameNumber, CustomPresent->GetOvrpCommandQueue())))
			{
				UE_LOG(LogHMD, Error, TEXT("Splash ovrp_BeginFrame4 %u failed (%d)"), XFrame->FrameNumber, ResultT);
				XFrame->ShowFlags.Rendering = false;
			}
		}

		FPlatformAtomics::InterlockedDecrement(&FramesOutstanding);

		Layers_RHIThread = XLayers;
		Layers_RHIThread.Sort(FLayerPtr_ComparePriority());

		if (XFrame->ShowFlags.Rendering)
		{
			TArray<const ovrpLayerSubmit*> LayerSubmitPtr;
			LayerSubmitPtr.SetNum(Layers_RHIThread.Num());

			for (int32 LayerIndex = 0; LayerIndex < Layers_RHIThread.Num(); LayerIndex++)
			{
				LayerSubmitPtr[LayerIndex] = Layers_RHIThread[LayerIndex]->UpdateLayer_RHIThread(XSettings.Get(), XFrame.Get(), LayerIndex);
			}

			UE_LOG(LogHMD, Verbose, TEXT("Splash ovrp_EndFrame4 %u"), XFrame->FrameNumber);
			if (OVRP_FAILURE(ResultT = ovrp_EndFrame4(XFrame->FrameNumber, LayerSubmitPtr.GetData(), LayerSubmitPtr.Num(), CustomPresent->GetOvrpCommandQueue())))
			{
				UE_LOG(LogHMD, Error, TEXT("Splash ovrp_EndFrame4 %u failed (%d)"), XFrame->FrameNumber, ResultT);
			}
			else
			{
				for (int32 LayerIndex = 0; LayerIndex < Layers_RHIThread.Num(); LayerIndex++)
				{
					Layers_RHIThread[LayerIndex]->IncrementSwapChainIndex_RHIThread(CustomPresent);
				}
			}
		}
	});
}

void FSplash::ReleaseResources_RHIThread()
{
	for (int32 LayerIndex = 0; LayerIndex < Layers_RenderThread.Num(); LayerIndex++)
	{
		Layers_RenderThread[LayerIndex]->ReleaseResources_RHIThread();
	}

	for (int32 LayerIndex = 0; LayerIndex < Layers_RHIThread.Num(); LayerIndex++)
	{
		Layers_RHIThread[LayerIndex]->ReleaseResources_RHIThread();
	}

	Layers_RenderThread.Reset();
	Layers_RHIThread.Reset();
}


void FSplash::PreShutdown()
{
	CheckInGameThread();

	// force Ticks to stop
	bTickable = false;
}


void FSplash::Shutdown()
{
	CheckInGameThread();

	if (bInitialized)
	{
		bTickable = false;

		ExecuteOnRenderThread([this]()
		{
			Ticker->Unregister();
			Ticker = nullptr;

			ExecuteOnRHIThread([this]()
			{
				SplashLayers.Reset();
				Layers_RenderThread.Reset();
				Layers_RHIThread.Reset();
			});
		});

		bInitialized = false;
	}
}

int FSplash::AddSplash(const FOculusSplashDesc& Desc)
{
	CheckInGameThread();

	FScopeLock ScopeLock(&RenderThreadLock);
	return SplashLayers.Add(FSplashLayer(Desc));
}


void FSplash::ClearSplashes()
{
	CheckInGameThread();

	FScopeLock ScopeLock(&RenderThreadLock);
	SplashLayers.Reset();
}


bool FSplash::GetSplash(unsigned InSplashLayerIndex, FOculusSplashDesc& OutDesc)
{
	CheckInGameThread();

	FScopeLock ScopeLock(&RenderThreadLock);
	if (InSplashLayerIndex < unsigned(SplashLayers.Num()))
	{
		OutDesc = SplashLayers[int32(InSplashLayerIndex)].Desc;
		return true;
	}
	return false;
}

IStereoLayers::FLayerDesc FSplash::StereoLayerDescFromOculusSplashDesc(FOculusSplashDesc OculusDesc)
{
	bool bIsCubemap = OculusDesc.LoadedTexture->GetTextureCube() != nullptr;

	IStereoLayers::FLayerDesc LayerDesc;
	LayerDesc.Transform = OculusDesc.TransformInMeters * FTransform(OculusHMD->GetSplashRotation().Quaternion());
	LayerDesc.QuadSize = OculusDesc.QuadSizeInMeters;
	LayerDesc.UVRect = FBox2D(OculusDesc.TextureOffset, OculusDesc.TextureOffset + OculusDesc.TextureScale);
	LayerDesc.Priority = INT32_MAX - (int32)(OculusDesc.TransformInMeters.GetTranslation().X * 1000.f);
	LayerDesc.PositionType = IStereoLayers::TrackerLocked;
	LayerDesc.ShapeType = bIsCubemap ? IStereoLayers::CubemapLayer : IStereoLayers::QuadLayer;
	LayerDesc.Texture = OculusDesc.LoadedTexture;
	LayerDesc.Flags = IStereoLayers::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO | (OculusDesc.bNoAlphaChannel ? IStereoLayers::LAYER_FLAG_TEX_NO_ALPHA_CHANNEL : 0) | (OculusDesc.bIsDynamic ? IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE : 0);

	return LayerDesc;
}

void FSplash::Show()
{
	CheckInGameThread();

	OculusHMD->InitDevice();

	// Create new textures
	UnloadTextures();

	// Make sure all UTextures are loaded and contain Resource->TextureRHI
	bool bWaitForRT = false;

	for (int32 SplashLayerIndex = 0; SplashLayerIndex < SplashLayers.Num(); ++SplashLayerIndex)
	{
		FSplashLayer& SplashLayer = SplashLayers[SplashLayerIndex];

		if (SplashLayer.Desc.TexturePath.IsValid())
		{
			// load temporary texture (if TexturePath was specified)
			LoadTexture(SplashLayer);
		}
		if (SplashLayer.Desc.LoadingTexture && SplashLayer.Desc.LoadingTexture->IsValidLowLevel())
		{
			SplashLayer.Desc.LoadingTexture->UpdateResource();
			bWaitForRT = true;
		}
	}

	if (bWaitForRT)
	{
		FlushRenderingCommands();
	}

	for (int32 SplashLayerIndex = 0; SplashLayerIndex < SplashLayers.Num(); ++SplashLayerIndex)
	{
		FSplashLayer& SplashLayer = SplashLayers[SplashLayerIndex];

		//@DBG BEGIN
		if (SplashLayer.Desc.LoadingTexture->IsValidLowLevel())
		{
			if (SplashLayer.Desc.LoadingTexture->Resource && SplashLayer.Desc.LoadingTexture->Resource->TextureRHI)
			{
				SplashLayer.Desc.LoadedTexture = SplashLayer.Desc.LoadingTexture->Resource->TextureRHI;
			}
			else
			{
				UE_LOG(LogHMD, Warning, TEXT("Splash, %s - no Resource"), *SplashLayer.Desc.LoadingTexture->GetDesc());
			}
		}
		//@DBG END

		if (SplashLayer.Desc.LoadedTexture)
		{
			SplashLayer.Layer = MakeShareable(new FLayer(NextLayerId++, StereoLayerDescFromOculusSplashDesc(SplashLayer.Desc)));
		}
	}

	{
		//add oculus-generated layers through the OculusVR settings area
		FScopeLock ScopeLock(&RenderThreadLock);
		Layers_RenderThread_Input.Reset();
		for (int32 SplashLayerIndex = 0; SplashLayerIndex < SplashLayers.Num(); SplashLayerIndex++)
		{
			const FSplashLayer& SplashLayer = SplashLayers[SplashLayerIndex];

			if (SplashLayer.Layer.IsValid())
			{
				Layers_RenderThread_Input.Add(SplashLayer.Layer->Clone());
			}
		}

		//add UE VR splash screen
		FOculusSplashDesc UESplashDesc = OculusHMD->GetUESplashScreenDesc();
		if (UESplashDesc.LoadedTexture != nullptr)
		{
			UELayer->SetDesc(StereoLayerDescFromOculusSplashDesc(UESplashDesc));
			Layers_RenderThread_Input.Add(UELayer->Clone());
		}

		Layers_RenderThread_Input.Sort(FLayerPtr_CompareId());
	}

	// If no textures are loaded, this will push black frame
	bTickable = true;
	bIsShown = true;

	UE_LOG(LogHMD, Log, TEXT("FSplash::OnShow"));
}


void FSplash::Hide()
{
	CheckInGameThread();

	UE_LOG(LogHMD, Log, TEXT("FSplash::OnHide"));
//	bTickable = false;
	bIsShown = false;
}

void FSplash::UnloadTextures()
{
	CheckInGameThread();

	// unload temporary loaded textures
	FScopeLock ScopeLock(&RenderThreadLock);
	for (int32 SplashLayerIndex = 0; SplashLayerIndex < SplashLayers.Num(); ++SplashLayerIndex)
	{
		if (SplashLayers[SplashLayerIndex].Desc.TexturePath.IsValid())
		{
			UnloadTexture(SplashLayers[SplashLayerIndex]);
		}
	}
}


void FSplash::LoadTexture(FSplashLayer& InSplashLayer)
{
	CheckInGameThread();

	UnloadTexture(InSplashLayer);

	UE_LOG(LogLoadingSplash, Log, TEXT("Loading texture for splash %s..."), *InSplashLayer.Desc.TexturePath.GetAssetName());
	InSplashLayer.Desc.LoadingTexture = Cast<UTexture>(InSplashLayer.Desc.TexturePath.TryLoad());
	if (InSplashLayer.Desc.LoadingTexture != nullptr)
	{
		UE_LOG(LogLoadingSplash, Log, TEXT("...Success. "));
	}
	InSplashLayer.Desc.LoadedTexture = nullptr;
	InSplashLayer.Layer.Reset();
}


void FSplash::UnloadTexture(FSplashLayer& InSplashLayer)
{
	CheckInGameThread();

	InSplashLayer.Desc.LoadingTexture = nullptr;
	InSplashLayer.Desc.LoadedTexture = nullptr;
	InSplashLayer.Layer.Reset();
}


} // namespace OculusHMD

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS
