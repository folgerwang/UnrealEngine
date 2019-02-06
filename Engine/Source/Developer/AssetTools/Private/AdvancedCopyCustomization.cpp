// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AdvancedCopyCustomization.h"
#include "Containers/UnrealString.h"
#include "AssetRegistryModule.h"


#define LOCTEXT_NAMESPACE "AdvancedCopyCustomization"


UAdvancedCopyCustomization::UAdvancedCopyCustomization(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bShouldGenerateRelativePaths(true)
{
	FilterForExcludingDependencies.PackagePaths.Add(TEXT("/Engine"));
	FilterForExcludingDependencies.PackagePaths.Add(TEXT("/Script"));
	FilterForExcludingDependencies.bRecursivePaths = true;
	FilterForExcludingDependencies.bRecursiveClasses = true;
}

void UAdvancedCopyCustomization::SetPackageThatInitiatedCopy(const FString& InBasePackage)
{
	FString TempPackage = InBasePackage;

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> DependencyAssetData;
	IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
	AssetRegistry.GetAssetsByPackageName(FName(*InBasePackage), DependencyAssetData);
	// We found a folder
	if (DependencyAssetData.Num() == 0)
	{
		// Take off the name of the folder we copied so copied files are still nested
		TempPackage.Split(TEXT("/"), &TempPackage, nullptr, ESearchCase::IgnoreCase, ESearchDir::FromEnd);
	}
	
	if (!TempPackage.EndsWith(TEXT("/")))
	{
		TempPackage += TEXT("/");
	}
	PackageThatInitiatedCopy = TempPackage;
}

#undef LOCTEXT_NAMESPACE
