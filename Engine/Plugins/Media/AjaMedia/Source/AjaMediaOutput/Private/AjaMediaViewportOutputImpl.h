// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

#include "AJALib.h"
#include "AjaMediaOutput.h"

#include "Engine/EngineBaseTypes.h"
#include "FrameGrabber.h"
#include "IMediaControls.h"
#include "Misc/FrameRate.h"
#include "Misc/Timecode.h"
#include "Slate/SceneViewport.h"
#include "Templates/Atomic.h"


class FAjaMediaViewportOutputImpl;
class FRunnableThread;

/**
 * Custom FramePayload for the FrameGrabber
 */
struct FAjaFramePayload : IFramePayload
{
	FTimecode Timecode;

	bool bUseEndFrameRenderThread;
	TWeakPtr<FAjaMediaViewportOutputImpl, ESPMode::ThreadSafe> ViewportOutputImpl;

	virtual bool OnFrameReady_RenderThread(FColor* ColorBuffer, FIntPoint BufferSize, FIntPoint TargetSize) const override;
};


/**
 * Implementation of AjaMediaViewportOutput
 */
class FAjaMediaViewportOutputImpl
	: public TSharedFromThis<FAjaMediaViewportOutputImpl, ESPMode::ThreadSafe>
	, public FSelfRegisteringExec
	, protected AJA::IAJAInputOutputChannelCallbackInterface
{
	friend FAjaFramePayload;

public:
	static TSharedPtr<FAjaMediaViewportOutputImpl, ESPMode::ThreadSafe> CreateShared(UAjaMediaOutput* MediaOutput, TSharedPtr<FSceneViewport> SceneViewport);

	virtual ~FAjaMediaViewportOutputImpl();
	void Shutdown();

	void Tick(const FTimecode& InTimecode);
	bool HasFinishedProcessing() const;

	FFrameRate GetOutputFrameRate() const { return FrameRate; }
	
	//~ Begin FSelfRegisteringExec Interface.

	virtual bool Exec(class UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

private:
	FAjaMediaViewportOutputImpl();
	FAjaMediaViewportOutputImpl(const FAjaMediaViewportOutputImpl&) = delete;
	FAjaMediaViewportOutputImpl& operator=(const FAjaMediaViewportOutputImpl&) = delete;

	bool Initialize(UAjaMediaOutput* MediaOutput, TSharedPtr<FSceneViewport> SceneViewport);

	bool InitAJA(UAjaMediaOutput* MediaOutput);
	void ReleaseAJA();

	void OnEndFrame_GameThread();
	void OnEndFrame_RenderThread(const FTimecode& FrameTimecode, FColor* ColorBuffer, int32 Width, int32 Height);
	bool WaitForSync() const;
	void EncodeTimecode(const AJA::FTimecode& Timecode, FColor* ColorBuffer, uint32 ColorBufferWidth, uint32 ColorBufferHeight) const;
	void EncodeTimecode_Pattern(FColor* ColorBuffer, uint32 ColorBufferWidth, int32 HeightIndex, int32 Amount) const;
	void EncodeTimecode_Time(FColor* ColorBuffer, uint32 ColorBufferWidth, int32 HeightIndex, int32 Time) const;
	void SendToAja(const FTimecode& FrameTimecode, FColor* ColorBuffer, uint32 ColorBufferWidth, uint32 ColorBufferHeight);

protected:

	//~ IAJAInputCallbackInterface interface

	virtual void OnInitializationCompleted(bool bSucceed) override;
	virtual bool OnInputFrameReceived(const AJA::AJAInputFrameData& InInputFrame, const AJA::AJAAncillaryFrameData& InAncillaryFrame, const AJA::AJAAudioFrameData& AudioFrame, const AJA::AJAVideoFrameData& VideoFrame) override;
	virtual bool OnOutputFrameCopied(const AJA::AJAOutputFrameData& InFrameData) override;
	virtual void OnCompletion(bool bSucceed) override;

private:
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
	bool bWaitForSyncEvent;
	bool bEncodeTimecodeInTexel;

	/** Saved IgnoreTextureAlpa flag from viewport */
	bool bSavedIgnoreTextureAlpha;
	bool bIgnoreTextureAlphaChanged;

	/** Critical section for synchronizing access to the OutputChannel */
	FCriticalSection RenderThreadCriticalSection;

	/** Event to wakeup When waiting for sync */
	FEvent* WakeUpEvent;

	/**
	 * State of the current port
	 * Can be in Closed, Error, Playing or Preparing
	 */
	EMediaState CurrentState;
	EMediaState AjaThreadNewState;

	/** Aja Port for outputting */
	AJA::AJAOutputChannel* OutputChannel;

	/** Last frame drop count to detect count */
	TAtomic<uint64> LastFrameDropCount;
	
	/** Name of this output port */
	FString PortName;

	/** Selected FrameRate of this output. todo: Populate it with future MediaOutput FrameRate */
	FFrameRate FrameRate;

	/** Size of the buffer AJA wish to received */
	FIntPoint AjaWishResolution;

	/** Captured data to be send to the AJA */
	struct FFrameData
	{
		FTimecode Timecode;
		TArray<FColor> ColorBuffer;
		FIntPoint BufferSize;
	};
	FFrameData FrameData;

	/** Enable Output Timecode Log */
	bool bIsTimecodeLogEnable;
};
