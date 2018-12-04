// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPixelStreamingPlugin.h"
#include "RHI.h"

class AController;
class AGameModeBase;
class APlayerController;
class FSceneViewport;
class FStreamer;
class UPixelStreamingInputComponent;

/**
 * This plugin allows the back buffer to be sent as a compressed video across
 * a network.
 */
class FPixelStreamingPlugin : public IPixelStreamingPlugin
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual TSharedPtr<class IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override;

	/** IPixelStreamingPlugin implementation */
	virtual FPixelStreamingInputDevice& GetInputDevice() override;
	virtual void AddClientConfig(TSharedRef<FJsonObject>& JsonObject) override;
	virtual void SendResponse(const FString& Descriptor) override;

	/**
	 * Returns a shared pointer to the device which handles pixel streaming
	 * input.
	 * @return The shared pointer to the input device.
	 */
	TSharedPtr<FPixelStreamingInputDevice> GetInputDevicePtr();

private:

	void UpdateViewport(FSceneViewport* Viewport);
	void OnBackBufferReady_RenderThread(const FTexture2DRHIRef& BackBuffer);
	void OnPreResizeWindowBackbuffer(void* BackBuffer);
	void OnPreResizeWindowBackbuffer_RenderThread();
	void OnGameModePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);
	void OnGameModeLogout(AGameModeBase* GameMode, AController* Exiting);

	TUniquePtr<FStreamer> Streamer;
	FTexture2DRHIRef mResolvedFrameBuffer;
	TSharedPtr<FPixelStreamingInputDevice> InputDevice;
	TArray<UPixelStreamingInputComponent*> InputComponents;
};
