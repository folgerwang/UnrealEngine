// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "IOS/IOSPlatformFramePacer.h"
#include "Containers/Array.h"
#include "HAL/ThreadingBase.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"

// Collection of events listening for this trigger.
static TArray<FEvent*> ListeningEvents;
static FCriticalSection HandlersMutex;
static NSMutableSet<FIOSFramePacerHandler>* Handlers = [NSMutableSet new];

namespace IOSDisplayConstants
{
    const uint32 MaxRefreshRate = 60;
}

/*******************************************************************
 * FIOSFramePacer implementation
 *******************************************************************/

@interface FIOSFramePacer : NSObject
{
    @public
	FEvent *FramePacerEvent;
}

-(void)run:(id)param;
-(void)signal:(id)param;

@end

// @todo ios: Move these up into some shared header
// __TV_OS_VERSION_MAX_ALLOWED is only defined when building for tvos, so we can use that to determine
#if PLATFORM_TVOS

#define UE4_HAS_IOS10 (__TVOS_10_0 && __TV_OS_VERSION_MAX_ALLOWED >= __TVOS_10_0)
#define UE4_HAS_IOS9 (__TVOS_9_0 && __TV_OS_VERSION_MAX_ALLOWED >= __TVOS_9_0)
#define UE4_TARGET_PRE_IOS10 (!__TVOS_10_0 || __TV_OS_VERSION_MIN_REQUIRED < __TVOS_10_0)
#define UE4_TARGET_PRE_IOS9 (!__TVOS_9_0 || __TV_OS_VERSION_MIN_REQUIRED < __TVOS_9_0)

#else

#define UE4_HAS_IOS10 (__IPHONE_10_0 && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_10_0)
#define UE4_HAS_IOS9 (__IPHONE_9_0 && __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_9_0)
#define UE4_TARGET_PRE_IOS10 (!__IPHONE_10_0 || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_10_0)
#define UE4_TARGET_PRE_IOS9 (!__IPHONE_9_0 || __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0)

#endif

@implementation FIOSFramePacer

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
-(void)run:(id)param
{
	NSRunLoop *runloop = [NSRunLoop currentRunLoop];
	CADisplayLink *displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(signal:)];
#if UE4_HAS_IOS10
	if ([displayLink respondsToSelector : @selector(preferredFramesPerSecond)] == YES)
	{
		displayLink.preferredFramesPerSecond = IOSDisplayConstants::MaxRefreshRate / FIOSPlatformRHIFramePacer::FrameInterval;
	}
	else
#endif
	{
#if UE4_TARGET_PRE_IOS10
		displayLink.frameInterval = FIOSPlatformRHIFramePacer::FrameInterval;
#endif
	}

	[displayLink addToRunLoop:runloop forMode:NSDefaultRunLoopMode];
	[runloop run];
}
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif


-(void)signal:(id)param
{
	// during shutdown, this can cause crashes (only non-backgrounding apps do this)
	if (GIsRequestingExit)
	{
		return;
	};

	{
		FScopeLock Lock(&HandlersMutex);
		CADisplayLink* displayLink = (CADisplayLink*)param;
		double OutputSeconds = displayLink.duration + displayLink.timestamp;
		if (@available(iOS 10.0, tvOS 10.0, *))
		{
			OutputSeconds = displayLink.targetTimestamp;
		}
		double OutputDuration = displayLink.duration;
		for (FIOSFramePacerHandler Handler in Handlers)
		{
			Handler(0, OutputSeconds, OutputDuration);
		}

		// If requested, get the latest frame sync interval and clamp to the project setting
		static TConsoleVariableData<int32>* VSyncCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.vsync"));
		static TConsoleVariableData<int32>* VSyncIntervalCVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("rhi.syncinterval"));

		if (VSyncCVar && VSyncCVar->GetValueOnRenderThread() > 0)
		{
			uint32 NewFrameInterval = VSyncIntervalCVar ? VSyncIntervalCVar->GetValueOnRenderThread() : FIOSPlatformRHIFramePacer::FrameInterval;
			NewFrameInterval = (NewFrameInterval < FIOSPlatformRHIFramePacer::MinFrameInterval) ? FIOSPlatformRHIFramePacer::MinFrameInterval : NewFrameInterval;

			// If changed, update the display link
			if (NewFrameInterval != FIOSPlatformRHIFramePacer::FrameInterval)
			{
				FIOSPlatformRHIFramePacer::FrameInterval = NewFrameInterval;
	
#if (UE4_HAS_IOS10 || UE4_TARGET_PRE_IOS10)	
				CADisplayLink* displayLinkParam = (CADisplayLink*)param;
#endif
		
#if UE4_HAS_IOS10
				if (displayLinkParam.preferredFramesPerSecond > 0)
				{
					displayLinkParam.preferredFramesPerSecond = IOSDisplayConstants::MaxRefreshRate / FIOSPlatformRHIFramePacer::FrameInterval;
				}
				else
#endif
				{
#if UE4_TARGET_PRE_IOS10
					displayLinkParam.frameInterval = FIOSPlatformRHIFramePacer::FrameInterval;
#endif
				}

				// Update pacing for present
				FIOSPlatformRHIFramePacer::Pace = IOSDisplayConstants::MaxRefreshRate / FIOSPlatformRHIFramePacer::FrameInterval;
			}
		}
	}	
    for( auto& NextEvent : ListeningEvents )
    {
        NextEvent->Trigger();
    }
}

@end



/*******************************************************************
 * FIOSPlatformRHIFramePacer implementation
 *******************************************************************/

uint32 FIOSPlatformRHIFramePacer::FrameInterval = 1;
uint32 FIOSPlatformRHIFramePacer::MinFrameInterval = 1;
FIOSFramePacer* FIOSPlatformRHIFramePacer::FramePacer = nil;
uint32 FIOSPlatformRHIFramePacer::Pace = 0;


bool FIOSPlatformRHIFramePacer::IsEnabled()
{
    static bool bIsRHIFramePacerEnabled = false;
	static bool bInitialized = false;

	if (!bInitialized)
	{
		FString FrameRateLockAsEnum;
		GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("FrameRateLock"), FrameRateLockAsEnum, GEngineIni);

		uint32 FrameRateLock = IOSDisplayConstants::MaxRefreshRate;
		FParse::Value(*FrameRateLockAsEnum, TEXT("PUFRL_"), FrameRateLock);

        const bool bOverridesFrameRate = FParse::Value( FCommandLine::Get(), TEXT( "FrameRateLock=" ), FrameRateLockAsEnum );
        if (bOverridesFrameRate)
        {
            FParse::Value(*FrameRateLockAsEnum, TEXT("PUFRL_"), FrameRateLock);
        }
        
        if (FrameRateLock == 0)
        {
            FrameRateLock = IOSDisplayConstants::MaxRefreshRate;
        }

        if (!bIsRHIFramePacerEnabled)
		{
			check((IOSDisplayConstants::MaxRefreshRate % FrameRateLock) == 0);
			FrameInterval = IOSDisplayConstants::MaxRefreshRate / FrameRateLock;
			MinFrameInterval = FrameInterval;

			bIsRHIFramePacerEnabled = (FrameInterval > 0);
			
			// remember the Pace if we are enabled
			Pace = bIsRHIFramePacerEnabled ? FrameRateLock : 0;
		}
		bInitialized = true;
	}
	
	return bIsRHIFramePacerEnabled;
}

void FIOSPlatformRHIFramePacer::InitWithEvent(FEvent* TriggeredEvent)
{
    // Create display link thread
    FramePacer = [[FIOSFramePacer alloc] init];
    [NSThread detachNewThreadSelector:@selector(run:) toTarget:FramePacer withObject:nil];
        
    // Only one supported for now, we may want more eventually.
    ListeningEvents.Add( TriggeredEvent );
}

void FIOSPlatformRHIFramePacer::AddHandler(FIOSFramePacerHandler Handler)
{
	check (FramePacer);
	FScopeLock Lock(&HandlersMutex);
	FIOSFramePacerHandler Copy = Block_copy(Handler);
	[Handlers addObject:Copy];
	Block_release(Copy);
}

void FIOSPlatformRHIFramePacer::RemoveHandler(FIOSFramePacerHandler Handler)
{
	check (FramePacer);
	FScopeLock Lock(&HandlersMutex);
	[Handlers removeObject:Handler];
}

void FIOSPlatformRHIFramePacer::Suspend()
{
    // send a signal to the events if we are enabled
    if (IsEnabled())
    {
        [FramePacer signal:0];
    }
}

void FIOSPlatformRHIFramePacer::Resume()
{
    
}

void FIOSPlatformRHIFramePacer::Destroy()
{
    if( FramePacer != nil )
    {
        [FramePacer release];
        FramePacer = nil;
    }
}
