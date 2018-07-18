// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MediaCapture.h"
#include "AJALib.h"
#include "HAL/CriticalSection.h"
#include "MediaIOCoreEncodeTime.h"
#include "Misc/FrameRate.h"
#include "AjaMediaCapture.generated.h"

class FEvent;
class UAjaMediaOutput;

/**
 * Output Media for Aja streams.
 * The output format could be any of EAjaMediaOutputPixelFormat.
 */
UCLASS(BlueprintType)
class AJAMEDIAOUTPUT_API UAjaMediaCapture : public UMediaCapture
{
	GENERATED_UCLASS_BODY()

	//~ UMediaCapture interface
public:
	virtual bool HasFinishedProcessing() const override;
protected:
	virtual bool ValidateMediaOutput() const override;
	virtual bool CaptureSceneViewportImpl(const TSharedPtr<FSceneViewport>& InSceneViewport) override;
	virtual bool CaptureRenderTargetImpl(UTextureRenderTarget2D* InRenderTarget) override;
	virtual void StopCaptureImpl(bool bAllowPendingFrameToBeProcess) override;

	virtual void OnFrameCaptured_RenderingThread(const FTimecode& InTimecode, TSharedPtr<FMediaCaptureUserData> InUserData, void* InBuffer, int32 Width, int32 Height) override;

private:
	struct FAjaOutputCallback;
	friend FAjaOutputCallback;

private:
	bool InitAJA(UAjaMediaOutput* InMediaOutput);
	void WaitForSync_RenderingThread() const;

private:
	/** Aja Port for outputting */
	AJA::AJAOutputChannel* OutputChannel;
	FAjaOutputCallback* OutputCallback;

	/** Name of this output port */
	FString PortName;

	/** Option from MediaOutput */
	bool bWaitForSyncEvent;
	bool bEncodeTimecodeInTexel;
	EMediaIOCoreEncodePixelFormat EncodePixelFormat;

	/** Saved IgnoreTextureAlpha flag from viewport */
	bool bSavedIgnoreTextureAlpha;
	bool bIgnoreTextureAlphaChanged;

	/** Selected FrameRate of this output */
	FFrameRate FrameRate;

	/** Critical section for synchronizing access to the OutputChannel */
	FCriticalSection RenderThreadCriticalSection;

	/** Event to wakeup When waiting for sync */
	FEvent* WakeUpEvent;

	/** Last frame drop count to detect count */
	uint64 LastFrameDropCount_AjaThread;
};
