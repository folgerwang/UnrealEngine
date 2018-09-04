// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/PackageName.h"

struct FCachedPackageFilename
{
	FCachedPackageFilename(FString&& InPackageFilename, FString&& InStandardFilename, FName InStandardFileFName)
	:	PackageFilename(MoveTemp(InPackageFilename))
	,	StandardFilename(MoveTemp(InStandardFilename))
	,	StandardFileFName(InStandardFileFName)
	{
	}

	FCachedPackageFilename(const FCachedPackageFilename& In) = default;

	FCachedPackageFilename(FCachedPackageFilename&& In)
	:	PackageFilename(MoveTemp(In.PackageFilename))
	,	StandardFilename(MoveTemp(In.StandardFilename))
	,	StandardFileFName(In.StandardFileFName)
	{
	}

	FString		PackageFilename; // this is also full path
	FString		StandardFilename;
	FName		StandardFileFName;
};

struct FPackageNameCache
{
	// Package name cache

	FString			GetCachedPackageFilename(const UPackage* Package) const;

	FString			GetCachedStandardPackageFilename(const UPackage* Package) const;

	FName			GetCachedStandardPackageFileFName(const FName& PackageName) const;
	FName			GetCachedStandardPackageFileFName(const UPackage* Package) const;

	const FName*	GetCachedPackageFilenameToPackageFName(const FName& StandardPackageFilename) const;

	void			ClearPackageFilenameCache() const;
	bool			ClearPackageFilenameCacheForPackage(const UPackage* Package) const;

private:
	const FCachedPackageFilename& Cache(const FName& PackageName) const;

	mutable TMap<FName, FCachedPackageFilename> PackageFilenameCache; // filename cache (only process the string operations once)
	mutable TMap<FName, FName>					PackageFilenameToPackageFNameCache;

	// temporary -- should eliminate the need for this
	friend uint32 UCookOnTheFlyServer::FullLoadAndSave(uint32& CookedPackageCount);
};

FName FPackageNameCache::GetCachedStandardPackageFileFName(const FName& PackageName) const
{
	return Cache(PackageName).StandardFileFName;
}

FString FPackageNameCache::GetCachedPackageFilename(const UPackage* Package) const
{
	// check( Package->GetName() == Package->GetFName().ToString() );
	return Cache(Package->GetFName()).PackageFilename;
}

FString FPackageNameCache::GetCachedStandardPackageFilename(const UPackage* Package) const
{
	// check( Package->GetName() == Package->GetFName().ToString() );
	return Cache(Package->GetFName()).StandardFilename;
}

FName FPackageNameCache::GetCachedStandardPackageFileFName(const UPackage* Package) const
{
	// check( Package->GetName() == Package->GetFName().ToString() );
	return Cache(Package->GetFName()).StandardFileFName;
}

bool FPackageNameCache::ClearPackageFilenameCacheForPackage(const UPackage* Package) const
{
	check(IsInGameThread());

	return PackageFilenameCache.Remove(Package->GetFName()) >= 1;
}

const FCachedPackageFilename& FPackageNameCache::Cache(const FName& PackageName) const
{
	check(IsInGameThread());

	FCachedPackageFilename *Cached = PackageFilenameCache.Find(PackageName);

	if (Cached != NULL)
	{
		return *Cached;
	}

	// cache all the things, like it's your birthday!

	FString Filename;
	FString PackageFilename;
	FString StandardFilename;
	FName StandardFileFName = NAME_None;

	if (FPackageName::DoesPackageExist(PackageName.ToString(), NULL, &Filename, false))
	{
		StandardFilename = PackageFilename = FPaths::ConvertRelativePathToFull(Filename);

		FPaths::MakeStandardFilename(StandardFilename);
		StandardFileFName = FName(*StandardFilename);
	}

	PackageFilenameToPackageFNameCache.Add(StandardFileFName, PackageName);

	return PackageFilenameCache.Emplace(PackageName, FCachedPackageFilename(MoveTemp(PackageFilename), MoveTemp(StandardFilename), StandardFileFName));
}

const FName* FPackageNameCache::GetCachedPackageFilenameToPackageFName(const FName& StandardPackageFilename) const
{
	check(IsInGameThread());
	const FName* Result = PackageFilenameToPackageFNameCache.Find(StandardPackageFilename);
	if (Result)
	{
		return Result;
	}

	FName PackageName = StandardPackageFilename;
	FString PotentialLongPackageName = StandardPackageFilename.ToString();
	if (FPackageName::IsValidLongPackageName(PotentialLongPackageName) == false)
	{
		PotentialLongPackageName = FPackageName::FilenameToLongPackageName(PotentialLongPackageName);
		PackageName = FName(*PotentialLongPackageName);
	}

	Cache(PackageName);

	return PackageFilenameToPackageFNameCache.Find(StandardPackageFilename);
}

void FPackageNameCache::ClearPackageFilenameCache() const
{
	check(IsInGameThread());
	PackageFilenameCache.Empty();
	PackageFilenameToPackageFNameCache.Empty();
}
