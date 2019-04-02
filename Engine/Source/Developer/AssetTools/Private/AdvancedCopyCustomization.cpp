// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "AdvancedCopyCustomization.h"
#include "Containers/UnrealString.h"
#include "AssetRegistryModule.h"
#include "Interfaces/IPluginManager.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/MapBuildDataRegistry.h"


#define LOCTEXT_NAMESPACE "AdvancedCopyCustomization"


UAdvancedCopyCustomization::UAdvancedCopyCustomization(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, bShouldGenerateRelativePaths(true)
{
	FilterForExcludingDependencies.PackagePaths.Add(TEXT("/Engine"));
	for (TSharedRef<IPlugin>& Plugin : IPluginManager::Get().GetDiscoveredPlugins())
	{
		if (Plugin->GetType() != EPluginType::Project)
		{
			FilterForExcludingDependencies.PackagePaths.Add(FName(*("/" + Plugin->GetName())));
		}
	}

	FilterForExcludingDependencies.bRecursivePaths = true;
	FilterForExcludingDependencies.bRecursiveClasses = true;
	FilterForExcludingDependencies.ClassNames.Add(UWorld::StaticClass()->GetFName());
	FilterForExcludingDependencies.ClassNames.Add(ULevel::StaticClass()->GetFName());
	FilterForExcludingDependencies.ClassNames.Add(UMapBuildDataRegistry::StaticClass()->GetFName());

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
