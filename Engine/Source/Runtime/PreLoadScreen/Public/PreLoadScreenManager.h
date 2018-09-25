// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PreLoadScreen.h"

#include "CoreMinimal.h"

#include "Widgets/SVirtualWindow.h"
#include "Widgets/SWindow.h"

#include "TickableObjectRenderThread.h"

#include "Containers/Ticker.h"

#include "PreLoadSlateThreading.h"

// Class that handles storing all registered PreLoadScreens and Playing/Stopping them
class PRELOADSCREEN_API FPreLoadScreenManager
{
public:
    //Gets the single instance of this settings object. Also creates it if needed
    static FPreLoadScreenManager* Get()
    {
        return Instance.Get();
    }

    static void Create()
    {
        if (!Instance.IsValid() && ArePreLoadScreensEnabled())
        {
            Instance = MakeShareable(new FPreLoadScreenManager());
        }
    }

    static void Destroy()
    {
        if (Instance.IsValid())
        {
            Instance->CleanUpResources();
        }

        Instance.Reset();
    }

    virtual ~FPreLoadScreenManager() {}

    void Initialize(FSlateRenderer& InSlateRenderer);

    void RegisterPreLoadScreen(TSharedPtr<IPreLoadScreen> PreLoadScreen);
    void UnRegisterPreLoadScreen(TSharedPtr<IPreLoadScreen> PreLoadScreen);

    //Plays the first found PreLoadScreen that matches the bEarlyPreLoadScreen setting passed in.
    void PlayFirstPreLoadScreen(EPreLoadScreenTypes PreLoadScreenTypeToPlay);
    void PlayPreLoadScreenAtIndex(int Index);

	/** Plays the PreLoadScreen with a tag that matches InTag
	 *  @returns false if no PreLoadScreen with that tag has been registered */
	bool PlayPreLoadScreenWithTag(FName InTag);
    
    void StopPreLoadScreen();

    void PassPreLoadScreenWindowBackToGame() const;

    bool IsUsingMainWindow() const;

    TSharedPtr<SWindow> GetRenderWindow();

    bool HasRegisteredPreLoadScreenType(EPreLoadScreenTypes PreLoadScreenTypeToCheck) const;
    bool HasActivePreLoadScreenType(EPreLoadScreenTypes PreLoadScreenTypeToCheck) const;
    bool HasValidActivePreLoadScreen() const;

    void WaitForEngineLoadingScreenToFinish();

    void SetEngineLoadingComplete(bool IsEngineLoadingFinished = true);
    bool IsEngineLoadingComplete() const { return bIsEngineLoadingComplete; }

    void CleanUpResources();

    static void EnableEarlyRendering(bool bEnabled);
    static bool ShouldEarlyScreenRender();
    static bool ArePreLoadScreensEnabled();

    //Creates a tick on the Render Thread that we run every
    virtual void RenderTick();
    
    // Callback for handling cleaning up any resources you would like to remove after the PreLoadScreenManager cleans up
    // Not needed for PreLoadScreens as those have a seperate CleanUp method called.
    DECLARE_MULTICAST_DELEGATE(FOnPreLoadScreenManagerCleanUp);
    FOnPreLoadScreenManagerCleanUp OnPreLoadScreenManagerCleanUp;

protected:
    //Default constructor. We don't want other classes to make these. Should just rely on Get()
    FPreLoadScreenManager()
        : ActivePreLoadScreenIndex(-1)
        , bInitialized(false)
        , SyncMechanism(nullptr)
        , bIsEngineLoadingComplete(false)
    {}

    TArray<TSharedPtr<IPreLoadScreen>> PreLoadScreens;

    //Singleton Instance
    static TSharedPtr<FPreLoadScreenManager> Instance;

    void BeginPlay();

    /*** These functions describe the flow for an EarlyPreLoadScreen where everything is blocking waiting on a call to StopPreLoadScreen ***/
    void HandleEarlyStartupPlay();
    //Separate tick that handles 
    void EarlyPlayFrameTick();
    void GameLogicFrameTick();
    void EarlyPlayRenderFrameTick();

    /*** These functions describe how everything is handled during an non-Early PreLoadPlay. Everything is handled asynchronously in this case with a standalone renderer ***/
    void HandleEngineLoadingPlay();

    IPreLoadScreen* GetActivePreLoadScreen();
    const IPreLoadScreen* GetActivePreLoadScreen() const;

    int ActivePreLoadScreenIndex;
    double LastTickTime;

    /** Widget renderer used to tick and paint windows in a thread safe way */
    TSharedPtr<FPreLoadSlateWidgetRenderer, ESPMode::ThreadSafe> WidgetRenderer;

    /** The window that the loading screen resides in */
    TWeakPtr<class SWindow> MainWindow;

    /** Virtual window that we render to instead of the main slate window (for thread safety).  Shares only the same backbuffer as the main window */
    TSharedPtr<class SVirtualWindow> VirtualRenderWindow;

    bool bInitialized;

    /** The threading mechanism with which we handle running slate on another thread */
    class FPreLoadScreenSlateSynchMechanism* SyncMechanism;
    /** Critical section to allow the slate loading thread and the render thread to safely utilize the synchronization mechanism for ticking Slate. */
    FCriticalSection SyncMechanismCriticalSection;

    static FCriticalSection EarlyRenderingEnabledCriticalSection;
    static bool bEarlyRenderingEnabled;

    double LastRenderTickTime;

    float OriginalSlateSleepVariableValue;
    bool bIsEngineLoadingComplete;
};
