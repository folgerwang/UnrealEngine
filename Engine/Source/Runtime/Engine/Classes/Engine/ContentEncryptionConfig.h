// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

/** Project specific configuration for content encryption */
class FContentEncryptionConfig
{
public:

	void AddPackage(FName InGroupName, FName InPackageName)
	{
		PackageGroups.FindOrAdd(InGroupName).Add(InPackageName);
	}

	void AddNonUFSFile(FName InGroupName, FName InFileName)
	{
		NonUFSFileGroups.FindOrAdd(InGroupName).Add(InFileName);
	}

	void AddReleasedKey(FGuid InKey)
	{
		ReleasedKeys.Add(InKey);
	}

	const TMap<FName, TSet<FName>>& GetPackageGroupMap() const
	{
		return PackageGroups;
	}

	const TMap<FName, TSet<FName>>& GetNonUFSFileGroupMap() const
	{
		return NonUFSFileGroups;
	}

	const TSet<FGuid>& GetReleasedKeys() const
	{
		return ReleasedKeys;
	}

private:

	TMap<FName, TSet<FName>> PackageGroups;
	TMap<FName, TSet<FName>> NonUFSFileGroups;
	TSet<FGuid> ReleasedKeys;
};