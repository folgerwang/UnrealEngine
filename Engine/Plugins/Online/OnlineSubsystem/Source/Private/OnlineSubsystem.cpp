// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystem.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/IConsoleManager.h"
#include "NboSerializer.h"
#include "Misc/NetworkVersion.h"
#include "Logging/LogMacros.h"
#include "Misc/EngineVersion.h"

#include "Interfaces/OnlineChatInterface.h"
#include "Interfaces/OnlinePartyInterface.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "Interfaces/OnlineUserInterface.h"
#include "Interfaces/OnlineEventsInterface.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Interfaces/OnlineStoreInterface.h"
#include "Interfaces/OnlineStoreInterfaceV2.h"
#include "Interfaces/OnlinePurchaseInterface.h"
#include "Interfaces/OnlineSharingInterface.h"
#include "Interfaces/OnlineFriendsInterface.h"
#include "Interfaces/OnlineExternalUIInterface.h"
#include "Interfaces/OnlineAchievementsInterface.h"
#include "Interfaces/OnlineUserCloudInterface.h"
#include "Interfaces/OnlineTitleFileInterface.h"
#include "Interfaces/OnlinePresenceInterface.h"
#include "Interfaces/VoiceInterface.h"
#include "Interfaces/OnlineLeaderboardInterface.h"
#include "Interfaces/OnlineTournamentInterface.h"
#include "Interfaces/OnlineStatsInterface.h"

DEFINE_LOG_CATEGORY(LogOnline);
DEFINE_LOG_CATEGORY(LogOnlineGame);
DEFINE_LOG_CATEGORY(LogOnlineChat);
DEFINE_LOG_CATEGORY(LogVoiceEngine);
DEFINE_LOG_CATEGORY(LogOnlineVoice);
DEFINE_LOG_CATEGORY(LogOnlineSession);
DEFINE_LOG_CATEGORY(LogOnlineUser);
DEFINE_LOG_CATEGORY(LogOnlineFriend);
DEFINE_LOG_CATEGORY(LogOnlineIdentity);
DEFINE_LOG_CATEGORY(LogOnlinePresence);
DEFINE_LOG_CATEGORY(LogOnlineExternalUI);
DEFINE_LOG_CATEGORY(LogOnlineAchievements);
DEFINE_LOG_CATEGORY(LogOnlineLeaderboard);
DEFINE_LOG_CATEGORY(LogOnlineCloud);
DEFINE_LOG_CATEGORY(LogOnlineTitleFile);
DEFINE_LOG_CATEGORY(LogOnlineEntitlement);
DEFINE_LOG_CATEGORY(LogOnlineEvents);
DEFINE_LOG_CATEGORY(LogOnlineSharing);
DEFINE_LOG_CATEGORY(LogOnlineStore);
DEFINE_LOG_CATEGORY(LogOnlineStoreV2);
DEFINE_LOG_CATEGORY(LogOnlinePurchase);
DEFINE_LOG_CATEGORY(LogOnlineTournament);
DEFINE_LOG_CATEGORY(LogOnlineStats);

#if STATS
ONLINESUBSYSTEM_API DEFINE_STAT(STAT_Online_Async);
ONLINESUBSYSTEM_API DEFINE_STAT(STAT_Online_AsyncTasks);
ONLINESUBSYSTEM_API DEFINE_STAT(STAT_Session_Interface);
ONLINESUBSYSTEM_API DEFINE_STAT(STAT_Voice_Interface);
#endif

namespace OnlineIdentity
{
	namespace Errors
	{
		// Params
		const FString AuthLoginParam = TEXT("auth_login");
		const FString AuthTypeParam = TEXT("auth_type");
		const FString AuthPasswordParam = TEXT("auth_password");

		// Results
		const FString NoUserId = TEXT("no_user_id");
		const FString NoAuthToken = TEXT("no_auth_token");
		const FString NoAuthType = TEXT("no_auth_type");
	}
}

/** Workaround, please avoid using this */
TSharedPtr<const FUniqueNetId> GetFirstSignedInUser(IOnlineIdentityPtr IdentityInt)
{
	TSharedPtr<const FUniqueNetId> UserId = nullptr;
	if (IdentityInt.IsValid())
	{
		for (int32 i = 0; i < MAX_LOCAL_PLAYERS; i++)
		{
			UserId = IdentityInt->GetUniquePlayerId(i);
			if (UserId.IsValid() && UserId->IsValid())
			{
				break;
			}
		}
	}

	return UserId;
}

int32 GetBuildUniqueId()
{
	static bool bStaticCheck = false;
	static int32 BuildId = 0;
	static bool bUseBuildIdOverride = false;
	static int32 BuildIdOverride = 0;

	// add a cvar so it can be modified at runtime
	static FAutoConsoleVariableRef CVarBuildIdOverride(
		TEXT("buildidoverride"), BuildId,
		TEXT("Sets build id used for matchmaking "),
		ECVF_Default);

	if (!bStaticCheck)
	{
		bStaticCheck = true;
		if (FParse::Value(FCommandLine::Get(), TEXT("BuildIdOverride="), BuildIdOverride) && BuildIdOverride != 0)
		{
			bUseBuildIdOverride = true;
		}
		else
		{
			if (!GConfig->GetBool(TEXT("OnlineSubsystem"), TEXT("bUseBuildIdOverride"), bUseBuildIdOverride, GEngineIni))
			{
				UE_LOG_ONLINE(Warning, TEXT("Missing bUseBuildIdOverride= in [OnlineSubsystem] of DefaultEngine.ini"));
			}

			if (!GConfig->GetInt(TEXT("OnlineSubsystem"), TEXT("BuildIdOverride"), BuildIdOverride, GEngineIni))
			{
				UE_LOG_ONLINE(Warning, TEXT("Missing BuildIdOverride= in [OnlineSubsystem] of DefaultEngine.ini"));
			}
		}

		if (bUseBuildIdOverride == false)
		{
			// Removed old hashing code to use something more predictable and easier to override for when
			// it's necessary to force compatibility with an older build
			BuildId = FNetworkVersion::GetNetworkCompatibleChangelist();
		}
		else
		{
			BuildId = BuildIdOverride;
		}
	}

	return BuildId;
}

TAutoConsoleVariable<FString> CVarPlatformOverride(
	TEXT("oss.PlatformOverride"),
	TEXT(""),
	TEXT("Overrides the detected platform of this client for various debugging\n")
	TEXT("Valid values WIN MAC PSN XBL IOS AND LIN SWT OTHER"),
	ECVF_Cheat);

FString IOnlineSubsystem::GetLocalPlatformName()
{
	FString OnlinePlatform;

	OnlinePlatform = CVarPlatformOverride.GetValueOnAnyThread();
	if (!OnlinePlatform.IsEmpty())
	{
		return OnlinePlatform.ToUpper();
	}
#if !UE_BUILD_SHIPPING
	FParse::Value(FCommandLine::Get(), TEXT("PLATFORMTEST="), OnlinePlatform);
	if (!OnlinePlatform.IsEmpty())
	{
		return OnlinePlatform.ToUpper();
	}
#endif
	GConfig->GetString(TEXT("OnlineSubsystem"), TEXT("LocalPlatformName"), OnlinePlatform, GEngineIni);
	if (!OnlinePlatform.IsEmpty())
	{
		return OnlinePlatform.ToUpper();
	}
	if (PLATFORM_PS4)
	{
		OnlinePlatform = OSS_PLATFORM_NAME_PS4;
	}
	else if (PLATFORM_XBOXONE)
	{
		OnlinePlatform = OSS_PLATFORM_NAME_XBOX;
	}
	else if (PLATFORM_WINDOWS)
	{
		OnlinePlatform = OSS_PLATFORM_NAME_WINDOWS;
	}
	else if (PLATFORM_MAC)
	{
		OnlinePlatform = OSS_PLATFORM_NAME_MAC;
	}
	else if (PLATFORM_LINUX)
	{
		OnlinePlatform = OSS_PLATFORM_NAME_LINUX;
	}
	else if (PLATFORM_IOS)
	{
		OnlinePlatform = OSS_PLATFORM_NAME_IOS;
	}
	else if (PLATFORM_ANDROID)
	{
		OnlinePlatform = OSS_PLATFORM_NAME_ANDROID;
	}
	else if (PLATFORM_SWITCH)
	{
		OnlinePlatform = OSS_PLATFORM_NAME_SWITCH;
	}
	else
	{
		OnlinePlatform = OSS_PLATFORM_NAME_OTHER;
	}
	return OnlinePlatform;
}

bool IsPlayerInSessionImpl(IOnlineSession* SessionInt, FName SessionName, const FUniqueNetId& UniqueId)
{
	bool bFound = false;
	FNamedOnlineSession* Session = SessionInt->GetNamedSession(SessionName);
	if (Session != NULL)
	{
		const bool bIsSessionOwner = *Session->OwningUserId == UniqueId;

		FUniqueNetIdMatcher PlayerMatch(UniqueId);
		if (bIsSessionOwner || 
			Session->RegisteredPlayers.IndexOfByPredicate(PlayerMatch) != INDEX_NONE)
		{
			bFound = true;
		}
	}
	return bFound;
}

bool IsUniqueIdLocal(const FUniqueNetId& UniqueId)
{
	if (IOnlineSubsystem::DoesInstanceExist(UniqueId.GetType()))
	{
		IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get(UniqueId.GetType());
		return OnlineSub ? OnlineSub->IsLocalPlayer(UniqueId) : false;
	}

	return false;
}

int32 GetBeaconPortFromSessionSettings(const FOnlineSessionSettings& SessionSettings)
{
	int32 BeaconListenPort = DEFAULT_BEACON_PORT;
	if (!SessionSettings.Get(SETTING_BEACONPORT, BeaconListenPort) || BeaconListenPort <= 0)
	{
		// Reset the default BeaconListenPort back to DEFAULT_BEACON_PORT because the SessionSettings value does not exist or was not valid
		BeaconListenPort = DEFAULT_BEACON_PORT;
	}
	return BeaconListenPort;
}

#if !UE_BUILD_SHIPPING

static void ResetAchievements()
{
	IOnlineSubsystem* OnlineSub = IOnlineSubsystem::Get();
	IOnlineIdentityPtr IdentityInterface = OnlineSub ? OnlineSub->GetIdentityInterface() : nullptr;
	if (!IdentityInterface.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("ResetAchievements command: couldn't get the identity interface"));
		return;
	}
	
	TSharedPtr<const FUniqueNetId> UserId = IdentityInterface->GetUniquePlayerId(0);
	if(!UserId.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("ResetAchievements command: invalid UserId"));
		return;
	}

	IOnlineAchievementsPtr AchievementsInterface = OnlineSub ? OnlineSub->GetAchievementsInterface() : nullptr;
	if (!AchievementsInterface.IsValid())
	{
		UE_LOG_ONLINE(Warning, TEXT("ResetAchievements command: couldn't get the achievements interface"));
		return;
	}

	AchievementsInterface->ResetAchievements(*UserId);
}

FAutoConsoleCommand CmdResetAchievements(
	TEXT("online.ResetAchievements"),
	TEXT("Reset achievements for the currently logged in user."),
	FConsoleCommandDelegate::CreateStatic(ResetAchievements)
	);

#endif

bool IOnlineSubsystem::IsEnabled(const FName& SubsystemName)
{
	bool bIsDisabledByCommandLine = false;
#if !UE_BUILD_SHIPPING && !UE_BUILD_SHIPPING_WITH_EDITOR
	// In non-shipping builds, check for a command line override to disable the OSS
	bIsDisabledByCommandLine = FParse::Param(FCommandLine::Get(), *FString::Printf(TEXT("no%s"), *SubsystemName.ToString()));
#endif
	
	if (!bIsDisabledByCommandLine)
	{
		bool bIsEnabledByConfig = false;
		const FString ConfigSection(FString::Printf(TEXT("OnlineSubsystem%s"), *SubsystemName.ToString()));
		const bool bConfigOptionExists = GConfig->GetBool(*ConfigSection, TEXT("bEnabled"), bIsEnabledByConfig, GEngineIni);
		UE_CLOG_ONLINE(!bConfigOptionExists, Verbose, TEXT("[%s].bEnabled is not set, defaulting to true"), *ConfigSection);
	
		return !bConfigOptionExists || bIsEnabledByConfig;
	}
	return false;
}
