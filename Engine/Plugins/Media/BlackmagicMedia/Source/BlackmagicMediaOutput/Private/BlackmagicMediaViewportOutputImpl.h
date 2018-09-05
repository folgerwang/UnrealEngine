// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BlackmagicLib.h"
#include "BlackmagicMediaOutput.h"
#include "BlackmagicHardwareSync.h"

#include "Engine/EngineBaseTypes.h"
#include "Misc/FrameRate.h"
#include "FrameGrabber.h"
#include "Slate/SceneViewport.h"
#include "Tickable.h"
#include "Misc/Timecode.h"

class FBlackmagicMediaViewportOutputImpl;
class FMediaIOCoreWaitVSyncThread;
class FRunnableThread;

/**
 * Custom FramePayload for the FrameGrabber
 */
struct FBlackmagicFramePayload : IFramePayload
{
	FTimecode Timecode;

	bool bUseEndFrameRenderThread;
	TWeakPtr<FBlackmagicMediaViewportOutputImpl, ESPMode::ThreadSafe> ViewportOutputImpl;

	virtual bool OnFrameReady_RenderThread(FColor* ColorBuffer, FIntPoint BufferSize, FIntPoint TargetSize) const override;
};


/**
 * Implementation of BlackmagicMediaViewportOutput
 */
class FBlackmagicMediaViewportOutputImpl 
	: public TSharedFromThis<FBlackmagicMediaViewportOutputImpl, ESPMode::ThreadSafe>
	, public FSelfRegisteringExec
{
	friend FBlackmagicFramePayload;

public:
	~FBlackmagicMediaViewportOutputImpl();

	static TSharedPtr<FBlackmagicMediaViewportOutputImpl, ESPMode::ThreadSafe> CreateShared(UBlackmagicMediaOutput* MediaOutput, TSharedPtr<FSceneViewport> SceneViewport);
	void Shutdown();

	void Tick(const FTimecode& InTimecode);
	bool HasFinishedProcessing() const;

	FFrameRate GetOutputFrameRate() const { return FrameRate; }
	
	//~ Begin FSelfRegisteringExec Interface.

	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

private:
	FBlackmagicMediaViewportOutputImpl();
	bool Initialize(UBlackmagicMediaOutput* MediaOutput, TSharedPtr<FSceneViewport> SceneViewport);

	FBlackmagicMediaViewportOutputImpl(const FBlackmagicMediaViewportOutputImpl&) = delete;
	FBlackmagicMediaViewportOutputImpl& operator=(const FBlackmagicMediaViewportOutputImpl&) = delete;

	bool InitDevice(UBlackmagicMediaOutput* MediaOutput);
	void ReleaseDevice();

	void OnEndFrame_GameThread();
	void OnEndFrame_RenderThread(const FTimecode& FrameTimecode, FColor* ColorBuffer, int32 Width, int32 Height);
	bool WaitForVSync() const;
	bool WaitForOutputFrame() const;
	void Present(const FTimecode& FrameTimecode, uint8* ColorBuffer, uint32 ColorBufferWidth, uint32 ColorBufferHeight) const;
	void VerifyFrameDropCount();

private:
	/** WaitForVSync task Runnable */
	TUniquePtr<FMediaIOCoreWaitVSyncThread> VSyncThread;

	/** WaitForVSync thread */
	TUniquePtr<FRunnableThread> VSyncRunnableThread;

	/** Delegate handle for the OnEndFrame event */
	FDelegateHandle EndFrameHandle;

	/** Grab the back buffer in the thread safe way */
	TSharedPtr<FFrameGrabber> FrameGrabber;

	/** Viewport we want to grab from */
	TWeakPtr<FSceneViewport> SceneViewport;

	/** Option from MediaOutput */
	bool bClearBuffer;
	FColor ClearBufferColor;
	bool bOutputTimecode;
	bool bCopyOnRenderThread;
	bool bWaitForOutputFrame;
	bool bWaitForVSyncEvent;

	/** Saved IgnoreTextureAlpa flag from viewport */
	bool bSavedIgnoreTextureAlpha;

	/** Current video mode */
	BlackmagicDevice::FFrameDesc FrameDesc;

	/** Info about the video mode */
	BlackmagicDevice::FFrameInfo FrameInfo;

	/** Device for outputting */
	BlackmagicDevice::FDevice Device;

	/** Port for outputting */
	BlackmagicDevice::FPort Port;

	/* Last frame drop count to detect count */
	uint64 LastFrameDropCount;
	
	/* Name of this output port */
	FString PortName;

	/* Selected FrameRate of this output. todo: Populate it with future MediaOutput FrameRate*/
	FFrameRate FrameRate;
	
	/** Enable Output Timecode Log */
	bool bIsTimecodeLogEnable;
};
