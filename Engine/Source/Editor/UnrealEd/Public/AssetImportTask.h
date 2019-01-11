// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "AssetImportTask.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogAssetImportTask, Log, All);

class UFactory;

/**
 * Contains data for a group of assets to import
 */ 
UCLASS(Transient, BlueprintType)
class UNREALED_API UAssetImportTask : public UObject
{
	GENERATED_BODY()

public:
	UAssetImportTask();

public:
	/** Filename to import */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	FString Filename;

	/** Content path in the projects content directory where asset will be imported */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	FString DestinationPath;

	/** Optional custom name to import as */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	FString DestinationName;

	/** Overwrite existing assets */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	bool bReplaceExisting;

	/** Avoid dialogs */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	bool bAutomated;

	/** Save after importing */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	bool bSave;

	/** Optional factory to use */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	UFactory* Factory;

	/** Import options specific to the type of asset */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	UObject* Options;

	/** Paths to objects created or updated after import */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	TArray<FString> ImportedObjectPaths;

	/** Imported object */
	UPROPERTY(BlueprintReadWrite, Category = "Asset Import Task")
	UObject* Result;
};

