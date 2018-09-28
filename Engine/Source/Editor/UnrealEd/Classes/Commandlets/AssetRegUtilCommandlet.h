// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AssetRegUtilCommandlet.cpp: General-purpose commandlet for anything which
	makes integral use of the asset registry.
=============================================================================*/

#pragma once
#include "Commandlets/Commandlet.h"
#include "AssetRegUtilCommandlet.generated.h"

UNREALED_API DECLARE_LOG_CATEGORY_EXTERN(LogAssetRegUtil, Log, All);

class IAssetRegistry;
struct FSortableDependencyEntry;

UCLASS(config=Editor)
class UAssetRegUtilCommandlet
	: public UCommandlet
{
	GENERATED_UCLASS_BODY()
public:

	//~ Begin UCommandlet Interface

	virtual int32 Main(const FString& CmdLineParams) override;
	
	//~ End UCommandlet Interface

protected:

	IAssetRegistry* AssetRegistry;

	void RecursivelyGrabDependencies(TArray<FSortableDependencyEntry>& OutSortableDependencies,
		const int32& DepSet, int32& DepOrder, int32 DepHierarchy, TSet<FName>& ProcessedFiles, const TSet<FName>& OriginalSet, const FName& FilePath, const FName& PackageFName, const TArray<FName>& FilterByClass);

	void ReorderOrderFile(const FString& OrderFilePath, const FString& ReorderFileOutPath);
};
