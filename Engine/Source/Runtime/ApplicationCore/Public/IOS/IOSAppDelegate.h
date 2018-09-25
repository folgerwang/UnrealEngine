// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#import <UIKit/UIKit.h>
#import <AVFoundation/AVAudioSession.h>
#import <GameKit/GKGameCenterViewController.h>
#include "Delegates/Delegate.h"
#include "Logging/LogMacros.h"
#include "Containers/UnrealString.h"

#define USE_MUTE_SWITCH_DETECTION 0

// Predicate to decide whether a push notification message should be processed
DECLARE_DELEGATE_RetVal_OneParam(bool, FPushNotificationFilter, NSDictionary*);

class APPLICATIONCORE_API FIOSCoreDelegates
{
public:
	// Broadcast when this application is opened from an external source.
	DECLARE_MULTICAST_DELEGATE_FourParams(FOnOpenURL, UIApplication*, NSURL*, NSString*, id);
	static FOnOpenURL OnOpenURL;

	/** Add a filter to decide whether each push notification should be processed */
	static FDelegateHandle AddPushNotificationFilter(const FPushNotificationFilter& FilterDel);

	/** Remove a previously processed push notification filter */
	static void RemovePushNotificationFilter(FDelegateHandle Handle);

	/** INTERNAL - check if a push notification payload passes all registered filters */
	static bool PassesPushNotificationFilters(NSDictionary* Payload);

private:
	struct FFilterDelegateAndHandle
	{
		FPushNotificationFilter Filter;
		FDelegateHandle Handle;
	};

	static TArray<FFilterDelegateAndHandle> PushNotificationFilters;
};

@class FIOSView;
@class IOSViewController;
@class SlateOpenGLESViewController;
@class IOSAppDelegate;

DECLARE_LOG_CATEGORY_EXTERN(LogIOSAudioSession, Log, All);

namespace FAppEntry
{
	void PlatformInit();
	void PreInit(IOSAppDelegate* AppDelegate, UIApplication* Application);
	void Init();
	void Tick();
    void SuspendTick();
	void Shutdown();
    void Suspend(bool bIsInterrupt = false);
    void Resume(bool bIsInterrupt = false);
	bool IsStartupMoviePlaying();

	extern bool	gAppLaunchedWithLocalNotification;
	extern FString	gLaunchLocalNotificationActivationEvent;
	extern int32	gLaunchLocalNotificationFireDate;
}

@interface IOSAppDelegate : UIResponder <UIApplicationDelegate,
#if !UE_BUILD_SHIPPING
	UIGestureRecognizerDelegate,
#endif
	GKGameCenterControllerDelegate,
UITextFieldDelegate>

/** Window object */
@property (strong, retain, nonatomic) UIWindow *Window;

/** Main GL View */
@property (retain) FIOSView* IOSView;

@property class FIOSApplication* IOSApplication;

/** The controller to handle rotation of the view */
@property (retain) IOSViewController* IOSController;

/** The view controlled by the auto-rotating controller */
@property (retain) UIView* RootView;

/** The controller to handle rotation of the view */
@property (retain) SlateOpenGLESViewController* SlateController;

/** The value of the alert response (atomically set since main thread and game thread use it */
@property (assign) int AlertResponse;

/** Version of the OS we are running on (NOT compiled with) */
@property (readonly) float OSVersion;

@property bool bDeviceInPortraitMode;

@property (retain) NSTimer* timer;

@property (retain) NSTimer* PeakMemoryTimer;

/** Timer used for re-enabling the idle timer */
@property (retain) NSTimer* IdleTimerEnableTimer;

/** The time delay (in seconds) between idle timer enable requests and actually enabling the idle timer */
@property (readonly) float IdleTimerEnablePeriod;

// parameters passed from openURL
@property (nonatomic, retain) NSMutableArray* savedOpenUrlParameters;

#if !UE_BUILD_SHIPPING && !PLATFORM_TVOS
	/** Properties for managing the console */
#if __IPHONE_OS_VERSION_MIN_REQUIRED < __IPHONE_9_0
	@property (nonatomic, retain) UIAlertView*		ConsoleAlert;
#endif
#ifdef __IPHONE_8_0
	@property (nonatomic, assign) UIAlertController* ConsoleAlertController;
#endif
	@property (nonatomic, retain) NSMutableArray*	ConsoleHistoryValues;
	@property (nonatomic, assign) int				ConsoleHistoryValuesIndex;
#endif

/** True if the engine has been initialized */
@property (readonly) bool bEngineInit;

/** Delays game initialization slightly in case we have a URL launch to handle */
@property (retain) NSTimer* CommandLineParseTimer;
@property (atomic) bool bCommandLineReady;

/** initial launch options */
@property(retain) NSDictionary* launchOptions;

@property (assign) NSProcessInfoThermalState ThermalState;
@property (assign) bool bBatteryState;
@property (assign) int BatteryLevel;

/**
 * @return the single app delegate object
 */
+ (IOSAppDelegate*)GetDelegate;

-(bool)IsIdleTimerEnabled;
-(void)EnableIdleTimer:(bool)bEnable;

-(void) ParseCommandLineOverrides;

-(int)GetAudioVolume;
-(bool)AreHeadphonesPluggedIn;
-(int)GetBatteryLevel;
-(bool)IsRunningOnBattery;
-(NSProcessInfoThermalState)GetThermalState;

/** TRUE if the device is playing background music and we want to allow that */
@property (assign) bool bUsingBackgroundMusic;
@property (assign) bool bLastOtherAudioPlaying;
@property (assign) bool bForceEmitOtherAudioPlaying;

#if USE_MUTE_SWITCH_DETECTION
@property (assign) bool bLastMutedState;
@property (assign) bool bForceEmitMutedState;
#endif

@property (assign) float LastVolume;
@property (assign) bool bForceEmitVolume;

- (void)InitializeAudioSession;
- (void)ToggleAudioSession:(bool)bActive force:(bool)bForce;
- (bool)IsBackgroundAudioPlaying;
- (void)EnableVoiceChat:(bool)bEnable;
- (bool)IsVoiceChatEnabled;

@property (atomic) bool bAudioActive;
@property (atomic) bool bVoiceChatEnabled;

@property (atomic) bool bIsSuspended;
@property (atomic) bool bHasSuspended;
@property (atomic) bool bHasStarted;
- (void)ToggleSuspend:(bool)bSuspend;

static void interruptionListener(void* ClientData, UInt32 Interruption);

-(UIWindow*)window;

@end

void InstallSignalHandlers();
