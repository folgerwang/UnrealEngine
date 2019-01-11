// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"

#include "SocialSettings.generated.h"

enum class ESocialSubsystem : uint8;

/**
 * Config-driven settings object for the social framework.
 * Only the CDO is ever expected to be used, no instance is ever expected to be created.
 */
UCLASS(Config = Game)
class PARTY_API USocialSettings : public UObject
{
	GENERATED_BODY()

public:
	USocialSettings();

	static FString GetUniqueIdEnvironmentPrefix(ESocialSubsystem SubsystemType);
	static bool ShouldPreferPlatformInvites();
	static int32 GetDefaultMaxPartySize();

private:
	/**
	 * The specific OSS' that have their IDs stored with an additional prefix for the environment to which they pertain.
	 * This is only necessary for OSS' (ex: Switch) that do not have separate environments, just one big pot with both dev and prod users/friendships/etc.
	 * For these cases, the linked account ID stored on the Primary UserInfo for this particular OSS will be prefixed with the specific environment in which the linkage exists.
	 * Additionally, the prefix must be prepended when mapping the external ID to a primary ID.
	 * Overall, it's a major hassle that can hopefully be done away with eventually, but for now is necessary to fake environmental behavior on OSS' without environments.
	 */
	UPROPERTY(config)
	TArray<FName> OssNamesWithEnvironmentIdPrefix;

	UPROPERTY(config)
	int32 DefaultMaxPartySize = 4;

	UPROPERTY(config)
	bool bPreferPlatformInvites = true;
};