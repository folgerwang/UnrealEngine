// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteSessionChannel.h"
#include "HAL/ThreadSafeCounter.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "HAL/ThreadSafeBool.h"


class FBackChannelOSCMessage;
class FBackChannelOSCDispatch;
class FFrameGrabber;
class FSceneViewport;
class UTexture2D;

/**
 *	A channel that captures the framebuffer on the host, encodes it as a jpg as an async task, then sends it to the client.
 *	On the client images are decoded into a double-buffered texture that can be accessed via GetHostScreen.
 */
class REMOTESESSION_API FRemoteSessionFrameBufferChannel : public IRemoteSessionChannel, FRunnable
{
public:

	FRemoteSessionFrameBufferChannel(ERemoteSessionChannelMode InRole, TSharedPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> InConnection);

	~FRemoteSessionFrameBufferChannel();

	/** Specifies which viewport to capture */
	void SetCaptureViewport(TSharedRef<FSceneViewport> Viewport);

	/** Specifies the quality and framerate to capture at */
	void SetCaptureQuality(int32 InQuality, int32 InFramerate);

	/** Tick this channel */
	virtual void Tick(const float InDeltaTime) override;

	UTexture2D* GetHostScreen() const;

	/* Begin IRemoteSessionChannel implementation */
	static const TCHAR* StaticType() { return TEXT("FRemoteSessionFrameBufferChannel"); }
	virtual const TCHAR* GetType() const override { return StaticType(); }
	/* End IRemoteSessionChannel implementation */

	/** Signals that the viewport was resized */
	void OnViewportResized(FVector2D NewSize);

	/** Safely create the frame grabber */
	void CreateFrameGrabber(TSharedRef<FSceneViewport> Viewport);

protected:

	/** Underlying connection */
	TWeakPtr<FBackChannelOSCConnection, ESPMode::ThreadSafe> Connection;

	/** Our role */
	ERemoteSessionChannelMode Role;

	/** Send an image to connected clients */
	void		SendImageToClients(int32 Width, int32 Height, const TArray<FColor>& ImageData);

	/** Bound to receive incoming images */
	void	ReceiveHostImage(FBackChannelOSCMessage & Message, FBackChannelOSCDispatch & Dispatch);


	/** Creates a texture to receive images into */
	void CreateTexture(const int32 InSlot, const int32 InWidth, const int32 InHeight);

	/** Release the FrameGrabber*/
	void ReleaseFrameGrabber();

	TSharedPtr<FFrameGrabber>				FrameGrabber;
	
	struct FImageData
	{
		FImageData() :
			Width(0)
			, Height(0)
			, ImageIndex(0)
		{
		}
		int32				Width;
		int32				Height;
		TArray<uint8>		ImageData;
		int32				ImageIndex;
	};

	FCriticalSection										IncomingImageMutex;
	TArray<TSharedPtr<FImageData, ESPMode::ThreadSafe>>		IncomingEncodedImages;

	FCriticalSection										DecodedImageMutex;
	TArray<TSharedPtr<FImageData, ESPMode::ThreadSafe>>		IncomingDecodedImages;
	FThreadSafeCounter										NumDecodingTasks;

	UTexture2D*												DecodedTextures[2];
	int32													DecodedTextureIndex;

	/** Time we last sent an image */
	double LastSentImageTime;
	int NumSentImages;

	/** So we can manage callback lifetimes properly */
	FDelegateHandle MessageCallbackHandle;

	/** Shows that the viewport was just resized */
	bool ViewportResized;

	/** Holds a reference to the scene viewport */
	TSharedPtr<FSceneViewport> SceneViewport;

	/* Last processed incoming image */
	int32 LastIncomingImageIndex;

	/* Last processed decode image */
	int32 LastDecodedImageIndex;

	/** Decode the incoming images on a dedicated thread */
	void	ProcessIncomingTextures();

protected:
	
	/** Background thread for decoding incoming images */
	uint32 Run();
	void StartBackgroundThread();
	void ExitBackgroundThread();

	FRunnableThread*	BackgroundThread;
	FEvent *			ScreenshotEvent;

	FThreadSafeBool		ExitRequested;
};
