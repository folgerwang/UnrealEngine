// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/ThreadSafeCounter.h"

#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"

#include "Widgets/SVirtualWindow.h"

#include "RHI.h"
#include "RHIResources.h"


/**
* The Slate thread is simply run on a worker thread.
* Slate is run on another thread because the game thread (where Slate is usually run)
* is blocked loading things. Slate is very modular, which makes it very easy to run on another
* thread with no adverse effects.
* It does not enqueue render commands, because the RHI is not thread safe. Thus, it waits to
* enqueue render commands until the render thread tickables ticks, and then it calls them there.
*/
class  FPreLoadScreenSlateThreadTask : public FRunnable
{
public:
    FPreLoadScreenSlateThreadTask(class FPreLoadScreenSlateSynchMechanism& InSyncMechanism)
        : SyncMechanism(&InSyncMechanism)
    {
    }

    /** FRunnable interface */
    virtual bool Init() override;
    virtual uint32 Run() override;
    virtual void Stop() override;
private:
    /** Hold a handle to our parent sync mechanism which handles all of our threading locks */
    class FPreLoadScreenSlateSynchMechanism* SyncMechanism;
};

class PRELOADSCREEN_API FPreLoadSlateWidgetRenderer
{
public:
    FPreLoadSlateWidgetRenderer(TSharedPtr<SWindow> InMainWindow, TSharedPtr<SVirtualWindow> InVirtualRenderWindowWindow, FSlateRenderer* InRenderer);

    void DrawWindow(float DeltaTime);

private:
    /** The actual window content will be drawn to */
    /** Note: This is raw as we SWindows registered with SlateApplication are not thread safe */
    SWindow* MainWindow;

    /** Virtual window that we render to instead of the main slate window (for thread safety).  Shares only the same backbuffer as the main window */
    TSharedRef<class SVirtualWindow> VirtualRenderWindow;

    TSharedPtr<FHittestGrid> HittestGrid;

    FSlateRenderer* SlateRenderer;

    FViewportRHIRef ViewportRHI;
};


/**
* This class will handle all the nasty bits about running Slate on a separate thread
* and then trying to sync it up with the game thread and the render thread simultaneously
*/
class PRELOADSCREEN_API FPreLoadScreenSlateSynchMechanism
{
public:
    FPreLoadScreenSlateSynchMechanism(TSharedPtr<FPreLoadSlateWidgetRenderer, ESPMode::ThreadSafe> InWidgetRenderer);
    ~FPreLoadScreenSlateSynchMechanism();

    /** Sets up the locks in their proper initial state for running */
    void Initialize();

    /** Cleans up the slate thread */
    void DestroySlateThread();

    /** Handles the strict alternation of the slate drawing passes */
    bool IsSlateDrawPassEnqueued();
    void SetSlateDrawPassEnqueued();
    void ResetSlateDrawPassEnqueued();

    /** Handles the counter to determine if the slate thread should keep running */
    bool IsSlateMainLoopRunning();
    void SetSlateMainLoopRunning();
    void ResetSlateMainLoopRunning();

    /** The main loop to be run from the Slate thread */
    void SlateThreadRunMainLoop();

private:
    volatile int8 MainLoopCounter;

    /**
    * This counter handles running the main loop of the slate thread
    */
    FThreadSafeCounter IsRunningSlateMainLoop;
    /**
    * This counter handles strict alternation between the slate thread and the render thread
    * for passing Slate render draw passes between each other.
    */
    FThreadSafeCounter IsSlateDrawEnqueued;

    /**
    * This counter is used to generate a unique id for each new instance of the loading thread
    */
    static FThreadSafeCounter LoadingThreadInstanceCounter;

    /** The worker thread that will become the Slate thread */
    FRunnableThread* SlateLoadingThread;
    FRunnable* SlateRunnableTask;

    TSharedPtr<FPreLoadSlateWidgetRenderer, ESPMode::ThreadSafe> WidgetRenderer;
};
