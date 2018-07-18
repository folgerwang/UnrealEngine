// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Containers/Array.h"


struct CORE_API FDataDrivenPlatformInfoRegistry
{
	// Information about a platform loaded from disk
	struct FPlatformInfo
	{
		FPlatformInfo()
			: bIsConfidential(false)
		{
		}

		// is this platform confidential
		bool bIsConfidential;

		// ini parent (ie TVOS would have IOS as ini parent)
		FString IniParent;
	};


	/**
	 * Get the data driven platform info for a given platform. If the platform doesn't have any on disk,
	 * this will return a default constructed FConfigDataDrivenPlatformInfo
	 */
	static const FPlatformInfo& GetPlatformInfo(const FString& PlatformName);

	/**
	 * Gets a list of all known confidential platforms (note these are just the platforms you have access to, so, for example PS4 won't be
	 * returned if you are not a PS4 licensee)
	 */
	static const TArray<FString>& GetConfidentialPlatforms();


private:
	/**
	* Get the global set of data driven platform information
	*/
	static const TMap<FString, FPlatformInfo>& GetAllPlatformInfos();
};
