#include "PreLoadScreenManager.h"

#include "Engine/GameEngine.h"

#include "GlobalShader.h"
#include "ShaderCompiler.h"

#include "PreLoadScreen.h"
#include "PreLoadSettingsContainer.h"

#include "HAL/ThreadManager.h"

#if PLATFORM_ANDROID
#if USE_ANDROID_EVENTS
#include "Android/AndroidEventManager.h"
#endif
#endif

DEFINE_LOG_CATEGORY_STATIC(LogPreLoadScreenManager, Log, All);

TSharedPtr<FPreLoadScreenManager> FPreLoadScreenManager::Instance;

FCriticalSection FPreLoadScreenManager::EarlyRenderingEnabledCriticalSection;
bool FPreLoadScreenManager::bEarlyRenderingEnabled = true;

void FPreLoadScreenManager::Initialize(FSlateRenderer& InSlateRenderer)
{
    if (bInitialized || !ArePreLoadScreensEnabled())       
    {
        return;
    }

    bInitialized = true;

    // Initialize shaders, because otherwise they might not be guaranteed to exist at this point
    if (!FPlatformProperties::RequiresCookedData())
    {
        TArray<int32> ShaderMapIds;
        ShaderMapIds.Add(GlobalShaderMapId);
        GShaderCompilingManager->FinishCompilation(TEXT("Global"), ShaderMapIds);
    }

    if (FApp::CanEverRender())
    {
        // Make sure we haven't created a game window already, if so use that. If not make a new one
        UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
        TSharedRef<SWindow> GameWindow = (GameEngine && GameEngine->GameViewportWindow.IsValid()) ? GameEngine->GameViewportWindow.Pin().ToSharedRef() : UGameEngine::CreateGameWindow();

        VirtualRenderWindow =
            SNew(SVirtualWindow)
            .Size(GameWindow->GetClientSizeInScreen());

        MainWindow = GameWindow;

        WidgetRenderer = MakeShared<FPreLoadSlateWidgetRenderer, ESPMode::ThreadSafe>(GameWindow, VirtualRenderWindow, &InSlateRenderer);
    }

    LastRenderTickTime = FPlatformTime::Seconds();
    LastTickTime = FPlatformTime::Seconds();
}

void FPreLoadScreenManager::RegisterPreLoadScreen(TSharedPtr<IPreLoadScreen> PreLoadScreen)
{
    PreLoadScreens.Add(PreLoadScreen);
}

void FPreLoadScreenManager::UnRegisterPreLoadScreen(TSharedPtr<IPreLoadScreen> PreLoadScreen)
{
    if (PreLoadScreen.IsValid())
    {
        PreLoadScreen->CleanUp();
        PreLoadScreens.Remove(PreLoadScreen);
    }
}

void FPreLoadScreenManager::PlayFirstPreLoadScreen(EPreLoadScreenTypes PreLoadScreenTypeToPlay)
{
    for (int PreLoadScreenIndex = 0; PreLoadScreenIndex < PreLoadScreens.Num(); ++PreLoadScreenIndex)
    {
        if (PreLoadScreens[PreLoadScreenIndex]->GetPreLoadScreenType() == PreLoadScreenTypeToPlay)
        {
            PlayPreLoadScreenAtIndex(PreLoadScreenIndex);
            break;
        }
    }
}

void FPreLoadScreenManager::PlayPreLoadScreenAtIndex(int Index)
{
    if (ArePreLoadScreensEnabled())
    {
        ActivePreLoadScreenIndex = Index;
        if (ensureAlwaysMsgf(HasValidActivePreLoadScreen(), TEXT("Call to FPreLoadScreenManager::PlayPreLoadScreenAtIndex with an invalid index! Nothing will play!")))
        {
            IPreLoadScreen* ActiveScreen = GetActivePreLoadScreen();
            if (ActiveScreen->GetPreLoadScreenType() == EPreLoadScreenTypes::EarlyStartupScreen)
            {
                HandleEarlyStartupPlay();
            }
            else if (ActiveScreen->GetPreLoadScreenType() == EPreLoadScreenTypes::EngineLoadingScreen)
            {
                HandleEngineLoadingPlay();
            }
            else
            {
                UE_LOG(LogPreLoadScreenManager, Fatal, TEXT("Attempting to play an Active PreLoadScreen type that hasn't been implemented inside of PreLoadScreenmanager!"));
            }
        }
    }
}

bool FPreLoadScreenManager::PlayPreLoadScreenWithTag(FName InTag)
{
	for (int PreLoadScreenIndex = 0; PreLoadScreenIndex < PreLoadScreens.Num(); ++PreLoadScreenIndex)
	{
		if (PreLoadScreens[PreLoadScreenIndex]->GetPreLoadScreenTag() == InTag)
		{
			PlayPreLoadScreenAtIndex(PreLoadScreenIndex);
			return true;
		}
	}
	return false;
}

void FPreLoadScreenManager::HandleEarlyStartupPlay()
{
    if (ensureAlwaysMsgf(HasActivePreLoadScreenType(EPreLoadScreenTypes::EarlyStartupScreen), TEXT("Invalid Active PreLoadScreen!")))
    {
        IPreLoadScreen* PreLoadScreen = GetActivePreLoadScreen();
        if (PreLoadScreen && MainWindow.IsValid())
        {
            PreLoadScreen->OnPlay(MainWindow.Pin());

            if (PreLoadScreen->GetWidget().IsValid())
            {
                MainWindow.Pin()->SetContent(PreLoadScreen->GetWidget().ToSharedRef());
            }

			bool bDidDisableScreensaver = false;
			if (FPlatformApplicationMisc::IsScreensaverEnabled())
			{
				bDidDisableScreensaver = FPlatformApplicationMisc::ControlScreensaver(FGenericPlatformApplicationMisc::EScreenSaverAction::Disable);
			}

            //We run this PreLoadScreen until its finished or we lose the MainWindow as EarlyPreLoadPlay is synchronous
            while (!PreLoadScreen->IsDone())
            {
                EarlyPlayFrameTick();
            }

            if (bDidDisableScreensaver)
            {
                FPlatformApplicationMisc::ControlScreensaver(FGenericPlatformApplicationMisc::EScreenSaverAction::Enable);
            }

            StopPreLoadScreen();
        }
    }
}

void FPreLoadScreenManager::HandleEngineLoadingPlay()
{
    if (ensureAlwaysMsgf(HasActivePreLoadScreenType(EPreLoadScreenTypes::EngineLoadingScreen), TEXT("Invalid Active PreLoadScreen!")))
    {
        IPreLoadScreen* PreLoadScreen = GetActivePreLoadScreen();
        if (PreLoadScreen)
        {
            PreLoadScreen->OnPlay(MainWindow.Pin());

            if (PreLoadScreen->GetWidget().IsValid() && VirtualRenderWindow.IsValid())
            {
                VirtualRenderWindow->SetContent(PreLoadScreen->GetWidget().ToSharedRef());
            }
        }

        if (WidgetRenderer.IsValid())
        {
            FScopeLock SyncMechanismLock(&SyncMechanismCriticalSection);
            if (SyncMechanism == nullptr)
            {
                SyncMechanism = new FPreLoadScreenSlateSynchMechanism(WidgetRenderer);
                SyncMechanism->Initialize();
            }
        }
    }
}


void FPreLoadScreenManager::RenderTick()
{
    //Calculate tick time
    const double CurrentTime = FPlatformTime::Seconds();
    double DeltaTime = CurrentTime - LastRenderTickTime;
    LastRenderTickTime = CurrentTime;

    //Check if we have an active index before doing any work
    if (HasValidActivePreLoadScreen())
    {
        IPreLoadScreen* PreLoadScreen = GetActivePreLoadScreen();

        check(PreLoadScreen && IsInRenderingThread());
        if (MainWindow.IsValid() && VirtualRenderWindow.IsValid() && !PreLoadScreen->IsDone())
        {                
            GFrameNumberRenderThread++;
            GRHICommandList.GetImmediateCommandList().BeginFrame();
            PreLoadScreen->RenderTick(DeltaTime);
            GRHICommandList.GetImmediateCommandList().EndFrame();
            GRHICommandList.GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
        }
    }
}

bool FPreLoadScreenManager::HasRegisteredPreLoadScreenType(EPreLoadScreenTypes PreLoadScreenTypeToCheck) const
{
    bool HasMatchingRegisteredScreen = false;
    for (const TSharedPtr<IPreLoadScreen>& Screen : PreLoadScreens)
    {
        if (Screen.IsValid() && (Screen->GetPreLoadScreenType() == PreLoadScreenTypeToCheck))
        {
            HasMatchingRegisteredScreen = true;
        }
    }

    return HasMatchingRegisteredScreen;
}

bool FPreLoadScreenManager::HasActivePreLoadScreenType(EPreLoadScreenTypes PreLoadScreenTypeToCheck) const
{
    return (HasValidActivePreLoadScreen() && (GetActivePreLoadScreen()->GetPreLoadScreenType() == PreLoadScreenTypeToCheck));
}

bool FPreLoadScreenManager::HasValidActivePreLoadScreen() const
{
    IPreLoadScreen* PreLoadScreen = nullptr;
    return (PreLoadScreens.IsValidIndex(ActivePreLoadScreenIndex) && PreLoadScreens[ActivePreLoadScreenIndex].IsValid());
}


IPreLoadScreen* FPreLoadScreenManager::GetActivePreLoadScreen()
{
    return HasValidActivePreLoadScreen() ? PreLoadScreens[ActivePreLoadScreenIndex].Get() : nullptr;
}

const IPreLoadScreen* FPreLoadScreenManager::GetActivePreLoadScreen() const
{
    return HasValidActivePreLoadScreen() ? PreLoadScreens[ActivePreLoadScreenIndex].Get() : nullptr;
}

void FPreLoadScreenManager::EarlyPlayFrameTick()
{
    if (ensureAlwaysMsgf(HasActivePreLoadScreenType(EPreLoadScreenTypes::EarlyStartupScreen), TEXT("EarlyPlayFrameTick called without a valid EarlyPreLoadScreen!")))
    {
        GameLogicFrameTick();
        EarlyPlayRenderFrameTick();
    }
}

void FPreLoadScreenManager::GameLogicFrameTick()
{
    IPreLoadScreen* ActivePreLoadScreen = GetActivePreLoadScreen();
    if (ensureAlwaysMsgf(ActivePreLoadScreen, TEXT("Invalid Active PreLoadScreen during GameLogicFameTick!")))
    {
        //First spin the platform by having it sleep a bit
        const float SleepTime = ActivePreLoadScreen ? ActivePreLoadScreen->GetAddedTickDelay() : 0.f;
        if (SleepTime > 0)
        {
            FPlatformProcess::Sleep(SleepTime);
        }

        double CurrentTime = FPlatformTime::Seconds();
        double DeltaTime = CurrentTime - LastTickTime;
        LastTickTime = CurrentTime;

        //We have to manually tick everything as we are looping the main thread here
        FTicker::GetCoreTicker().Tick(DeltaTime);
        FThreadManager::Get().Tick();

        //Tick android events
#if PLATFORM_ANDROID
#if USE_ANDROID_EVENTS
    // Process any Android events or we may have issues returning from background
        FAppEventManager::GetInstance()->Tick();
#endif
#endif
        //Tick the Active Screen
        ActivePreLoadScreen->Tick(DeltaTime);

        // Pump messages to handle input , etc from system
        FPlatformApplicationMisc::PumpMessages(true);

        FSlateApplication::Get().PollGameDeviceState();
        // Gives widgets a chance to process any accumulated input
        FSlateApplication::Get().FinishedInputThisFrame();

        //Needed as this won't be incrementing on its own and some other tick functions rely on this (like analytics)
        GFrameCounter++;
    }
}

bool FPreLoadScreenManager::ShouldEarlyScreenRender()
{
    FScopeLock ScopeLock(&EarlyRenderingEnabledCriticalSection);
    return bEarlyRenderingEnabled;
}

void FPreLoadScreenManager::EnableEarlyRendering(bool bEnabled)
{
    FScopeLock ScopeLock(&EarlyRenderingEnabledCriticalSection);
    bEarlyRenderingEnabled = bEnabled;
}

void FPreLoadScreenManager::EarlyPlayRenderFrameTick()
{
    if (!ShouldEarlyScreenRender())
    {
        return;
    }

    IPreLoadScreen* ActivePreLoadScreen = PreLoadScreens[ActivePreLoadScreenIndex].Get();
    if (ensureAlwaysMsgf(ActivePreLoadScreen, TEXT("Invalid Active PreLoadScreen during EarlyPlayRenderFrameTick!")))
    {
        FSlateApplication& SlateApp = FSlateApplication::Get();
        float SlateDeltaTime = SlateApp.GetDeltaTime();

        //Setup Slate Render Command
        ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
            BeginPreLoadScreenFrame,
            IPreLoadScreen*, ActivePreLoadScreen, ActivePreLoadScreen,
            float, SlateDeltaTime, SlateDeltaTime,
            {
                if (FPreLoadScreenManager::ShouldEarlyScreenRender())
                {
                    GFrameNumberRenderThread++;
                    GRHICommandList.GetImmediateCommandList().BeginFrame();

                    ActivePreLoadScreen->RenderTick(SlateDeltaTime);
                }
            }
        );

        SlateApp.Tick();

        // Synchronize the game thread and the render thread so that the render thread doesn't get too far behind.
        SlateApp.GetRenderer()->Sync();

        ENQUEUE_RENDER_COMMAND(FinishPreLoadScreenFrame)(
            [](FRHICommandListImmediate& RHICmdList)
        {
            if (FPreLoadScreenManager::ShouldEarlyScreenRender())
            {
                GRHICommandList.GetImmediateCommandList().EndFrame();
                GRHICommandList.GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::FlushRHIThreadFlushResources);
            }
        });

        FlushRenderingCommands();
    }
}

void FPreLoadScreenManager::StopPreLoadScreen()
{
    if (HasValidActivePreLoadScreen())
    {
        PreLoadScreens[ActivePreLoadScreenIndex]->OnStop();
    }

    ActivePreLoadScreenIndex = -1;

    //Clear our window content
    if (MainWindow.IsValid())
    {
        MainWindow.Pin()->SetContent(SNullWidget::NullWidget);
    }
    if (VirtualRenderWindow.IsValid())
    {
        VirtualRenderWindow->SetContent(SNullWidget::NullWidget);
    }

    FlushRenderingCommands();
}

void FPreLoadScreenManager::PassPreLoadScreenWindowBackToGame() const
{
    if (IsUsingMainWindow())
    {
        UGameEngine* GameEngine = Cast<UGameEngine>(GEngine);
        if (MainWindow.IsValid() && GameEngine)
        {
            GameEngine->GameViewportWindow = MainWindow;
        }
        else
        {
            UE_LOG(LogPreLoadScreenManager, Warning, TEXT("FPreLoadScreenManager::PassLoadingScreenWindowBackToGame failed.  No Window"));
        }
    }
}

bool FPreLoadScreenManager::IsUsingMainWindow() const
{
    return MainWindow.IsValid();
}

TSharedPtr<SWindow> FPreLoadScreenManager::GetRenderWindow()
{
    return MainWindow.IsValid() ? MainWindow.Pin() : nullptr;
}

void FPreLoadScreenManager::WaitForEngineLoadingScreenToFinish()
{
    //Start just doing game logic ticks until the Screen is finished.
    //Since this is a non-early screen, rendering happens separately still on the Slate rendering thread, so only need
    //the game logic ticks
    if (HasActivePreLoadScreenType(EPreLoadScreenTypes::EngineLoadingScreen))
    {
        IPreLoadScreen* ActivePreLoadScreen = GetActivePreLoadScreen();
        while (ActivePreLoadScreen && !ActivePreLoadScreen->IsDone())
        {
            GameLogicFrameTick();
        }
    }

    //No longer need SyncMechanism now that the widget has finished rendering
    FScopeLock SyncMechanismLock(&SyncMechanismCriticalSection);
    if (SyncMechanism != nullptr)
    {
        SyncMechanism->DestroySlateThread();

        delete SyncMechanism;
        SyncMechanism = nullptr;
    }

    StopPreLoadScreen();
}

void FPreLoadScreenManager::SetEngineLoadingComplete(bool IsEngineLoadingFinished)
{
    bIsEngineLoadingComplete = IsEngineLoadingFinished;

    IPreLoadScreen* PreLoadScreen = GetActivePreLoadScreen();
    if (PreLoadScreen)
    {
        PreLoadScreen->SetEngineLoadingFinished(IsEngineLoadingFinished);
    }
}

bool FPreLoadScreenManager::ArePreLoadScreensEnabled()
{
	bool bEnabled = !GIsEditor && !IsRunningDedicatedServer() && !IsRunningCommandlet() && GUseThreadedRendering;

#if !UE_BUILD_SHIPPING
	bEnabled &= !FParse::Param(FCommandLine::Get(), TEXT("NoLoadingScreen"));
#endif

#if PLATFORM_UNIX
	bEnabled = false;
#endif

	return bEnabled;
}

void FPreLoadScreenManager::CleanUpResources()
{
    for (TSharedPtr<IPreLoadScreen>& PreLoadScreen : PreLoadScreens)
    {
        if (PreLoadScreen.IsValid())
        {
            PreLoadScreen->CleanUp();
        }

        PreLoadScreen.Reset();
    }

    OnPreLoadScreenManagerCleanUp.Broadcast();

    //Make sure our FPreLoadSettingsContainer is cleaned up. We do this here instead of one of the
    //StartupScreens because we don't know how many of them will be using the same PreLoadScreenContainer, however any
    //other game specific settings containers should be cleaned up by their screens/modules
    BeginCleanup(&FPreLoadSettingsContainerBase::Get());
}
