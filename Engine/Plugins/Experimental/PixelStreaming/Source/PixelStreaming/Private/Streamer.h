// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "VideoEncoder.h"
#include "AudioEncoder.h"
#include "RHI.h"
#include "RHIResources.h"
#include "Engine/GameViewportClient.h"
#include "ProtocolDefs.h"

DECLARE_STATS_GROUP(TEXT("PixelStreaming"), STATGROUP_PixelStreaming, STATCAT_Advanced);

class FRenderTarget;
class FProxyConnection;
class IFileHandle;
class FSocket;
struct ID3D11Device;

class FStreamer
{
private:
	FStreamer(const FStreamer&) = delete;
	FStreamer& operator=(const FStreamer&) = delete;

public:
	FStreamer(const TCHAR* IP, uint16 Port, const FTexture2DRHIRef& FrameBuffer);
	virtual ~FStreamer();

	void OnFrameBufferReady(const FTexture2DRHIRef& FrameBuffer);
	void OnPreResizeWindowBackbuffer();
	void OnAudioPCMPacketReady(const uint8* Data, int Size);
	void ForceIdrFrame();

	void StartStreaming()
	{
		bStreamingStarted = true;
		ForceIdrFrame();
	}

	void StopStreaming()
	{
		bStreamingStarted = false;
	}

	void SetBitrate(uint16 Kbps);
	void SetFramerate(int32 Fps);

	void SendResponse(const FString& Descriptor);

private:
	void CreateVideoEncoder(const FTexture2DRHIRef& FrameBuffer);
	void SendSpsPpsHeader();
	void UpdateEncoderSettings(const FTexture2DRHIRef& FrameBuffer, int32 Fps = -1);
	void Stream(uint64 Timestamp, PixelStreamingProtocol::EToProxyMsg, const uint8* Data, uint32 Size);
	void SaveEncodedVideoToFile(PixelStreamingProtocol::EToProxyMsg PktType, const uint8* Data, uint32 Size);
	void SubmitVideoFrame(uint64 Timestamp, bool KeyFrame, const uint8* Data, uint32 Size);

private:
	bool bResizingWindowBackBuffer;
	FVideoEncoderSettings VideoEncoderSettings;
	TUniquePtr<IVideoEncoder> VideoEncoder;
	FAudioEncoder AudioEncoder;

	TUniquePtr<FProxyConnection> ProxyConnection;
	TArray<uint8> ReceiveBuffer;

	FThreadSafeBool bSendSpsPps;

	// we shouldn't start streaming immediately after WebRTC is connected because
	// encoding pipeline is not ready yet and a couple of first frames can be lost.
	// instead wait for an explicit command to start streaming
	FThreadSafeBool bStreamingStarted;

	FCriticalSection AudioVideoStreamSync;

#if !UE_BUILD_SHIPPING
	TUniquePtr<IFileHandle> EncodedVideoFile;
#endif

	int32 InitialMaxFPS;
};

