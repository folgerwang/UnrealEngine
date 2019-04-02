// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "PreLoadSlateThreading.h"
#include "HAL/Runnable.h"
#include "HAL/RunnableThread.h"
#include "Misc/ScopeLock.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformAtomics.h"
#include "Rendering/SlateDrawBuffer.h"
#include "RenderingThread.h"

#include "PreLoadScreenManager.h"

FThreadSafeCounter FPreLoadScreenSlateSynchMechanism::LoadingThreadInstanceCounter;

bool FPreLoadScreenSlateThreadTask::Init()
{
    // First thing to do is set the slate loading thread ID
    // This guarantees all systems know that a slate thread exists
    GSlateLoadingThreadId = FPlatformTLS::GetCurrentThreadId();

    return true;
}

uint32 FPreLoadScreenSlateThreadTask::Run()
{
    check(GSlateLoadingThreadId == FPlatformTLS::GetCurrentThreadId());

    SyncMechanism->SlateThreadRunMainLoop();

    // Tear down the slate loading thread ID
    FPlatformAtomics::InterlockedExchange((int32*)&GSlateLoadingThreadId, 0);

    return 0;
}

void FPreLoadScreenSlateThreadTask::Stop()
{
    SyncMechanism->ResetSlateDrawPassEnqueued();
    SyncMechanism->ResetSlateMainLoopRunning();
}

FPreLoadSlateWidgetRenderer::FPreLoadSlateWidgetRenderer(TSharedPtr<SWindow> InMainWindow, TSharedPtr<SVirtualWindow> InVirtualRenderWindow, FSlateRenderer* InRenderer)
    : MainWindow(InMainWindow.Get())
    , VirtualRenderWindow(InVirtualRenderWindow.ToSharedRef())
    , SlateRenderer(InRenderer)
{
    HittestGrid = MakeShareable(new FHittestGrid);
}

void FPreLoadSlateWidgetRenderer::DrawWindow(float DeltaTime)
{
    if (GDynamicRHI && GDynamicRHI->RHIIsRenderingSuspended())
    {
        // This avoids crashes if we Suspend rendering whilst the loading screen is up
        // as we don't want Slate to submit any more draw calls until we Resume.
        return;
    }

    FVector2D DrawSize = VirtualRenderWindow->GetClientSizeInScreen();

    FSlateApplication::Get().Tick(ESlateTickType::TimeOnly);

    const float Scale = 1.0f;
    FGeometry WindowGeometry = FGeometry::MakeRoot(DrawSize, FSlateLayoutTransform(Scale));

    VirtualRenderWindow->SlatePrepass(WindowGeometry.Scale);

    FSlateRect ClipRect = WindowGeometry.GetLayoutBoundingRect();

    HittestGrid->ClearGridForNewFrame(ClipRect);

    // Get the free buffer & add our virtual window
    FSlateDrawBuffer& DrawBuffer = SlateRenderer->GetDrawBuffer();
    FSlateWindowElementList& WindowElementList = DrawBuffer.AddWindowElementList(VirtualRenderWindow);

    WindowElementList.SetRenderTargetWindow(MainWindow);

    int32 MaxLayerId = 0;
    {
        FPaintArgs PaintArgs(*VirtualRenderWindow, *HittestGrid, FVector2D::ZeroVector, FSlateApplication::Get().GetCurrentTime(), FSlateApplication::Get().GetDeltaTime());

        // Paint the window
        MaxLayerId = VirtualRenderWindow->Paint(
            PaintArgs,
            WindowGeometry, ClipRect,
            WindowElementList,
            0,
            FWidgetStyle(),
            VirtualRenderWindow->IsEnabled());
    }

    SlateRenderer->DrawWindows(DrawBuffer);

    DrawBuffer.ViewOffset = FVector2D::ZeroVector;
}


FPreLoadScreenSlateSynchMechanism::FPreLoadScreenSlateSynchMechanism(TSharedPtr<FPreLoadSlateWidgetRenderer, ESPMode::ThreadSafe> InWidgetRenderer)
    : MainLoopCounter(0)
    , WidgetRenderer(InWidgetRenderer)
{
}

FPreLoadScreenSlateSynchMechanism::~FPreLoadScreenSlateSynchMechanism()
{
    DestroySlateThread();
}

void FPreLoadScreenSlateSynchMechanism::Initialize()
{
    check(IsInGameThread());

    ResetSlateDrawPassEnqueued();
    SetSlateMainLoopRunning();

    //Try to only spin up 1 Slate Loading Thread
    int8 LoopRunningCount = FPlatformAtomics::InterlockedIncrement(&MainLoopCounter);
    if (LoopRunningCount == 1)
    {
        FString ThreadName = TEXT("SlateLoadingThread");
        ThreadName.AppendInt(LoadingThreadInstanceCounter.Increment());

        SlateRunnableTask = new FPreLoadScreenSlateThreadTask(*this);
        SlateLoadingThread = FRunnableThread::Create(SlateRunnableTask, *ThreadName);
    }
}

void FPreLoadScreenSlateSynchMechanism::DestroySlateThread()
{
    check(IsInGameThread());

    if (SlateLoadingThread)
    {
        IsRunningSlateMainLoop.Reset();

        while (FPlatformAtomics::AtomicRead(&MainLoopCounter) > 0)
        {
            FPlatformApplicationMisc::PumpMessages(false);
            FPlatformProcess::Sleep(0.1f);
        }

        delete SlateLoadingThread;
        delete SlateRunnableTask;
        SlateLoadingThread = nullptr;
        SlateRunnableTask = nullptr;
    }
}

bool FPreLoadScreenSlateSynchMechanism::IsSlateDrawPassEnqueued()
{
    return IsSlateDrawEnqueued.GetValue() != 0;
}

void FPreLoadScreenSlateSynchMechanism::SetSlateDrawPassEnqueued()
{
    IsSlateDrawEnqueued.Set(1);
}

void FPreLoadScreenSlateSynchMechanism::ResetSlateDrawPassEnqueued()
{
    IsSlateDrawEnqueued.Reset();
}

bool FPreLoadScreenSlateSynchMechanism::IsSlateMainLoopRunning()
{
    return IsRunningSlateMainLoop.GetValue() != 0;
}

void FPreLoadScreenSlateSynchMechanism::SetSlateMainLoopRunning()
{
    IsRunningSlateMainLoop.Set(1);
}

void FPreLoadScreenSlateSynchMechanism::ResetSlateMainLoopRunning()
{
    IsRunningSlateMainLoop.Reset();
}

void FPreLoadScreenSlateSynchMechanism::SlateThreadRunMainLoop()
{
    double LastTime = FPlatformTime::Seconds();

    while (IsSlateMainLoopRunning())
    {
        double CurrentTime = FPlatformTime::Seconds();
        double DeltaTime = CurrentTime - LastTime;

        // 60 fps max
        const double MaxTickRate = 1.0 / 60.0f;

        const double TimeToWait = MaxTickRate - DeltaTime;

        if (TimeToWait > 0)
        {
            FPlatformProcess::Sleep(TimeToWait);
            CurrentTime = FPlatformTime::Seconds();
            DeltaTime = CurrentTime - LastTime;
        }

        if (FSlateApplication::IsInitialized() && !IsSlateDrawPassEnqueued() && FPreLoadScreenManager::ShouldRender())
        {
            FSlateRenderer* MainSlateRenderer = FSlateApplication::Get().GetRenderer();
            FScopeLock ScopeLock(MainSlateRenderer->GetResourceCriticalSection());

            //Don't queue up a draw pass if our main loop is shutting down
            if (IsSlateMainLoopRunning())
            {
                WidgetRenderer->DrawWindow(DeltaTime);
                SetSlateDrawPassEnqueued();
            }

            //Queue up a render tick every time we tick on this sync thread.
			FPreLoadScreenSlateSynchMechanism* SyncMech = this;
			ENQUEUE_RENDER_COMMAND(PreLoadScreenRenderTick)(
				[SyncMech](FRHICommandListImmediate& RHICmdList)
                {
                    FPreLoadScreenManager* PreLoadManager = FPreLoadScreenManager::Get();
                    if (PreLoadManager && FPreLoadScreenManager::ShouldRender())
                    {
                        FPreLoadScreenManager::Get()->RenderTick();
                    }
                        
                    SyncMech->ResetSlateDrawPassEnqueued();
                }
            );
        }

        LastTime = CurrentTime;
    }

    while (IsSlateDrawPassEnqueued())
    {
        FPlatformProcess::Sleep(0.1f);
    }

    FPlatformAtomics::InterlockedDecrement(&MainLoopCounter);
}