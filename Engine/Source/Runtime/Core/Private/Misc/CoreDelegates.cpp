// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

// Core includes.
#include "Misc/CoreDelegates.h"
#include "Math/Vector.h"


//////////////////////////////////////////////////////////////////////////
// FCoreDelegates

TArray<FCoreDelegates::FHotFixDelegate> FCoreDelegates::HotFixDelegates;
TArray<FCoreDelegates::FResolvePackageNameDelegate> FCoreDelegates::PackageNameResolvers;

FCoreDelegates::FHotFixDelegate& FCoreDelegates::GetHotfixDelegate(EHotfixDelegates::Type HotFix)
{
	if (HotFix >= HotFixDelegates.Num())
	{
		HotFixDelegates.SetNum(HotFix + 1);
	}
	return HotFixDelegates[HotFix];
}

FCoreDelegates::FOnPreMainInit& FCoreDelegates::GetPreMainInitDelegate()
{
	static FOnPreMainInit StaticDelegate;
	return StaticDelegate;
}

FCoreDelegates::FOnMountAllPakFiles FCoreDelegates::OnMountAllPakFiles;
FCoreDelegates::FOnMountPak FCoreDelegates::OnMountPak;
FCoreDelegates::FOnUnmountPak FCoreDelegates::OnUnmountPak;
FCoreDelegates::FPakFileMountedDelegate FCoreDelegates::PakFileMountedCallback;
FCoreDelegates::FNoPakFilesMountedDelegate FCoreDelegates::NoPakFilesMountedDelegate;
FCoreDelegates::FOnUserLoginChangedEvent FCoreDelegates::OnUserLoginChangedEvent;
FCoreDelegates::FOnUserControllerConnectionChange FCoreDelegates::OnControllerConnectionChange;
FCoreDelegates::FOnUserControllerPairingChange FCoreDelegates::OnControllerPairingChange;
FCoreDelegates::FOnSafeFrameChangedEvent FCoreDelegates::OnSafeFrameChangedEvent;
FCoreDelegates::FOnHandleSystemEnsure FCoreDelegates::OnHandleSystemEnsure;
FCoreDelegates::FOnHandleSystemError FCoreDelegates::OnHandleSystemError;
FCoreDelegates::FOnActorLabelChanged FCoreDelegates::OnActorLabelChanged;

FCoreDelegates::FRegisterMovieStreamerDelegate FCoreDelegates::RegisterMovieStreamerDelegate;
FCoreDelegates::FUnRegisterMovieStreamerDelegate FCoreDelegates::UnRegisterMovieStreamerDelegate;

FCoreDelegates::FRegisterEncryptionKeyDelegate& FCoreDelegates::GetRegisterEncryptionKeyDelegate()
{
	static FRegisterEncryptionKeyDelegate RegisterEncryptionKeyDelegate;
	return RegisterEncryptionKeyDelegate;
}

FCoreDelegates::FPakEncryptionKeyDelegate& FCoreDelegates::GetPakEncryptionKeyDelegate()
{
	static FPakEncryptionKeyDelegate PakEncryptionKeyDelegate;
	return PakEncryptionKeyDelegate;
}

FCoreDelegates::FPakSigningKeysDelegate& FCoreDelegates::GetPakSigningKeysDelegate()
{
	static FPakSigningKeysDelegate PakSigningKeysDelegate;
	return PakSigningKeysDelegate;
}

#if WITH_EDITOR
	FSimpleMulticastDelegate FCoreDelegates::PreModal;
	FSimpleMulticastDelegate FCoreDelegates::PostModal;
    FSimpleMulticastDelegate FCoreDelegates::PreSlateModal;
    FSimpleMulticastDelegate FCoreDelegates::PostSlateModal;
#endif	//WITH_EDITOR
FSimpleMulticastDelegate FCoreDelegates::OnShutdownAfterError;
FSimpleMulticastDelegate FCoreDelegates::OnInit;
FSimpleMulticastDelegate FCoreDelegates::OnPostEngineInit;
FSimpleMulticastDelegate FCoreDelegates::OnFEngineLoopInitComplete;
FSimpleMulticastDelegate FCoreDelegates::OnExit;
FSimpleMulticastDelegate FCoreDelegates::OnPreExit;
FCoreDelegates::FGatherAdditionalLocResPathsDelegate FCoreDelegates::GatherAdditionalLocResPathsCallback;
FSimpleMulticastDelegate FCoreDelegates::ColorPickerChanged;
FSimpleMulticastDelegate FCoreDelegates::OnBeginFrame;
FSimpleMulticastDelegate FCoreDelegates::OnEndFrame;
FSimpleMulticastDelegate FCoreDelegates::OnBeginFrameRT;
FSimpleMulticastDelegate FCoreDelegates::OnEndFrameRT;
FCoreDelegates::FOnModalMessageBox FCoreDelegates::ModalErrorMessage;
FCoreDelegates::FOnInviteAccepted FCoreDelegates::OnInviteAccepted;
FCoreDelegates::FWorldOriginOffset FCoreDelegates::PreWorldOriginOffset;
FCoreDelegates::FWorldOriginOffset FCoreDelegates::PostWorldOriginOffset;
FCoreDelegates::FStarvedGameLoop FCoreDelegates::StarvedGameLoop;
FCoreDelegates::FOnTemperatureChange FCoreDelegates::OnTemperatureChange;
FCoreDelegates::FOnLowPowerMode FCoreDelegates::OnLowPowerMode;

FCoreDelegates::FApplicationLifetimeDelegate FCoreDelegates::ApplicationWillDeactivateDelegate;
FCoreDelegates::FApplicationLifetimeDelegate FCoreDelegates::ApplicationHasReactivatedDelegate;
FCoreDelegates::FApplicationLifetimeDelegate FCoreDelegates::ApplicationWillEnterBackgroundDelegate;
FCoreDelegates::FApplicationLifetimeDelegate FCoreDelegates::ApplicationHasEnteredForegroundDelegate;
FCoreDelegates::FApplicationLifetimeDelegate FCoreDelegates::ApplicationWillTerminateDelegate;
FCoreDelegates::FApplicationLifetimeDelegate FCoreDelegates::ApplicationShouldUnloadResourcesDelegate;

FCoreDelegates::FApplicationStartupArgumentsDelegate FCoreDelegates::ApplicationReceivedStartupArgumentsDelegate;

FCoreDelegates::FUserMusicInterruptDelegate FCoreDelegates::UserMusicInterruptDelegate;
FCoreDelegates::FAudioRouteChangedDelegate FCoreDelegates::AudioRouteChangedDelegate;
FCoreDelegates::FAudioMuteDelegate FCoreDelegates::AudioMuteDelegate;
FCoreDelegates::FApplicationRequestAudioState FCoreDelegates::ApplicationRequestAudioState;

FCoreDelegates::FApplicationRegisteredForRemoteNotificationsDelegate FCoreDelegates::ApplicationRegisteredForRemoteNotificationsDelegate;
FCoreDelegates::FApplicationRegisteredForUserNotificationsDelegate FCoreDelegates::ApplicationRegisteredForUserNotificationsDelegate;
FCoreDelegates::FApplicationFailedToRegisterForRemoteNotificationsDelegate FCoreDelegates::ApplicationFailedToRegisterForRemoteNotificationsDelegate;
FCoreDelegates::FApplicationReceivedRemoteNotificationDelegate FCoreDelegates::ApplicationReceivedRemoteNotificationDelegate;
FCoreDelegates::FApplicationReceivedLocalNotificationDelegate FCoreDelegates::ApplicationReceivedLocalNotificationDelegate;

FCoreDelegates::FApplicationPerformFetchDelegate FCoreDelegates::ApplicationPerformFetchDelegate;
FCoreDelegates::FApplicationBackgroundSessionEventDelegate FCoreDelegates::ApplicationBackgroundSessionEventDelegate;

FCoreDelegates::FCountPreLoadConfigFileRespondersDelegate FCoreDelegates::CountPreLoadConfigFileRespondersDelegate;
FCoreDelegates::FPreLoadConfigFileDelegate FCoreDelegates::PreLoadConfigFileDelegate;
FCoreDelegates::FPreSaveConfigFileDelegate FCoreDelegates::PreSaveConfigFileDelegate;
FCoreDelegates::FOnFConfigFileCreated FCoreDelegates::OnFConfigCreated;
FCoreDelegates::FOnFConfigFileCreated FCoreDelegates::OnFConfigDeleted;
FCoreDelegates::FOnApplyCVarFromIni FCoreDelegates::OnApplyCVarFromIni;
FCoreDelegates::FOnSystemResolutionChanged FCoreDelegates::OnSystemResolutionChanged;

#if WITH_EDITOR
FCoreDelegates::FOnTargetPlatformChangedSupportedFormats FCoreDelegates::OnTargetPlatformChangedSupportedFormats;
#endif 

FCoreDelegates::FStatCheckEnabled FCoreDelegates::StatCheckEnabled;
FCoreDelegates::FStatEnabled FCoreDelegates::StatEnabled;
FCoreDelegates::FStatDisabled FCoreDelegates::StatDisabled;
FCoreDelegates::FStatDisableAll FCoreDelegates::StatDisableAll;

FCoreDelegates::FApplicationLicenseChange FCoreDelegates::ApplicationLicenseChange;
FCoreDelegates::FPlatformChangedLaptopMode FCoreDelegates::PlatformChangedLaptopMode;

FCoreDelegates::FVRHeadsetRecenter FCoreDelegates::VRHeadsetTrackingInitializingAndNeedsHMDToBeTrackedDelegate;
FCoreDelegates::FVRHeadsetRecenter FCoreDelegates::VRHeadsetTrackingInitializedDelegate;
FCoreDelegates::FVRHeadsetRecenter FCoreDelegates::VRHeadsetRecenter;
FCoreDelegates::FVRHeadsetLost FCoreDelegates::VRHeadsetLost;
FCoreDelegates::FVRHeadsetReconnected FCoreDelegates::VRHeadsetReconnected;
FCoreDelegates::FVRHeadsetConnectCanceled FCoreDelegates::VRHeadsetConnectCanceled;
FCoreDelegates::FVRHeadsetPutOnHead FCoreDelegates::VRHeadsetPutOnHead;
FCoreDelegates::FVRHeadsetRemovedFromHead FCoreDelegates::VRHeadsetRemovedFromHead;
FCoreDelegates::FVRControllerRecentered FCoreDelegates::VRControllerRecentered;

FCoreDelegates::FOnUserActivityStringChanged FCoreDelegates::UserActivityStringChanged;
FCoreDelegates::FOnGameSessionIDChange FCoreDelegates::GameSessionIDChanged;
FCoreDelegates::FOnGameStateClassChange FCoreDelegates::GameStateClassChanged;
FCoreDelegates::FOnCrashOverrideParamsChanged FCoreDelegates::CrashOverrideParamsChanged;
FCoreDelegates::FOnIsVanillaProductChanged FCoreDelegates::IsVanillaProductChanged;

FCoreDelegates::FOnAsyncLoadingFlush FCoreDelegates::OnAsyncLoadingFlush;
FCoreDelegates::FOnAsyncLoadingFlushUpdate FCoreDelegates::OnAsyncLoadingFlushUpdate;
FCoreDelegates::FOnAsyncLoadPackage FCoreDelegates::OnAsyncLoadPackage;
FCoreDelegates::FOnSyncLoadPackage FCoreDelegates::OnSyncLoadPackage;
FCoreDelegates::FRenderingThreadChanged FCoreDelegates::PostRenderingThreadCreated;
FCoreDelegates::FRenderingThreadChanged FCoreDelegates::PreRenderingThreadDestroyed;

FCoreDelegates::FApplicationReceivedOnScreenOrientationChangedNotificationDelegate FCoreDelegates::ApplicationReceivedScreenOrientationChangedNotificationDelegate;

FCoreDelegates::FConfigReadyForUse FCoreDelegates::ConfigReadyForUse;

FCoreDelegates::FIsLoadingMovieCurrentlyPlaying FCoreDelegates::IsLoadingMovieCurrentlyPlaying;

FCoreDelegates::FShouldLaunchUrl FCoreDelegates::ShouldLaunchUrl;

PRAGMA_DISABLE_DEPRECATION_WARNINGS
FCrashOverrideParameters::~FCrashOverrideParameters()
{
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

/**	 Implemented as a function to address global ctor issues */
FSimpleMulticastDelegate& FCoreDelegates::GetMemoryTrimDelegate()
{
	static FSimpleMulticastDelegate OnMemoryTrim;;
	return OnMemoryTrim;
}

/**	 Implemented as a function to address global ctor issues */
FSimpleMulticastDelegate& FCoreDelegates::GetOutOfMemoryDelegate()
{
	static FSimpleMulticastDelegate OnOOM;
	return OnOOM;
}

FCoreDelegates::FGetOnScreenMessagesDelegate FCoreDelegates::OnGetOnScreenMessages;

typedef void(*TSigningKeyFunc)(TArray<uint8>&, TArray<uint8>&);
typedef void(*TEncryptionKeyFunc)(unsigned char[32]);

void RegisterSigningKeyCallback(TSigningKeyFunc InCallback)
{
	FCoreDelegates::GetPakSigningKeysDelegate().BindLambda([InCallback](TArray<uint8>& OutExponent, TArray<uint8>& OutModulus)
	{
		InCallback(OutExponent, OutModulus);
	});
}

void RegisterEncryptionKeyCallback(TEncryptionKeyFunc InCallback)
{
	FCoreDelegates::GetPakEncryptionKeyDelegate().BindLambda([InCallback](uint8 OutKey[32])
	{
		InCallback(OutKey);
	});
}

