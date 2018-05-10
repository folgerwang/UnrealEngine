// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "SteamVRSplash.h"
#include "SteamVRPrivate.h"
#include "SteamVRHMD.h"

#if STEAMVR_SUPPORTED_PLATFORMS

FSteamSplashTicker::FSteamSplashTicker(class FSteamVRHMD* InSteamVRHMD)
	: FTickableObjectRenderThread(false, true)
	, SteamVRHMD(InSteamVRHMD)
{}

void FSteamSplashTicker::RegisterForMapLoad()
{
	FCoreUObjectDelegates::PreLoadMap.AddSP(this, &FSteamSplashTicker::OnPreLoadMap);
	FCoreUObjectDelegates::PostLoadMapWithWorld.AddSP(this, &FSteamSplashTicker::OnPostLoadMap);
}

void FSteamSplashTicker::UnregisterForMapLoad()
{
	FCoreUObjectDelegates::PreLoadMap.RemoveAll(this);
	FCoreUObjectDelegates::PostLoadMapWithWorld.RemoveAll(this);
};

void FSteamSplashTicker::OnPreLoadMap(const FString&)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(RegisterAsyncTick,
		FTickableObjectRenderThread*, Ticker, this,
		{
			Ticker->Register();
		});
}

void FSteamSplashTicker::OnPostLoadMap(UWorld*)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(UnregisterAsyncTick, 
		FTickableObjectRenderThread*, Ticker, this,
		{
			Ticker->Unregister();
		});
}

void FSteamSplashTicker::Tick(float DeltaTime)
{
	int32 Dummy = 0;

	// Note, that we use the fact that BridgeBaseImpl::Present only returns false when VRCompositor is null,
	// even though when used by the renderer as an indication whether normal present is needed.
	if (SteamVRHMD->bSplashIsShown && SteamVRHMD->pBridge && SteamVRHMD->pBridge->Present(Dummy))
	{
		check(!SteamVRHMD->VRCompositor);
		SteamVRHMD->VRCompositor->PostPresentHandoff();
	}
}
TStatId FSteamSplashTicker::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FSplashTicker, STATGROUP_Tickables);
}

bool FSteamSplashTicker::IsTickable() const
{
	return true;
}

#endif
