// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "RemoteSessionChannel.h"
#include "HAL/ThreadSafeCounter.h"


class FBackChannelOSCMessage;
class FBackChannelOSCDispatch;
class FFrameGrabber;
class FSceneViewport;
class UTexture2D;

/**
 *	A channel that captures the framebuffer on the host, encodes it as a jpg as an async task, then sends it to the client.
 *	On the client images are decoded into a double-buffered texture that can be accessed via GetHostScreen.
 */
class REMOTESESSION_API FRemoteSessionFrameBufferChannel : public IRemoteSessionChannel
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
	static FString StaticType();
	virtual FString GetType() const override { return StaticType(); }
	/* End IRemoteSessionChannel implementation */

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
	TArray<TSharedPtr<FImageData>>							IncomingDecodedImages;
	FThreadSafeCounter										NumDecodingTasks;

	UTexture2D*												DecodedTextures[2];
	int32													DecodedTextureIndex;

	/** Time we last sent an image */
	double LastSentImageTime;
	int KickedTaskCount;
	int NumSentImages;

	/** So we can manage callback lifetimes properly */
	FDelegateHandle MessageCallbackHandle;
};