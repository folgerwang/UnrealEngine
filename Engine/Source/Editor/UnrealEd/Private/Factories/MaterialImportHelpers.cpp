// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Factories/MaterialImportHelpers.h"
#include "AssetRegistryModule.h"
#include "AssetData.h"
#include "ARFilter.h"
#include "Materials/MaterialInterface.h"
#include "Misc/Paths.h"

UMaterialInterface* UMaterialImportHelpers::FindExistingMaterialFromSearchLocation(const FString& MaterialFullName, const FString& BasePackagePath, EMaterialSearchLocation SearchLocation, FText& OutError)
{
	//Search in memory
	UMaterialInterface* FoundMaterial = LoadObject<UMaterialInterface>(nullptr, *MaterialFullName, nullptr, LOAD_Quiet | LOAD_NoWarn);
	
	if (FoundMaterial == nullptr)
	{
		FString SearchPath = FPaths::GetPath(BasePackagePath);
		
		// Search in asset's local folder
		FoundMaterial = FindExistingMaterial(SearchPath, MaterialFullName, false, OutError);
		
		// Search recursively in asset's folder
		if (FoundMaterial == nullptr &&
			(SearchLocation != EMaterialSearchLocation::Local))
		{
			FoundMaterial = FindExistingMaterial(SearchPath, MaterialFullName, true, OutError);
		}

		if (FoundMaterial == nullptr &&
			(	SearchLocation == EMaterialSearchLocation::UnderParent ||
				SearchLocation == EMaterialSearchLocation::UnderRoot ||
				SearchLocation == EMaterialSearchLocation::AllAssets))
		{
			// Search recursively in parent's folder
			SearchPath = FPaths::GetPath(SearchPath);

			FoundMaterial = FindExistingMaterial(SearchPath, MaterialFullName, true, OutError);
		}
		if (FoundMaterial == nullptr &&
			(	SearchLocation == EMaterialSearchLocation::UnderRoot ||
				SearchLocation == EMaterialSearchLocation::AllAssets))
		{
			// Search recursively in root folder of asset
			FString OutPackageRoot, OutPackagePath, OutPackageName;
			FPackageName::SplitLongPackageName(SearchPath, OutPackageRoot, OutPackagePath, OutPackageName);

			FoundMaterial = FindExistingMaterial(OutPackageRoot, MaterialFullName, true, OutError);
		}
		if (FoundMaterial == nullptr &&
			SearchLocation == EMaterialSearchLocation::AllAssets)
		{
			// Search everywhere
			FoundMaterial = FindExistingMaterial(TEXT("/"), MaterialFullName, true, OutError);
		}
	}

	return FoundMaterial;
}

UMaterialInterface* UMaterialImportHelpers::FindExistingMaterial(const FString& BasePath, const FString& MaterialFullName, const bool bRecursivePaths, FText& OutError)
{
	UMaterialInterface* Material = nullptr;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	TArray<FAssetData> AssetData;
	FARFilter Filter;

	AssetRegistry.SearchAllAssets(true);

	Filter.bRecursiveClasses = true;
	Filter.bRecursivePaths = bRecursivePaths;
	Filter.ClassNames.Add(UMaterialInterface::StaticClass()->GetFName());
	Filter.PackagePaths.Add(FName(*BasePath));

	AssetRegistry.GetAssets(Filter, AssetData);

	TArray<UMaterialInterface*> FoundMaterials;
	for (const FAssetData& Data : AssetData)
	{
		if (Data.AssetName == FName(*MaterialFullName))
		{
			Material = Cast<UMaterialInterface>(Data.GetAsset());
			if (Material != nullptr)
			{
				FoundMaterials.Add(Material);
			}
		}
	}

	if (FoundMaterials.Num() > 1)
	{
		check(Material != nullptr);
		OutError =
			FText::Format(NSLOCTEXT("MaterialImportHelpers", "MultipleMaterialsFound", "Found {0} materials matching name '{1}'. Using '{2}'."),
				FText::FromString(FString::FromInt(FoundMaterials.Num())),
				FText::FromString(MaterialFullName),
				FText::FromString(Material->GetOutermost()->GetName()));
	}
	return Material;
}
