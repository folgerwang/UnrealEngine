// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SocialSettings.h"
#include "SocialManager.h"

USocialSettings::USocialSettings()
{
	// Switch is the only default supported OSS that does not itself support multiple environments
	OssNamesWithEnvironmentIdPrefix.Add(SWITCH_SUBSYSTEM);
}

FString USocialSettings::GetUniqueIdEnvironmentPrefix(ESocialSubsystem SubsystemType)
{
	const USocialSettings& SettingsCDO = *GetDefault<USocialSettings>();

	// We don't need to worry about world specificity here for the OSS (both because there is no platform PIE and because we aren't accessing data that could differ if there was)
	IOnlineSubsystem* OSS = USocialManager::GetSocialOss(nullptr, SubsystemType);
	if (OSS && SettingsCDO.OssNamesWithEnvironmentIdPrefix.Contains(OSS->GetSubsystemName()))
	{
		return OSS->GetOnlineEnvironmentName() + TEXT("_");
	}
	return FString();
}

bool USocialSettings::ShouldPreferPlatformInvites()
{
	const USocialSettings& SettingsCDO = *GetDefault<USocialSettings>();
	return SettingsCDO.bPreferPlatformInvites;
}

int32 USocialSettings::GetDefaultMaxPartySize()
{
	const USocialSettings& SettingsCDO = *GetDefault<USocialSettings>();
	return SettingsCDO.DefaultMaxPartySize;
}
