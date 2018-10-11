// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
 	IOSLocalNotification.cpp: Unreal IOSLocalNotification service interface object.
 =============================================================================*/

/*------------------------------------------------------------------------------------
	Includes
 ------------------------------------------------------------------------------------*/

#include "IOSLocalNotification.h"

#include "IOS/IOSApplication.h"
#include "IOS/IOSAppDelegate.h"

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
#import <UserNotifications/UserNotifications.h>
#endif

DEFINE_LOG_CATEGORY(LogIOSLocalNotification);

class FIOSLocalNotificationModule : public ILocalNotificationModule
{
public:

	/** Creates a new instance of the audio device implemented by the module. */
	virtual ILocalNotificationService* GetLocalNotificationService() override
	{
		static ILocalNotificationService*	oneTrueLocalNotificationService = nullptr;
		
		if(oneTrueLocalNotificationService == nullptr)
		{
			oneTrueLocalNotificationService = new FIOSLocalNotificationService;
		}
		
		return oneTrueLocalNotificationService;
	}

#if !PLATFORM_TVOS
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
	static UNMutableNotificationContent* CreateNotificationContent(const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent, uint32 BadgeNumber)
	{
		UNMutableNotificationContent* Content = [UNMutableNotificationContent new];
		if(Content != nil)
		{
			if(!Title.IsEmpty())
			{
				NSString* NotificationTitle = [NSString stringWithFString:Title.ToString()];
				if(NotificationTitle != nil)
				{
					Content.title = NotificationTitle;
				}
			}
			
			if(!Body.IsEmpty())
			{
				NSString* NotificationBody = [NSString stringWithFString:Body.ToString()];
				if(NotificationBody != nil)
				{
					Content.body = NotificationBody;
				}
			}
			
			NSNumber* BadgeNSNumber = [NSNumber numberWithInt:BadgeNumber];
			Content.badge = BadgeNSNumber;
			Content.sound = [UNNotificationSound defaultSound];
			
			if(!ActivationEvent.IsEmpty())
			{
				NSString* ActivationEventString = [NSString stringWithFString:ActivationEvent];
				NSString* LocalString = [NSString stringWithFString:FString(TEXT("Local"))];
				if (ActivationEventString != nil && LocalString != nil)
				{
					NSDictionary* Dict = [NSDictionary dictionaryWithObjectsAndKeys: ActivationEventString, @"ActivationEvent", LocalString, @"NotificationType", nil];
					if (Dict != nil)
					{
						Content.userInfo = Dict;
					}
				}
			}
		}
		
		return Content;
	}
	static UNCalendarNotificationTrigger* CreateCalendarNotificationTrigger(const FDateTime& FireDateTime)
	{
		NSCalendar *calendar = [NSCalendar autoupdatingCurrentCalendar];
		NSDateComponents *dateComps = [[NSDateComponents alloc] init];
		[dateComps setDay : FireDateTime.GetDay()];
		[dateComps setMonth : FireDateTime.GetMonth()];
		[dateComps setYear : FireDateTime.GetYear()];
		[dateComps setHour : FireDateTime.GetHour()];
		[dateComps setMinute : FireDateTime.GetMinute()];
		[dateComps setSecond : FireDateTime.GetSecond()];
		
		UNCalendarNotificationTrigger *trigger = [UNCalendarNotificationTrigger triggerWithDateMatchingComponents:dateComps repeats:NO];
		
		return trigger;
	}
#else
	static UILocalNotification* CreateLocalNotification(const FDateTime& FireDateTime, bool bLocalTime, const FString& ActivationEvent)
	{
		UIApplication* application = [UIApplication sharedApplication];

		NSCalendar *calendar = [NSCalendar autoupdatingCurrentCalendar];
		NSDateComponents *dateComps = [[NSDateComponents alloc] init];
		[dateComps setDay : FireDateTime.GetDay()];
		[dateComps setMonth : FireDateTime.GetMonth()];
		[dateComps setYear : FireDateTime.GetYear()];
		[dateComps setHour : FireDateTime.GetHour()];
		[dateComps setMinute : FireDateTime.GetMinute()];
		[dateComps setSecond : FireDateTime.GetSecond()];
		NSDate *itemDate = [calendar dateFromComponents : dateComps];

		UILocalNotification *localNotif = [[UILocalNotification alloc] init];
		if (localNotif != nil)
		{
			localNotif.fireDate = itemDate;
			if (bLocalTime)
			{
				localNotif.timeZone = [NSTimeZone defaultTimeZone];
			}
			else
			{
				localNotif.timeZone = nil;
			}

			NSString* activateEventNSString = [NSString stringWithFString:ActivationEvent];
			if (activateEventNSString != nil)
			{
				NSDictionary* infoDict = [NSDictionary dictionaryWithObject:activateEventNSString forKey:@"ActivationEvent"];
				if (infoDict != nil)
				{
					localNotif.userInfo = infoDict;
				}
			}
		}
		return localNotif;
	}
#endif
#endif
};

IMPLEMENT_MODULE(FIOSLocalNotificationModule, IOSLocalNotification);

/*------------------------------------------------------------------------------------
	FIOSLocalNotification
 ------------------------------------------------------------------------------------*/
FIOSLocalNotificationService::FIOSLocalNotificationService()
{
	AppLaunchedWithNotification = false;
	LaunchNotificationFireDate = 0;
}

void FIOSLocalNotificationService::ClearAllLocalNotifications()
{
#if !PLATFORM_TVOS
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
	UNUserNotificationCenter *Center = [UNUserNotificationCenter currentNotificationCenter];
	
	[Center removeAllPendingNotificationRequests];
#else
	UIApplication* application = [UIApplication sharedApplication];
	
	[application cancelAllLocalNotifications];
#endif
#endif
}

static uint32 NotificationNumber = 0;
void FIOSLocalNotificationService::ScheduleLocalNotificationAtTime(const FDateTime& FireDateTime, bool LocalTime, const FText& Title, const FText& Body, const FText& Action, const FString& ActivationEvent)
{
#if !PLATFORM_TVOS
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
	UNMutableNotificationContent* Content = FIOSLocalNotificationModule::CreateNotificationContent(Title, Body, Action, ActivationEvent, 1);
	UNCalendarNotificationTrigger* Trigger = FIOSLocalNotificationModule::CreateCalendarNotificationTrigger(FireDateTime);
	
	UNUserNotificationCenter *Center = [UNUserNotificationCenter currentNotificationCenter];
	
	FString NotId = Title.ToString() + FString::FromInt(NotificationNumber);
	NSString* NotificationIdentifier = [NSString stringWithFString:NotId];
	
	UNNotificationRequest *Request = [UNNotificationRequest requestWithIdentifier:NotificationIdentifier content:Content trigger:Trigger];
	
	[Center addNotificationRequest:Request withCompletionHandler:^(NSError * _Nullable error) {
		if (error != nil) {
			UE_LOG(LogIOSLocalNotification, Warning, TEXT("Error scheduling notification: %s"), *NotId);
		}
	}];
#else
	UILocalNotification *localNotif = FIOSLocalNotificationModule::CreateLocalNotification(FireDateTime, LocalTime, ActivationEvent);
	if (localNotif == nil)
		return;

	NSString*	alertBody = [NSString stringWithFString : Body.ToString()];
	if (alertBody != nil)
	{
		localNotif.alertBody = alertBody;
	}

	NSString*	alertAction = [NSString stringWithFString:Action.ToString()];
	if(alertAction != nil)
	{
		localNotif.alertAction = alertAction;
	}
	
	if([IOSAppDelegate GetDelegate].OSVersion >= 8.2f)
	{
		NSString*	alertTitle = [NSString stringWithFString:Title.ToString()];
		if(alertTitle != nil)
		{
			localNotif.alertTitle = alertTitle;
		}
	}
	
	localNotif.soundName = UILocalNotificationDefaultSoundName;
	localNotif.applicationIconBadgeNumber = 1;

	[[UIApplication sharedApplication] scheduleLocalNotification:localNotif];
#endif
#endif
}

void FIOSLocalNotificationService::ScheduleLocalNotificationBadgeAtTime(const FDateTime& FireDateTime, bool LocalTime, const FString& ActivationEvent)
{
#if !PLATFORM_TVOS
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
	UNMutableNotificationContent* Content = FIOSLocalNotificationModule::CreateNotificationContent(FText(), FText(), FText(), ActivationEvent, 1);
	UNCalendarNotificationTrigger* Trigger = FIOSLocalNotificationModule::CreateCalendarNotificationTrigger(FireDateTime);
	
	UNUserNotificationCenter *Center = [UNUserNotificationCenter currentNotificationCenter];
	
	FString NotId = FString(TEXT("Badge")) + FString::FromInt(NotificationNumber);
	NSString* NotificationIdentifier = [NSString stringWithFString:NotId];
	
	UNNotificationRequest *Request = [UNNotificationRequest requestWithIdentifier:NotificationIdentifier content:Content trigger:Trigger];
	
	[Center addNotificationRequest:Request withCompletionHandler:^(NSError * _Nullable error) {
		if (error != nil) {
			UE_LOG(LogIOSLocalNotification, Warning, TEXT("Error scheduling notification: %s"), *NotId);
		}
	}];
#else
	UILocalNotification *localNotif = FIOSLocalNotificationModule::CreateLocalNotification(FireDateTime, LocalTime, ActivationEvent);
	if (localNotif == nil)
		return;

	// As per Apple documentation, a nil 'alertBody' results in 'no alert'
	// https://developer.apple.com/reference/uikit/uilocalnotification/1616646-alertbody?language=objc
	localNotif.alertBody = nil;
	localNotif.applicationIconBadgeNumber = 1;

	[[UIApplication sharedApplication] scheduleLocalNotification:localNotif];
#endif
#endif
}

void FIOSLocalNotificationService::CancelLocalNotification(const FString& ActivationEvent)
{
	// TODO
}

void FIOSLocalNotificationService::GetLaunchNotification(bool& NotificationLaunchedApp, FString& ActivationEvent, int32& FireDate)
{
	NotificationLaunchedApp = AppLaunchedWithNotification;
	ActivationEvent = LaunchNotificationActivationEvent;
	FireDate = LaunchNotificationFireDate;
}

void FIOSLocalNotificationService::SetLaunchNotification(FString const& ActivationEvent, int32 FireDate)
{
	AppLaunchedWithNotification = true;
	LaunchNotificationActivationEvent = ActivationEvent;
	LaunchNotificationFireDate = FireDate;
}


static FIOSLocalNotificationService::FAllowedNotifications NotificationsAllowedDelegate;
void FIOSLocalNotificationService::CheckAllowedNotifications(const FAllowedNotifications& AllowedNotificationsDelegate)
{
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_10_0
	NotificationsAllowedDelegate = AllowedNotificationsDelegate;
	
	UNUserNotificationCenter *Center = [UNUserNotificationCenter currentNotificationCenter];
	[Center getNotificationSettingsWithCompletionHandler:^(UNNotificationSettings * _Nonnull settings) {
		bool NotificationsAllowed = settings.authorizationStatus == UNAuthorizationStatusAuthorized;
		NotificationsAllowedDelegate.ExecuteIfBound(NotificationsAllowed);
	}];
	
#else
	checkf(false, TEXT("For min iOS version < 10 use FIOSPlatformMisc::IsAllowedRemoteNotifications()."));
#endif
}
