// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "IPixelStreamingPlugin.h"
#include "UObject/UObjectIterator.h"
#include "Engine/GameEngine.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "Streamer.h"
#include "Windows/WindowsHWrapper.h"
#include "RenderingThread.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "RendererInterface.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "PixelStreamingInputDevice.h"
#include "PixelStreamingInputComponent.h"
#include "GameFramework/GameModeBase.h"
#include "Dom/JsonObject.h"
#include "Misc/App.h"

DEFINE_LOG_CATEGORY(PixelStreaming);
DEFINE_LOG_CATEGORY(PixelStreamingInput);
DEFINE_LOG_CATEGORY(PixelStreamingNet);
DEFINE_LOG_CATEGORY(PixelStreamingCapture);

class FPixelStreamingPlugin : public IPixelStreamingPlugin
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override
	{
		// detect hardware capabilities, init nvidia capture libs, etc

		// subscribe to engine delegates here for init / framebuffer creation / whatever
		if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			if (FSlateApplication::IsInitialized())
			{
				FSlateRenderer::FOnBackBufferReadyToPresent OnBackBufferReadyDelegate;
				OnBackBufferReadyDelegate.BindRaw(this, &FPixelStreamingPlugin::OnBackBufferReady_RenderThread);
				FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent() = OnBackBufferReadyDelegate;

				FSlateApplication::Get().GetRenderer()->OnPreResizeWindowBackBuffer().AddRaw(this, &FPixelStreamingPlugin::OnPreResizeWindowBackbuffer);
			}

		}

		FGameModeEvents::GameModePostLoginEvent.AddRaw(this, &FPixelStreamingPlugin::OnGameModePostLogin);
		FGameModeEvents::GameModeLogoutEvent.AddRaw(this, &FPixelStreamingPlugin::OnGameModeLogout);

		IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);

		FApp::SetUnfocusedVolumeMultiplier(1.0f);
	}

	virtual void ShutdownModule() override
	{
		if (FSlateApplication::IsInitialized())
		{
			FSlateApplication::Get().GetRenderer()->OnBackBufferReadyToPresent().Unbind();
			FSlateApplication::Get().GetRenderer()->OnPreResizeWindowBackBuffer().RemoveAll(this);
		}

		IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
	}

private:
	void UpdateViewport(FSceneViewport* Viewport)
	{
		FRHIViewport* const ViewportRHI = Viewport->GetViewportRHI().GetReference();
	}

	void OnBackBufferReady_RenderThread(const FTexture2DRHIRef& BackBuffer)
	{
		check(IsInRenderingThread());

		if (!Streamer)
		{
			FString IP = TEXT("0.0.0.0");
			FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingIP="), IP);
			uint16 Port = 8124;
			FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingPort="), Port);

			Streamer = MakeUnique<FStreamer>(*IP, Port, BackBuffer);
		}

		Streamer->OnFrameBufferReady(BackBuffer);
	}

	void OnPreResizeWindowBackbuffer(void* BackBuffer)
	{
		if (Streamer)
		{
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				FPixelStreamingOnPreResizeWindowBackbuffer,
				FPixelStreamingPlugin*, Plugin, this,
			{
				Plugin->OnPreResizeWindowBackbuffer_RenderThread();
			});	

			// Make sure OnPreResizeWindowBackbuffer_RenderThread is executed before continuing
			FlushRenderingCommands();
		}
	}

	void OnPreResizeWindowBackbuffer_RenderThread()
	{
		Streamer->OnPreResizeWindowBackbuffer();
	}

	virtual TSharedPtr<class IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InMessageHandler) override
	{
		InputDevice = MakeShareable(new FPixelStreamingInputDevice(InMessageHandler, InputComponents));
		return InputDevice;
	}

	virtual FPixelStreamingInputDevice& GetInputDevice() override
	{
		return *InputDevice;
	}

	virtual void AddClientConfig(TSharedRef<FJsonObject>& JsonObject) override
	{
		checkf(InputDevice.IsValid(), TEXT("No Input Device available when populating Client Config"));

		JsonObject->SetBoolField(TEXT("FakingTouchEvents"), InputDevice->IsFakingTouchEvents());

		FString PixelStreamingControlScheme;
		if (FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingControlScheme="), PixelStreamingControlScheme))
		{
			JsonObject->SetStringField(TEXT("ControlScheme"), PixelStreamingControlScheme);
		}

		float PixelStreamingFastPan;
		if (FParse::Value(FCommandLine::Get(), TEXT("PixelStreamingFastPan="), PixelStreamingFastPan))
		{
			JsonObject->SetNumberField(TEXT("FastPan"), PixelStreamingFastPan);
		}
	}

	virtual void SendResponse(const FString& Descriptor) override
	{
		Streamer->SendResponse(Descriptor);
	}

	void OnGameModePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
	{
		UWorld* NewPlayerWorld = NewPlayer->GetWorld();
		for (TObjectIterator<UPixelStreamingInputComponent> ObjIt; ObjIt; ++ObjIt)
		{
			UPixelStreamingInputComponent* InputComponent = *ObjIt;
			UWorld* InputComponentWorld = InputComponent->GetWorld();
			if (InputComponentWorld == NewPlayerWorld)
			{
				InputComponents.Push(InputComponent);
			}
		}
		if (InputComponents.Num() == 0)
		{
			UPixelStreamingInputComponent* InputComponent = NewObject<UPixelStreamingInputComponent>(NewPlayer);
			InputComponent->RegisterComponent();
			InputComponents.Push(InputComponent);
		}
	}

	void OnGameModeLogout(AGameModeBase* GameMode, AController* Exiting)
	{
		InputComponents.Empty();
	}

private:
	TUniquePtr<FStreamer> Streamer;
	FTexture2DRHIRef mResolvedFrameBuffer;
	TSharedPtr<FPixelStreamingInputDevice> InputDevice;
	TArray<UPixelStreamingInputComponent*> InputComponents;
};

IMPLEMENT_MODULE(FPixelStreamingPlugin, PixelStreaming)
