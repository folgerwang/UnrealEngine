// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AutomationBlueprintFunctionLibrary.h"
#include "HAL/IConsoleManager.h"
#include "Misc/AutomationTest.h"
#include "EngineGlobals.h"
#include "UnrealClient.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Texture.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#if WITH_EDITOR
#include "Editor/EditorEngine.h"
#endif
#include "Tests/AutomationCommon.h"
#include "Logging/MessageLog.h"
#include "TakeScreenshotAfterTimeLatentAction.h"
#include "HighResScreenshot.h"
#include "Slate/SceneViewport.h"
#include "Tests/AutomationTestSettings.h"
#include "Slate/WidgetRenderer.h"
#include "DelayAction.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "ShaderCompiler.h"
#include "AutomationBlueprintFunctionLibrary.h"
#include "BufferVisualizationData.h"
#include "Engine/LocalPlayer.h"
#include "ContentStreaming.h"
#include "Stats/StatsData.h"
#include "HAL/PlatformProperties.h"
#include "IAutomationControllerModule.h"
#include "Scalability.h"
#include "SceneViewExtension.h"
#include "SceneView.h"

#define LOCTEXT_NAMESPACE "Automation"

DEFINE_LOG_CATEGORY_STATIC(BlueprintAssertion, Error, Error)
DEFINE_LOG_CATEGORY_STATIC(AutomationFunctionLibrary, Log, Log)

static TAutoConsoleVariable<int32> CVarAutomationScreenshotResolutionWidth(
	TEXT("AutomationScreenshotResolutionWidth"),
	0,
	TEXT("The width of automation screenshots."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarAutomationScreenshotResolutionHeight(
	TEXT("AutomationScreenshotResolutionHeight"),
	0,
	TEXT("The height of automation screenshots."),
	ECVF_Default);

#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)

template<typename T>
FConsoleVariableSwapperTempl<T>::FConsoleVariableSwapperTempl(FString InConsoleVariableName)
	: bModified(false)
	, ConsoleVariableName(InConsoleVariableName)
{
}

template<typename T>
void FConsoleVariableSwapperTempl<T>::Set(T Value)
{
	IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*ConsoleVariableName);
	if (ensure(ConsoleVariable))
	{
		if (bModified == false)
		{
			bModified = true;
			OriginalValue = ConsoleVariable->GetInt();
		}

		ConsoleVariable->AsVariable()->SetWithCurrentPriority(Value);
	}
}

template<>
void FConsoleVariableSwapperTempl<float>::Set(float Value)
{
	IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*ConsoleVariableName);
	if (ensure(ConsoleVariable))
	{
		if (bModified == false)
		{
			bModified = true;
			OriginalValue = ConsoleVariable->GetFloat();
		}

		// I need these overrides to superseded anything the user does while taking the shot.
		ConsoleVariable->AsVariable()->SetWithCurrentPriority(Value);
	}
}

template<typename T>
void FConsoleVariableSwapperTempl<T>::Restore()
{
	if (bModified)
	{
		IConsoleVariable* ConsoleVariable = IConsoleManager::Get().FindConsoleVariable(*ConsoleVariableName);
		if (ensure(ConsoleVariable))
		{
			// First we stomp the current with the original, then restore the original flags
			// so that code continues to treat it using whatever source it was from originally, code, cmdline..etc.
			ConsoleVariable->AsVariable()->SetWithCurrentPriority(OriginalValue);
		}

		bModified = false;
	}
}

class FAutomationViewExtension : public FSceneViewExtensionBase
{
public:
	FAutomationViewExtension(const FAutoRegister& AutoRegister, UWorld* InWorld, FAutomationScreenshotOptions& InOptions, float InCurrentTimeToSimulate)
		: FSceneViewExtensionBase(AutoRegister)
		, WorldPtr(InWorld)
		, Options(InOptions)
		, CurrentTime(InCurrentTimeToSimulate)
	{
	}
	
	/** ISceneViewExtension interface */
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView)
	{
		//if (Options.VisualizeBuffer != NAME_None)
		//{
		//	InViewFamily.ViewMode = VMI_VisualizeBuffer;
		//	InViewFamily.EngineShowFlags.SetVisualizeBuffer(true);
		//	InViewFamily.EngineShowFlags.SetTonemapper(false);

		//	if (GetBufferVisualizationData().GetMaterial(Options.VisualizeBuffer) == NULL)
		//	{
		//		InView.CurrentBufferVisualizationMode = Options.VisualizeBuffer;
		//	}
		//}
	}

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override
	{
		if (UAutomationViewSettings* ViewSettings = Options.ViewSettings)
		{
			// Turn off common show flags for noisy sources of rendering.
			FEngineShowFlags& ShowFlags = InViewFamily.EngineShowFlags;
			ShowFlags.SetAntiAliasing(ViewSettings->AntiAliasing);
			ShowFlags.SetMotionBlur(ViewSettings->MotionBlur);
			ShowFlags.SetTemporalAA(ViewSettings->TemporalAA);
			ShowFlags.SetScreenSpaceReflections(ViewSettings->ScreenSpaceReflections);
			ShowFlags.SetScreenSpaceAO(ViewSettings->ScreenSpaceAO);
			ShowFlags.SetDistanceFieldAO(ViewSettings->DistanceFieldAO);
			ShowFlags.SetContactShadows(ViewSettings->ContactShadows);
			ShowFlags.SetEyeAdaptation(ViewSettings->EyeAdaptation);
			ShowFlags.SetBloom(ViewSettings->Bloom);
		}

		if (Options.bOverride_OverrideTimeTo)
		{
			// Turn off time the ultimate source of noise.
			InViewFamily.CurrentWorldTime = Options.OverrideTimeTo;
			InViewFamily.CurrentRealTime = Options.OverrideTimeTo;
			InViewFamily.DeltaWorldTime = 0;
		}

		if (Options.bDisableNoisyRenderingFeatures)
		{
			//// Turn off common show flags for noisy sources of rendering.
			//InViewFamily.EngineShowFlags.SetAntiAliasing(false);
			//InViewFamily.EngineShowFlags.SetMotionBlur(false);
			//InViewFamily.EngineShowFlags.SetTemporalAA(false);
			//InViewFamily.EngineShowFlags.SetScreenSpaceReflections(false);
			////InViewFamily.EngineShowFlags.SetScreenSpaceAO(false);
			////InViewFamily.EngineShowFlags.SetDistanceFieldAO(false);
			//InViewFamily.EngineShowFlags.SetContactShadows(false);
			//InViewFamily.EngineShowFlags.SetEyeAdaptation(false);

			//TODO Auto Exposure?
			//TODO EyeAdaptation Gamma?

			// Disable screen percentage.
			//InViewFamily.EngineShowFlags.SetScreenPercentage(false);
		}
		
		if (Options.bDisableTonemapping)
		{
			//InViewFamily.EngineShowFlags.SetEyeAdaptation(false);
			//InViewFamily.EngineShowFlags.SetTonemapper(false);
		}
	}

	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) {}
	virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) {}
	virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) {}

	virtual bool IsActiveThisFrame(class FViewport* InViewport) const
	{
		if (InViewport)
		{
			FViewportClient* Client = InViewport->GetClient();
			if (Client)
			{
				return WorldPtr->GetWorld() == Client->GetWorld();
			}
		}

		return false;
	}

	/** We always want to go last. */
	virtual int32 GetPriority() const override { return MIN_int32; }

private:
	TWeakObjectPtr<UWorld> WorldPtr;
	FAutomationScreenshotOptions Options;
	float CurrentTime;
};

FAutomationTestScreenshotEnvSetup::FAutomationTestScreenshotEnvSetup()
	: DefaultFeature_AntiAliasing(TEXT("r.DefaultFeature.AntiAliasing"))
	, DefaultFeature_AutoExposure(TEXT("r.DefaultFeature.AutoExposure"))
	, DefaultFeature_MotionBlur(TEXT("r.DefaultFeature.MotionBlur"))
	, PostProcessAAQuality(TEXT("r.PostProcessAAQuality"))
	, MotionBlurQuality(TEXT("r.MotionBlurQuality"))
	, ScreenSpaceReflectionQuality(TEXT("r.SSR.Quality"))
	, EyeAdaptationQuality(TEXT("r.EyeAdaptationQuality"))
	, ContactShadows(TEXT("r.ContactShadows"))
	, TonemapperGamma(TEXT("r.TonemapperGamma"))
	, TonemapperSharpen(TEXT("r.Tonemapper.Sharpen"))
	, SecondaryScreenPercentage(TEXT("r.SecondaryScreenPercentage.GameViewport"))
{
}

FAutomationTestScreenshotEnvSetup::~FAutomationTestScreenshotEnvSetup()
{
}

void FAutomationTestScreenshotEnvSetup::Setup(UWorld* InWorld, FAutomationScreenshotOptions& InOutOptions)
{
	check(IsInGameThread());

	WorldPtr = InWorld;

	if (InOutOptions.bDisableNoisyRenderingFeatures)
	{
		DefaultFeature_AntiAliasing.Set(0);
		DefaultFeature_AutoExposure.Set(0);
		DefaultFeature_MotionBlur.Set(0);
		PostProcessAAQuality.Set(0);
		MotionBlurQuality.Set(0);
		ScreenSpaceReflectionQuality.Set(0);
		ContactShadows.Set(0);
		EyeAdaptationQuality.Set(0);
		TonemapperGamma.Set(2.2f);
		//TonemapperSharpen.Set(0);
	}
	else if (InOutOptions.bDisableTonemapping)
	{
		EyeAdaptationQuality.Set(0);
		TonemapperGamma.Set(2.2f);
		//TonemapperSharpen.Set(0);
	}

	// Ignore High-DPI settings
	SecondaryScreenPercentage.Set(100.f); 

	InOutOptions.SetToleranceAmounts(InOutOptions.Tolerance);

	const float InCurrentTimeToSimulate = 0.0f;
	AutomationViewExtension = FSceneViewExtensions::NewExtension<FAutomationViewExtension>(InWorld, InOutOptions, InCurrentTimeToSimulate);

	// TODO - I don't like needing to set this here.  Because the gameviewport uses a console variable, it wins.
	if (UGameViewportClient* ViewportClient = GEngine->GameViewport)
	{
		static IConsoleVariable* ICVar = IConsoleManager::Get().FindConsoleVariable(FBufferVisualizationData::GetVisualizationTargetConsoleCommandName());
		if (ICVar)
		{
			if (ViewportClient->GetEngineShowFlags())
			{
				ViewportClient->GetEngineShowFlags()->SetVisualizeBuffer(InOutOptions.VisualizeBuffer == NAME_None ? false : true);
				ViewportClient->GetEngineShowFlags()->SetTonemapper(InOutOptions.VisualizeBuffer == NAME_None ? true : false);
				ICVar->Set(*InOutOptions.VisualizeBuffer.ToString());
			}
		}
	}
}

void FAutomationTestScreenshotEnvSetup::Restore()
{
	check(IsInGameThread());

	DefaultFeature_AntiAliasing.Restore();
	DefaultFeature_AutoExposure.Restore();
	DefaultFeature_MotionBlur.Restore();
	PostProcessAAQuality.Restore();
	MotionBlurQuality.Restore();
	ScreenSpaceReflectionQuality.Restore();
	EyeAdaptationQuality.Restore();
	ContactShadows.Restore();
	TonemapperGamma.Restore();
	//TonemapperSharpen.Restore();
	SecondaryScreenPercentage.Restore();

	AutomationViewExtension.Reset();

	if (UGameViewportClient* ViewportClient = GEngine->GameViewport)
	{
		static IConsoleVariable* ICVar = IConsoleManager::Get().FindConsoleVariable(FBufferVisualizationData::GetVisualizationTargetConsoleCommandName());
		if (ICVar)
		{
			if (ViewportClient->GetEngineShowFlags())
			{
				ViewportClient->GetEngineShowFlags()->SetVisualizeBuffer(false);
				ViewportClient->GetEngineShowFlags()->SetTonemapper(true);
				ICVar->Set(TEXT(""));
			}
		}
	}
}

class FAutomationScreenshotTaker
{
public:
	FAutomationScreenshotTaker(UWorld* InWorld, const FString& InName, const FString& InNotes, FAutomationScreenshotOptions InOptions)
		: World(InWorld)
		, Name(InName)
		, Notes(InNotes)
		, Options(InOptions)
		, bNeedsViewportSizeRestore(false)
	{
		EnvSetup.Setup(InWorld, Options);

		if (!FPlatformProperties::HasFixedResolution())
		{
			FSceneViewport* GameViewport = GEngine->GameViewport ? GEngine->GameViewport->GetGameViewport() : nullptr;
			if (GameViewport)
			{
#if WITH_EDITOR
				// In the editor we can only attempt to re-size standalone viewports
				UEditorEngine* EditorEngine = Cast<UEditorEngine>(GEngine);	

				const bool bIsPIEViewport = GameViewport->IsPlayInEditorViewport();	
				const bool bIsNewViewport = InWorld && EditorEngine && EditorEngine->WorldIsPIEInNewViewport(InWorld);

				if (!bIsPIEViewport || bIsNewViewport)
#endif		
				{
					ViewportRestoreSize = GameViewport->GetSize();
					FIntPoint ScreenshotViewportSize = UAutomationBlueprintFunctionLibrary::GetAutomationScreenshotSize(InOptions);
					GameViewport->SetViewportSize(ScreenshotViewportSize.X, ScreenshotViewportSize.Y);
					bNeedsViewportSizeRestore = true;
				}
			}
		}

		FlushRenderingCommands();

		GEngine->GameViewport->OnScreenshotCaptured().AddRaw(this, &FAutomationScreenshotTaker::GrabScreenShot);
		FWorldDelegates::LevelRemovedFromWorld.AddRaw(this, &FAutomationScreenshotTaker::WorldDestroyed);
		FScreenshotRequest::OnScreenshotRequestProcessed().AddRaw(this, &FAutomationScreenshotTaker::OnScreenshotProcessed);
	}

	virtual ~FAutomationScreenshotTaker()
	{
		FAutomationTestFramework::Get().OnScreenshotCompared.RemoveAll(this);
		FScreenshotRequest::OnScreenshotRequestProcessed().RemoveAll(this);

		if (GEngine->GameViewport)
		{
			GEngine->GameViewport->OnScreenshotCaptured().RemoveAll(this);
		}

		FWorldDelegates::LevelRemovedFromWorld.RemoveAll(this);

		if (!FPlatformProperties::HasFixedResolution() && bNeedsViewportSizeRestore)
		{
			if (GEngine->GameViewport)
			{
				FSceneViewport* GameViewport = GEngine->GameViewport->GetGameViewport();
				GameViewport->SetViewportSize(ViewportRestoreSize.X, ViewportRestoreSize.Y);
			}
		}

		EnvSetup.Restore();

		FAutomationTestFramework::Get().NotifyScreenshotTakenAndCompared();
	}

	void GrabScreenShot(int32 InSizeX, int32 InSizeY, const TArray<FColor>& InImageData)
	{
		check(IsInGameThread());

		if (World.IsValid())
		{
			FAutomationScreenshotData Data = AutomationCommon::BuildScreenshotData(World->GetName(), Name, InSizeX, InSizeY);

			// Copy the relevant data into the metadata for the screenshot.
			Data.bHasComparisonRules = true;
			Data.ToleranceRed = Options.ToleranceAmount.Red;
			Data.ToleranceGreen = Options.ToleranceAmount.Green;
			Data.ToleranceBlue = Options.ToleranceAmount.Blue;
			Data.ToleranceAlpha = Options.ToleranceAmount.Alpha;
			Data.ToleranceMinBrightness = Options.ToleranceAmount.MinBrightness;
			Data.ToleranceMaxBrightness = Options.ToleranceAmount.MaxBrightness;
			Data.bIgnoreAntiAliasing = Options.bIgnoreAntiAliasing;
			Data.bIgnoreColors = Options.bIgnoreColors;
			Data.MaximumLocalError = Options.MaximumLocalError;
			Data.MaximumGlobalError = Options.MaximumGlobalError;

			// Record any user notes that were made to accompany this shot.
			Data.Notes = Notes;

			bool bAttemptToCompareShot = FAutomationTestFramework::Get().OnScreenshotCaptured().ExecuteIfBound(InImageData, Data);

			UE_LOG(AutomationFunctionLibrary, Log, TEXT("Screenshot captured as %s"), *Data.Path);

			if (GIsAutomationTesting)
			{
				FAutomationTestFramework::Get().OnScreenshotCompared.AddRaw(this, &FAutomationScreenshotTaker::OnComparisonComplete);
				FScreenshotRequest::OnScreenshotRequestProcessed().RemoveAll(this);
				return;
			}
		}
		
		delete this;
	}

	void OnScreenshotProcessed()
	{
		UE_LOG(AutomationFunctionLibrary, Log, TEXT("Screenshot processed, but not compared."));

		// If it's done being processed 
		delete this;
	}

	void OnComparisonComplete(const FAutomationScreenshotCompareResults& CompareResults)
	{
		FAutomationTestFramework::Get().OnScreenshotCompared.RemoveAll(this);

		if (FAutomationTestBase* CurrentTest = FAutomationTestFramework::Get().GetCurrentTest())
		{
			CurrentTest->AddEvent(CompareResults.ToAutomationEvent(Name));
		}

		delete this;
	}

	void WorldDestroyed(ULevel* InLevel, UWorld* InWorld)
	{
		// If the InLevel is null, it's a signal that the entire world is about to disappear, so
		// go ahead and remove this widget from the viewport, it could be holding onto too many
		// dangerous actor references that won't carry over into the next world.
		if (InLevel == nullptr && InWorld == World.Get())
		{
			delete this;
		}
	}

private:

	TWeakObjectPtr<UWorld> World;
	
	FString	Name;
	FString Notes;
	FAutomationScreenshotOptions Options;

	FAutomationTestScreenshotEnvSetup EnvSetup;
	FIntPoint ViewportRestoreSize;
	bool bNeedsViewportSizeRestore;
};

#endif

UAutomationBlueprintFunctionLibrary::UAutomationBlueprintFunctionLibrary(const class FObjectInitializer& Initializer)
	: Super(Initializer)
{
}

void UAutomationBlueprintFunctionLibrary::FinishLoadingBeforeScreenshot()
{
	// Finish compiling the shaders if the platform doesn't require cooked data.
	if (!FPlatformProperties::RequiresCookedData())
	{
		GShaderCompilingManager->FinishAllCompilation();
		FModuleManager::GetModuleChecked<IAutomationControllerModule>("AutomationController").GetAutomationController()->ResetAutomationTestTimeout(TEXT("shader compilation"));
	}

	// Force all mip maps to load before taking the screenshot.
	UTexture::ForceUpdateTextureStreaming();

	IStreamingManager::Get().StreamAllResources(0.0f);

	//IStreamingManager::Get().
}

FIntPoint UAutomationBlueprintFunctionLibrary::GetAutomationScreenshotSize(const FAutomationScreenshotOptions& Options)
{
	// Fallback resolution if all else fails for screenshots.
	uint32 ResolutionX = 1280;
	uint32 ResolutionY = 720;

	// First get the default set for the project.
	UAutomationTestSettings const* AutomationTestSettings = GetDefault<UAutomationTestSettings>();
	if (AutomationTestSettings->DefaultScreenshotResolution.GetMin() > 0)
	{
		ResolutionX = (uint32)AutomationTestSettings->DefaultScreenshotResolution.X;
		ResolutionY = (uint32)AutomationTestSettings->DefaultScreenshotResolution.Y;
	}

	// If there's an override resolution, use that instead.
	if (Options.Resolution.GetMin() > 0)
	{
		ResolutionX = (uint32)Options.Resolution.X;
		ResolutionY = (uint32)Options.Resolution.Y;
	}
	else
	{
		// Failing to find an override, look for a platform override that may have been provided through the
		// device profiles setup, to configure the CVars for controlling the automation screenshot size.
		int32 OverrideWidth = CVarAutomationScreenshotResolutionWidth.GetValueOnGameThread();
		int32 OverrideHeight = CVarAutomationScreenshotResolutionHeight.GetValueOnGameThread();

		if (OverrideWidth > 0)
		{
			ResolutionX = (uint32)OverrideWidth;
		}

		if (OverrideHeight > 0)
		{
			ResolutionY = (uint32)OverrideHeight;
		}
	}

	return FIntPoint(ResolutionX, ResolutionY);
}

bool UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotInternal(UObject* WorldContextObject, const FString& Name, const FString& Notes, FAutomationScreenshotOptions Options)
{
	UAutomationBlueprintFunctionLibrary::FinishLoadingBeforeScreenshot();

#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)
	FAutomationScreenshotTaker* TempObject = new FAutomationScreenshotTaker(WorldContextObject ? WorldContextObject->GetWorld() : nullptr, Name, Notes, Options);
#endif

	FScreenshotRequest::RequestScreenshot(false);
	return true; //-V773
}

void UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshot(UObject* WorldContextObject, FLatentActionInfo LatentInfo, const FString& Name, const FString& Notes, const FAutomationScreenshotOptions& Options)
{
	if ( GIsAutomationTesting )
	{
		if ( UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) )
		{
			FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
			if ( LatentActionManager.FindExistingAction<FTakeScreenshotAfterTimeLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr )
			{
				LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FTakeScreenshotAfterTimeLatentAction(LatentInfo, Name, Notes, Options));
			}
		}
	}
	else
	{
		UE_LOG(AutomationFunctionLibrary, Log, TEXT("Screenshot not captured - screenshots are only taken during automation tests"));
	}
}

void UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotAtCamera(UObject* WorldContextObject, FLatentActionInfo LatentInfo, ACameraActor* Camera, const FString& NameOverride, const FString& Notes, const FAutomationScreenshotOptions& Options)
{
	if ( Camera == nullptr )
	{
		FMessageLog("PIE").Error(LOCTEXT("CameraRequired", "A camera is required to TakeAutomationScreenshotAtCamera"));
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(WorldContextObject, 0);

	if ( PlayerController == nullptr )
	{
		FMessageLog("PIE").Error(LOCTEXT("PlayerRequired", "A player controller is required to TakeAutomationScreenshotAtCamera"));
		return;
	}

	// Move the player, then queue up a screenshot.
	// We need to delay before the screenshot so that the motion blur has time to stop.
	PlayerController->SetViewTarget(Camera, FViewTargetTransitionParams());
	FString ScreenshotName = Camera->GetName();

	if ( !NameOverride.IsEmpty() )
	{
		ScreenshotName = NameOverride;
	}

	if ( UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull) )
	{
		ScreenshotName = FString::Printf(TEXT("%s_%s"), *World->GetName(), *ScreenshotName);

		FLatentActionManager& LatentActionManager = World->GetLatentActionManager();
		if ( LatentActionManager.FindExistingAction<FTakeScreenshotAfterTimeLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr )
		{
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FTakeScreenshotAfterTimeLatentAction(LatentInfo, ScreenshotName, Notes, Options));
		}
	}
}

bool UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotOfUI_Immediate(UObject* WorldContextObject, const FString& Name, const FAutomationScreenshotOptions& Options)
{
	UAutomationBlueprintFunctionLibrary::FinishLoadingBeforeScreenshot();

	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		if (UGameViewportClient* GameViewport = WorldContextObject->GetWorld()->GetGameViewport())
		{
			TSharedPtr<SViewport> Viewport = GameViewport->GetGameViewportWidget();
			if (Viewport.IsValid())
			{
				TArray<FColor> OutColorData;
				FIntVector OutSize;
				if (FSlateApplication::Get().TakeScreenshot(Viewport.ToSharedRef(), OutColorData, OutSize))
				{
#if (WITH_DEV_AUTOMATION_TESTS || WITH_PERF_AUTOMATION_TESTS)
					// For UI, we only care about what the final image looks like. So don't compare alpha channel.
					// In editor, scene is rendered into a PF_B8G8R8A8 RT and then copied over to the R10B10G10A2 swapchain back buffer and
					// this copy ignores alpha. In game, however, scene is directly rendered into the back buffer and the alpha values are
					// already meaningless at that stage.
					for (int32 Idx = 0; Idx < OutColorData.Num(); ++Idx)
					{
						OutColorData[Idx].A = 0xff;
					}

					// The screenshot taker deletes itself later.
					FAutomationScreenshotTaker* TempObject = new FAutomationScreenshotTaker(World, Name, TEXT(""), Options);

					FAutomationScreenshotData Data = AutomationCommon::BuildScreenshotData(World->GetName(), Name, OutSize.X, OutSize.Y);

					// Copy the relevant data into the metadata for the screenshot.
					Data.bHasComparisonRules = true;
					Data.ToleranceRed = Options.ToleranceAmount.Red;
					Data.ToleranceGreen = Options.ToleranceAmount.Green;
					Data.ToleranceBlue = Options.ToleranceAmount.Blue;
					Data.ToleranceAlpha = Options.ToleranceAmount.Alpha;
					Data.ToleranceMinBrightness = Options.ToleranceAmount.MinBrightness;
					Data.ToleranceMaxBrightness = Options.ToleranceAmount.MaxBrightness;
					Data.bIgnoreAntiAliasing = Options.bIgnoreAntiAliasing;
					Data.bIgnoreColors = Options.bIgnoreColors;
					Data.MaximumLocalError = Options.MaximumLocalError;
					Data.MaximumGlobalError = Options.MaximumGlobalError;

					GEngine->GameViewport->OnScreenshotCaptured().Broadcast(OutSize.X, OutSize.Y, OutColorData);
#endif

					return true; //-V773
				}
			}
		}
	}

	return false;
}

void UAutomationBlueprintFunctionLibrary::TakeAutomationScreenshotOfUI(UObject* WorldContextObject, FLatentActionInfo LatentInfo, const FString& Name, const FAutomationScreenshotOptions& Options)
{
	if (TakeAutomationScreenshotOfUI_Immediate(WorldContextObject, Name, Options))
	{
		FLatentActionManager& LatentActionManager = WorldContextObject->GetWorld()->GetLatentActionManager();
		if ( LatentActionManager.FindExistingAction<FTakeScreenshotAfterTimeLatentAction>(LatentInfo.CallbackTarget, LatentInfo.UUID) == nullptr )
		{
			LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, new FWaitForScreenshotComparisonLatentAction(LatentInfo));
		}
	}
}

void UAutomationBlueprintFunctionLibrary::EnableStatGroup(UObject* WorldContextObject, FName GroupName)
{
#if STATS
	if (FGameThreadStatsData* StatsData = FLatestGameThreadStatsData::Get().Latest)
	{
		const FString GroupNameString = FString(TEXT("STATGROUP_")) + GroupName.ToString();
		const FName GroupNameFull = FName(*GroupNameString, EFindName::FNAME_Find);
		if(StatsData->GroupNames.Contains(GroupNameFull))
		{
			return;
		}
	}

	if (APlayerController* TargetPC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		TargetPC->ConsoleCommand( FString(TEXT("stat ")) + GroupName.ToString() + FString(TEXT(" -nodisplay")), /*bWriteToLog=*/false);
	}
#endif
}

void UAutomationBlueprintFunctionLibrary::DisableStatGroup(UObject* WorldContextObject, FName GroupName)
{
#if STATS
	if (FGameThreadStatsData* StatsData = FLatestGameThreadStatsData::Get().Latest)
	{
		const FString GroupNameString = FString(TEXT("STATGROUP_")) + GroupName.ToString();
		const FName GroupNameFull = FName(*GroupNameString, EFindName::FNAME_Find);

		if (!StatsData->GroupNames.Contains(GroupNameFull))
		{
			return;
		}
	}

	if (APlayerController* TargetPC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		TargetPC->ConsoleCommand(FString(TEXT("stat ")) + GroupName.ToString() + FString(TEXT(" -nodisplay")), /*bWriteToLog=*/false);
	}
#endif
}

#if STATS
template <EComplexStatField::Type ValueType, bool bCallCount = false>
float HelperGetStat(FName StatName)
{
	if (FGameThreadStatsData* StatsData = FLatestGameThreadStatsData::Get().Latest)
	{
		if (const FComplexStatMessage* StatMessage = StatsData->GetStatData(StatName))
		{
			if(bCallCount)
			{
				return StatMessage->GetValue_CallCount(ValueType);	
			}
			else
			{
				return FPlatformTime::ToMilliseconds(StatMessage->GetValue_Duration(ValueType));
			}
		}
	}

#if WITH_EDITOR
	FText WarningOut = FText::Format(LOCTEXT("StatNotFound", "Could not find stat data for {0}, did you call ToggleStatGroup with enough time to capture data?"), FText::FromName(StatName));
	FMessageLog("PIE").Warning(WarningOut);
	UE_LOG(AutomationFunctionLibrary, Warning, TEXT("%s"), *WarningOut.ToString());
#endif

	return 0.f;
}
#endif

float UAutomationBlueprintFunctionLibrary::GetStatIncAverage(FName StatName)
{
#if STATS
	return HelperGetStat<EComplexStatField::IncAve>(StatName);
#else
	return 0.0f;
#endif
}

float UAutomationBlueprintFunctionLibrary::GetStatIncMax(FName StatName)
{
#if STATS
	return HelperGetStat<EComplexStatField::IncMax>(StatName);
#else
	return 0.0f;
#endif
}

float UAutomationBlueprintFunctionLibrary::GetStatExcAverage(FName StatName)
{
#if STATS
	return HelperGetStat<EComplexStatField::ExcAve>(StatName);
#else
	return 0.0f;
#endif
}

float UAutomationBlueprintFunctionLibrary::GetStatExcMax(FName StatName)
{
#if STATS
	return HelperGetStat<EComplexStatField::ExcMax>(StatName);
#else
	return 0.0f;
#endif
}

float UAutomationBlueprintFunctionLibrary::GetStatCallCount(FName StatName)
{
#if STATS
	return HelperGetStat<EComplexStatField::IncAve, /*bCallCount=*/true>(StatName);
#else
	return 0.0f;
#endif
}

bool UAutomationBlueprintFunctionLibrary::AreAutomatedTestsRunning()
{
	return GIsAutomationTesting;
}

FAutomationScreenshotOptions UAutomationBlueprintFunctionLibrary::GetDefaultScreenshotOptionsForGameplay(EComparisonTolerance Tolerance, float Delay)
{
	FAutomationScreenshotOptions Options;
	Options.Delay = Delay;
	Options.Tolerance = Tolerance;
	Options.bDisableNoisyRenderingFeatures = true;
	Options.bIgnoreAntiAliasing = true;
	Options.SetToleranceAmounts(Tolerance);

	return Options;
}

FAutomationScreenshotOptions UAutomationBlueprintFunctionLibrary::GetDefaultScreenshotOptionsForRendering(EComparisonTolerance Tolerance, float Delay)
{
	FAutomationScreenshotOptions Options;
	Options.Delay = Delay;
	Options.Tolerance = Tolerance;
	Options.bDisableNoisyRenderingFeatures = true;
	Options.bIgnoreAntiAliasing = true;
	Options.SetToleranceAmounts(Tolerance);

	return Options;
}

void UAutomationBlueprintFunctionLibrary::SetScalabilityQualityLevelRelativeToMax(UObject* WorldContextObject, int32 Value /*= 1*/)
{
	Scalability::FQualityLevels Quality;
	Quality.SetFromSingleQualityLevelRelativeToMax(Value);
	Scalability::SetQualityLevels(Quality, true);
}

void UAutomationBlueprintFunctionLibrary::SetScalabilityQualityToEpic(UObject* WorldContextObject)
{
	Scalability::FQualityLevels Quality;
	Quality.SetFromSingleQualityLevelRelativeToMax(0);
	Scalability::SetQualityLevels(Quality, true);
}

void UAutomationBlueprintFunctionLibrary::SetScalabilityQualityToLow(UObject* WorldContextObject)
{
	Scalability::FQualityLevels Quality;
	Quality.SetFromSingleQualityLevel(0);
	Scalability::SetQualityLevels(Quality, true);
}

#undef LOCTEXT_NAMESPACE
