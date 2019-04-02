// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Commandlets/Commandlet.h"
#include "LocTextHelper.h"
#include "LocalizationSourceControlUtil.h"
#include "LocalizedAssetUtil.h"
#include "GatherTextCommandletBase.generated.h"

/** Performs fuzzy path matching against a set of include and exclude paths */
class FFuzzyPathMatcher
{
public:
	enum EPathMatch
	{
		Included,
		Excluded,
		NoMatch,
	};

public:
	FFuzzyPathMatcher(const TArray<FString>& InIncludePathFilters, const TArray<FString>& InExcludePathFilters);

	EPathMatch TestPath(const FString& InPathToTest) const;

private:
	enum EPathType : uint8
	{
		Include,
		Exclude,
	};

	struct FFuzzyPath
	{
		FFuzzyPath(FString InPathFilter, const EPathType InPathType)
			: PathFilter(MoveTemp(InPathFilter))
			, PathType(InPathType)
		{
		}

		FString PathFilter;
		EPathType PathType;
	};

	TArray<FFuzzyPath> FuzzyPaths;
};

/**
 *	UGatherTextCommandletBase: Base class for localization commandlets. Just to force certain behaviors and provide helper functionality. 
 */
UCLASS()
class UNREALED_API UGatherTextCommandletBase : public UCommandlet
{
	GENERATED_UCLASS_BODY()

public:
	virtual void Initialize( const TSharedRef< FLocTextHelper >& InGatherManifestHelper, const TSharedPtr< FLocalizationSCC >& InSourceControlInfo );

	// Wrappers for extracting config values
	bool GetBoolFromConfig( const TCHAR* Section, const TCHAR* Key, bool& OutValue, const FString& Filename );
	bool GetStringFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename );
	bool GetPathFromConfig( const TCHAR* Section, const TCHAR* Key, FString& OutValue, const FString& Filename );
	int32 GetStringArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename );
	int32 GetPathArrayFromConfig( const TCHAR* Section, const TCHAR* Key, TArray<FString>& OutArr, const FString& Filename );

	// Utilities for split platform detection
	bool IsSplitPlatformName(const FName InPlatformName) const;
	bool ShouldSplitPlatformForPath(const FString& InPath, FName* OutPlatformName = nullptr) const;
	FName GetSplitPlatformNameFromPath(const FString& InPath) const;

protected:
	TSharedPtr< FLocTextHelper > GatherManifestHelper;

	TSharedPtr< FLocalizationSCC > SourceControlInfo;

	/** Mapping from platform name to the path marker for that platform */
	TMap<FName, FString> SplitPlatforms;

private:
	virtual void CreateCustomEngine(const FString& Params) override ; //Disallow other text commandlets to make their own engine.	
};
