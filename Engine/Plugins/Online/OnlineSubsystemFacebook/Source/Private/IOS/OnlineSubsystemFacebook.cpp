// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemFacebook.h"
#include "OnlineSubsystemFacebookPrivate.h"

#include "OnlineFriendsFacebook.h"
#include "OnlineIdentityFacebook.h"
#include "OnlineSharingFacebook.h"
#include "OnlineUserFacebook.h"

#import <FBSDKCoreKit/FBSDKCoreKit.h>
#import <FBSDKCoreKit/FBSDKSettings.h>

#include "Misc/CoreDelegates.h"
#include "IOS/IOSAppDelegate.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"

#define FACEBOOK_DEBUG_ENABLED 0

FOnlineSubsystemFacebook::FOnlineSubsystemFacebook(FName InInstanceName)
	: FOnlineSubsystemFacebookCommon(InInstanceName)
{
	FString IOSFacebookAppID;
	if (!GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("FacebookAppID"), IOSFacebookAppID, GEngineIni))
	{
		UE_LOG_ONLINE(Warning, TEXT("The [IOSRuntimeSettings]:FacebookAppID has not been set"));
	}

	if (ClientId.IsEmpty() || IOSFacebookAppID.IsEmpty() || (IOSFacebookAppID != ClientId))
	{
		UE_LOG_ONLINE(Warning, TEXT("Inconsistency between OnlineSubsystemFacebook AppId [%s] and IOSRuntimeSettings AppId [%s]"), *ClientId, *IOSFacebookAppID);
	}
}

FOnlineSubsystemFacebook::~FOnlineSubsystemFacebook()
{
}

static void OnFacebookOpenURL(UIApplication* application, NSURL* url, NSString* sourceApplication, id annotation)
{
	[[FBSDKApplicationDelegate sharedInstance] application:application
		openURL : url
		sourceApplication : sourceApplication
		annotation : annotation];
}

static void OnFacebookAppDidBecomeActive()
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		[FBSDKAppEvents activateApp];
	});
}

/** Add verbose logging for various Facebook SDK features */
void SetFBLoggingBehavior()
{
#if FACEBOOK_DEBUG_ENABLED
	[FBSDKSettings enableLoggingBehavior:FBSDKLoggingBehaviorAccessTokens];
	[FBSDKSettings enableLoggingBehavior:FBSDKLoggingBehaviorPerformanceCharacteristics];
	[FBSDKSettings enableLoggingBehavior:FBSDKLoggingBehaviorAppEvents];
	[FBSDKSettings enableLoggingBehavior:FBSDKLoggingBehaviorInformational];
	[FBSDKSettings enableLoggingBehavior:FBSDKLoggingBehaviorCacheErrors];
	[FBSDKSettings enableLoggingBehavior:FBSDKLoggingBehaviorUIControlErrors];
	[FBSDKSettings enableLoggingBehavior:FBSDKLoggingBehaviorGraphAPIDebugWarning];
	[FBSDKSettings enableLoggingBehavior:FBSDKLoggingBehaviorGraphAPIDebugInfo];
	[FBSDKSettings enableLoggingBehavior:FBSDKLoggingBehaviorNetworkRequests];
	[FBSDKSettings enableLoggingBehavior:FBSDKLoggingBehaviorDeveloperErrors];
#endif
}

/** Print various details about the Facebook SDK */
void PrintSDKStatus()
{
	NSString* AppId = [FBSDKSettings appID];
	NSString* SDKVersion = [FBSDKSettings sdkVersion];
	NSString* GraphVer = [FBSDKSettings graphAPIVersion];
	NSString* OverrideAppId = [FBSDKAppEvents loggingOverrideAppID];
	NSSet* LoggingBehaviors = [FBSDKSettings loggingBehavior];

	UE_LOG_ONLINE(Verbose, TEXT("Facebook SDK:%s"), *FString(SDKVersion));
	UE_LOG_ONLINE(Verbose, TEXT("AppId:%s"), *FString(AppId));
	UE_LOG_ONLINE(Verbose, TEXT("OverridAppId:%s"), *FString(OverrideAppId));
	UE_LOG_ONLINE(Verbose, TEXT("GraphVer:%s"), *FString(GraphVer));

	if (LoggingBehaviors != nil && [LoggingBehaviors count] > 0)
	{
		UE_LOG_ONLINE(Verbose, TEXT("Logging:"));
		for (NSString* loggingBehavior in LoggingBehaviors)
		{
			UE_LOG_ONLINE(Verbose, TEXT(" - %s"), *FString(loggingBehavior));
		}
	}
}

bool FOnlineSubsystemFacebook::Init()
{
	bool bSuccessfullyStartedUp = false;
	if (FOnlineSubsystemFacebookCommon::Init())
	{
		FIOSCoreDelegates::OnOpenURL.AddStatic(&OnFacebookOpenURL);
		FCoreDelegates::ApplicationHasReactivatedDelegate.AddStatic(&OnFacebookAppDidBecomeActive);

		FOnlineIdentityFacebookPtr TempPtr = MakeShareable(new FOnlineIdentityFacebook(this));
		if (TempPtr->Init())
		{
			FacebookIdentity = TempPtr;
		}
		FacebookSharing = MakeShareable(new FOnlineSharingFacebook(this));
		FacebookFriends = MakeShareable(new FOnlineFriendsFacebook(this));
		FacebookUser = MakeShareable(new FOnlineUserFacebook(this));

		FString AnalyticsId;
		GConfig->GetString(TEXT("OnlineSubsystemFacebook"), TEXT("AnalyticsId"), AnalyticsId, GEngineIni);

		NSString* APIVerStr = [NSString stringWithFString:GetAPIVer()];
		[FBSDKSettings setGraphAPIVersion:APIVerStr];
		SetFBLoggingBehavior();

		// Trigger Facebook SDK last now that everything is setup
		dispatch_async(dispatch_get_main_queue(), ^
		{
			UIApplication* sharedApp = [UIApplication sharedApplication];
			NSDictionary* launchDict = [IOSAppDelegate GetDelegate].launchOptions;
			if (!AnalyticsId.IsEmpty())
			{
				NSString* AnalyticsStr = [NSString stringWithFString:AnalyticsId];
				[FBSDKAppEvents setLoggingOverrideAppID:AnalyticsStr];
			}
			[FBSDKAppEvents activateApp];
			[[FBSDKApplicationDelegate sharedInstance] application:sharedApp didFinishLaunchingWithOptions: launchDict];
			PrintSDKStatus();
		});

		bSuccessfullyStartedUp = FacebookIdentity.IsValid() && FacebookSharing.IsValid() && FacebookFriends.IsValid() && FacebookUser.IsValid();
	}
	return bSuccessfullyStartedUp;
}

bool FOnlineSubsystemFacebook::Shutdown()
{
	bool bSuccessfullyShutdown = true;
	StaticCastSharedPtr<FOnlineIdentityFacebook>(FacebookIdentity)->Shutdown();

	bSuccessfullyShutdown = FOnlineSubsystemFacebookCommon::Shutdown();
	return bSuccessfullyShutdown;
}

bool FOnlineSubsystemFacebook::IsEnabled() const
{
	bool bEnableFacebookSupport = false;

	// IOSRuntimeSettings holds a value for editor ease of use
	if (!GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("bEnableFacebookSupport"), bEnableFacebookSupport, GEngineIni))
	{
		UE_LOG_ONLINE(Warning, TEXT("The [IOSRuntimeSettings]:bEnableFacebookSupport flag has not been set"));

		// Fallback to regular OSS location
		bEnableFacebookSupport = FOnlineSubsystemFacebookCommon::IsEnabled();
	}

	return bEnableFacebookSupport;
}

