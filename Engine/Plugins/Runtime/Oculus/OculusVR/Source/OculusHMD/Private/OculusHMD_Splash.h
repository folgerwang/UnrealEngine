// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "OculusHMDPrivate.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
#include "OculusHMD_GameFrame.h"
#include "OculusHMD_Layer.h"
#include "TickableObjectRenderThread.h"
#include "OculusHMDTypes.h"

namespace OculusHMD
{

class FOculusHMD;

//-------------------------------------------------------------------------------------------------
// FSplashLayer
//-------------------------------------------------------------------------------------------------

struct FSplashLayer
{
	FOculusSplashDesc Desc;
	FLayerPtr Layer;

public:
	FSplashLayer(const FOculusSplashDesc& InDesc) : Desc(InDesc) {}
	FSplashLayer(const FSplashLayer& InSplashLayer) : Desc(InSplashLayer.Desc), Layer(InSplashLayer.Layer) {}
};


//-------------------------------------------------------------------------------------------------
// FSplash
//-------------------------------------------------------------------------------------------------

class FSplash : public TSharedFromThis<FSplash>
{
protected:
	class FTicker : public FTickableObjectRenderThread, public TSharedFromThis<FTicker>
	{
	public:
		FTicker(FSplash* InSplash) : FTickableObjectRenderThread(false, true), pSplash(InSplash) {}

		virtual void Tick(float DeltaTime) override { pSplash->Tick_RenderThread(DeltaTime); }
		virtual TStatId GetStatId() const override  { RETURN_QUICK_DECLARE_CYCLE_STAT(FSplash, STATGROUP_Tickables); }
		virtual bool IsTickable() const override	{ return pSplash->IsTickable(); }
	protected:
		FSplash* pSplash;
	};

public:
	FSplash(FOculusHMD* InPlugin);
	virtual ~FSplash();

	void Tick_RenderThread(float DeltaTime);
	bool IsTickable() const { return bTickable; }
	bool IsShown() const { return bIsShown; }

	void Startup();
	void LoadSettings();
	void ReleaseResources_RHIThread();
	void PreShutdown();
	void Shutdown();

	int AddSplash(const FOculusSplashDesc&);
	void ClearSplashes();
	bool GetSplash(unsigned index, FOculusSplashDesc& OutDesc);
	void StopTicker();

	void Show();
	void Hide();

protected:
	void UnloadTextures();
	void LoadTexture(FSplashLayer& InSplashLayer);
	void UnloadTexture(FSplashLayer& InSplashLayer);

	void RenderFrame_RenderThread(FRHICommandListImmediate& RHICmdList);
	IStereoLayers::FLayerDesc StereoLayerDescFromOculusSplashDesc(FOculusSplashDesc OculusDesc);

protected:
	FOculusHMD* OculusHMD;
	FCustomPresent* CustomPresent;
	TSharedPtr<FTicker> Ticker;
	int32 FramesOutstanding;
	FCriticalSection RenderThreadLock;
	FSettingsPtr Settings;
	FGameFramePtr Frame;
	TArray<FSplashLayer> SplashLayers;
	uint32 NextLayerId;
	FLayerPtr BlackLayer;
	FLayerPtr UELayer;
	TArray<FLayerPtr> Layers_RenderThread_Input;
	TArray<FLayerPtr> Layers_RenderThread;
	TArray<FLayerPtr> Layers_RHIThread;

	// All these flags are only modified from the Game thread
	bool bInitialized;
	bool bTickable;
	bool bIsShown;

	float SystemDisplayInterval;
	double LastTimeInSeconds;
};

typedef TSharedPtr<FSplash> FSplashPtr;


} // namespace OculusHMD

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS