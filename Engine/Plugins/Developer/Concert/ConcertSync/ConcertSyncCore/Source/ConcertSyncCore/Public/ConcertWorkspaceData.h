// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ConcertWorkspaceData.generated.h"

UENUM()
enum class EConcertPackageUpdateType : uint8
{
	/** A dummy update, typically used to fence some transactions as no longer relevant */
	Dummy,
	/** This package has been added, but not yet saved */
	Added,
	/** This package has been saved */
	Saved,
	/** This package has been renamed (leaving a redirector) */
	Renamed,
	/** This package has been deleted */
	Deleted,
};

USTRUCT()
struct FConcertPackageInfo
{
	GENERATED_BODY()

	FConcertPackageInfo()
		: PackageUpdateType(EConcertPackageUpdateType::Dummy)
		, NextTransactionIndexWhenSaved(0)
	{
	}

	/** The name of the package */
	UPROPERTY()
	FName PackageName;

	/** The new name of the package (if PackageUpdateType == EConcertPackageUpdateType::Renamed) */
	UPROPERTY()
	FName NewPackageName;

	/** The extension of the package file on disk (eg, .umap or .uasset) */
	UPROPERTY()
	FString PackageFileExtension;

	/** What kind of package update is this? */
	UPROPERTY()
	EConcertPackageUpdateType PackageUpdateType;

	/** What was the next transaction index when this update was made (to discard older transactions that applied to this package) */
	UPROPERTY()
	uint64 NextTransactionIndexWhenSaved;
};

USTRUCT()
struct FConcertPackage
{
	GENERATED_BODY()

	UPROPERTY()
	FConcertPackageInfo Info;

	UPROPERTY()
	TArray<uint8> PackageData;
};
