// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditorAssetLibrary.h"

#include "EditorScriptingUtils.h"

#include "AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Algo/Count.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "Engine/Blueprint.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "ISourceControlModule.h"
#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "ObjectTools.h"
#include "UObject/MetaData.h"

#define LOCTEXT_NAMESPACE "Editoribrary"

namespace InternalEditorLevelLibrary
{
	bool ListAssetDatas(const FString& AnyAssetPathOrAnyDirectoryPath, bool bRecursive, bool& bOutIsFolder, TArray<FAssetData>& OutResult, FString& OutValidDirectoryPath, FString& OutFailureReason);
	bool ListAssetDatasForDirectory(const FString& AnyDirectoryPath, bool bRecursive, TArray<FAssetData>& OutResult, FString& OutValidDirectoryPath, FString& OutFailureReason);

	bool IsAssetRegistryModuleLoading()
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		if (AssetRegistryModule.Get().IsLoadingAssets())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("The AssetRegistry is currently loading."));
			return false;
		}
		return true;
	}

	UObject* LoadAsset(const FString& AssetPath, bool bAllowMapAsset, FString& OutFailureReason)
	{
		FAssetData AssetData = EditorScriptingUtils::FindAssetDataFromAnyPath(AssetPath, OutFailureReason);
		if (!AssetData.IsValid())
		{
			return nullptr;
		}
		return EditorScriptingUtils::LoadAsset(AssetData, bAllowMapAsset, OutFailureReason);
	}

	bool GetContentBrowserPackagesForDirectory(const FString& AnyDirectoryPath, bool bOnlyIfIsDirty, bool bRecursive, TArray<UPackage*>& OutResult, FString& OutFailureReason)
	{
		FString ValidDirectoryPath;
		TArray<FAssetData> AssetDatas;
		if (!ListAssetDatasForDirectory(AnyDirectoryPath, bRecursive, AssetDatas, ValidDirectoryPath, OutFailureReason))
		{
			return false;
		}

		if (bOnlyIfIsDirty)
		{
			for (const FAssetData& AssetData : AssetDatas)
			{
				// Can't be dirty is not loaded
				if (AssetData.IsAssetLoaded())
				{
					UPackage* Package = AssetData.GetPackage();
					if (Package && Package->IsDirty())
					{
						Package->FullyLoad();
						OutResult.AddUnique(Package);
					}
				}
			}
		}
		else
		{
			// load all assets
			for (const FAssetData& AssetData : AssetDatas)
			{
				UPackage* Package = AssetData.GetPackage();
				if (Package)
				{
					Package->FullyLoad();
					OutResult.AddUnique(Package);
				}
			}
		}

		return true;
	}

	// Valid inputs: "/Game/MyFolder/", "/Game/MyFolder", "/Game/", "/Game"
	bool ListAssetDatasForDirectory(const FString& AnyPathDirectoryPath, bool bRecursive, TArray<FAssetData>& OutResult, FString& OutValidDirectoryPath, FString& OutFailureReason)
	{
		OutResult.Reset();
		OutValidDirectoryPath.Reset();

		OutValidDirectoryPath = EditorScriptingUtils::ConvertAnyPathToLongPackagePath(AnyPathDirectoryPath, OutFailureReason);
		if (OutValidDirectoryPath.IsEmpty())
		{
			return false;
		}

		TArray<FAssetData> MapAssetDatas;
		return EditorScriptingUtils::GetAssetsInPath(OutValidDirectoryPath, bRecursive, OutResult, MapAssetDatas, OutFailureReason);
	}

	// Valid inputs: "/Game/MyFolder/MyAsset.MyAsset", "/Game/MyFolder/MyAsset", "/Game/MyFolder/", "/Game/MyFolder", "/Game/", "/Game"
	bool ListAssetDatas(const FString& AnyAssetPathOrAnyDirectoryPath, bool bRecursive, bool& bOutIsFolder, TArray<FAssetData>& OutResult, FString& OutValidDirectoryPath, FString& OutFailureReason)
	{
		OutResult.Reset();
		OutValidDirectoryPath.Reset();
		bOutIsFolder = false;

		// Ask the AssetRegistry if it's a file
		FAssetData AssetData = EditorScriptingUtils::FindAssetDataFromAnyPath(AnyAssetPathOrAnyDirectoryPath, OutFailureReason);
		if (AssetData.IsValid())
		{
			if (EditorScriptingUtils::IsPackageFlagsSupportedForAssetLibrary(AssetData.PackageFlags))
			{
				OutResult.Add(AssetData);
			}
		}
		else
		{
			bOutIsFolder = true;
			return ListAssetDatasForDirectory(AnyAssetPathOrAnyDirectoryPath, bRecursive, OutResult, OutValidDirectoryPath, OutFailureReason);
		}

		return true;
	}

	struct FValidatedPaths
	{
		FString SourceValidDirectoryPath;
		FString SourceFilePath;
		FString DestinationValidDirectoryPath;
		FString DestinationFilePath;
	};
	struct FValidatedObjectInfos
	{
		TArray<UObject*> PreviousLoadedAssets;
		TArray<FString> NewAssetsDirectoryPath;
		void Reset()
		{
			PreviousLoadedAssets.Reset();
			NewAssetsDirectoryPath.Reset();
		}
	};
	bool ValidateSourceAndDestinationForOperation(const FString& SourceDirectoryPath, const FString& DestinationDirectoryPath, bool bValidIfOnlyAllAssetCanBeOperatedOn, const TCHAR* CommandName, FValidatedPaths& OutValidatedPaths, FValidatedObjectInfos& OutObjectInfos)
	{
		// Test the path to see if they are valid
		FString FailureReason;
		OutValidatedPaths.SourceValidDirectoryPath = EditorScriptingUtils::ConvertAnyPathToLongPackagePath(SourceDirectoryPath, FailureReason);
		if (OutValidatedPaths.SourceValidDirectoryPath.IsEmpty())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to convert the source path. %s"), CommandName, *FailureReason);
			return false;
		}

		OutValidatedPaths.SourceFilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(OutValidatedPaths.SourceValidDirectoryPath));
		if (OutValidatedPaths.SourceFilePath.IsEmpty())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to convert the source path '%s' to a full path. Was it rooted?"), CommandName, *OutValidatedPaths.SourceValidDirectoryPath);
			return false;
		}

		OutValidatedPaths.DestinationValidDirectoryPath = EditorScriptingUtils::ConvertAnyPathToLongPackagePath(DestinationDirectoryPath, FailureReason);
		if (OutValidatedPaths.DestinationValidDirectoryPath.IsEmpty())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to convert the destination path. %s"), CommandName, *FailureReason);
			return false;
		}

		OutValidatedPaths.DestinationFilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(OutValidatedPaths.DestinationValidDirectoryPath));
		if (OutValidatedPaths.DestinationFilePath.IsEmpty())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to convert the destination path '%s' to a full path. Was it rooted?"), CommandName, *OutValidatedPaths.DestinationFilePath);
			return false;
		}

		// If the directory doesn't exist on drive then can't rename/duplicate it
		if (!IFileManager::Get().DirectoryExists(*OutValidatedPaths.SourceFilePath))
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. The source directory do not exist."), CommandName);
			return false;
		}

		// Create the distination directory if it doesn't already exist
		if (!IFileManager::Get().DirectoryExists(*OutValidatedPaths.DestinationFilePath))
		{
			const bool bTree = true;
			if (!IFileManager::Get().MakeDirectory(*OutValidatedPaths.DestinationFilePath, bTree))
			{
				UE_LOG(LogEditorScripting, Error, TEXT("%s. The destination directory could not be created."), CommandName);
				return false;
			}
		}

		// Select all the asset the folder contains
		// Because we want to rename a folder, we can't rename any files that can't be deleted
		TArray<FAssetData> CouldNotLoadAssetData;
		TArray<FString> FailureReasons;
		bool bFailedToGetLoadedAssets = !EditorScriptingUtils::GetAssetsInPath(OutValidatedPaths.SourceValidDirectoryPath, true, OutObjectInfos.PreviousLoadedAssets, CouldNotLoadAssetData, FailureReasons);
		if (bFailedToGetLoadedAssets && bValidIfOnlyAllAssetCanBeOperatedOn)
		{
			bFailedToGetLoadedAssets = CouldNotLoadAssetData.Num() > 0;
		}
		if (bFailedToGetLoadedAssets)
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to validate all assets."), CommandName);
			for (const FString& Reason : FailureReasons)
			{
				UE_LOG(LogEditorScripting, Error, TEXT("  %s"), *Reason);
			}
			return false;
		}

		// Test to see if the destination will be valid
		if (OutObjectInfos.PreviousLoadedAssets.Num())
		{
			for (UObject* Object : OutObjectInfos.PreviousLoadedAssets)
			{
				FString ObjectPackageName = Object->GetOutermost()->GetName();
				FString ObjectLongPackagePath = FPackageName::GetLongPackagePath(ObjectPackageName);
				FString ObjectName = FPackageName::ObjectPathToObjectName(ObjectPackageName);

				// Remove source from the object name
				ObjectLongPackagePath = ObjectLongPackagePath.Mid(OutValidatedPaths.SourceValidDirectoryPath.Len());

				// Create AssetPath /Game/MyFolder/MyAsset.MyAsset
				FString NewAssetPackageName;
				if (ObjectLongPackagePath.IsEmpty())
				{
					NewAssetPackageName = FString::Printf(TEXT("%s/%s.%s"), *OutValidatedPaths.DestinationValidDirectoryPath, *Object->GetName(), *Object->GetName());
				}
				else
				{
					NewAssetPackageName = FString::Printf(TEXT("%s%s/%s.%s"), *OutValidatedPaths.DestinationValidDirectoryPath, *ObjectLongPackagePath, *Object->GetName(), *Object->GetName());
				}

				if (!EditorScriptingUtils::IsAValidPathForCreateNewAsset(NewAssetPackageName, FailureReason))
				{
					UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to validate the destination for asset '%s'. %s"), CommandName, *Object->GetName(), *FailureReason);
					OutObjectInfos.Reset();
					return false;
				}

				// Rename should do it, but will suggest another location via a Modal
				if (FPackageName::DoesPackageExist(NewAssetPackageName, nullptr, nullptr))
				{
					UE_LOG(LogEditorScripting, Error, TEXT("%s. Failed to validate the destination for asset '%s'. There's alreay an asset at the destination."), CommandName, *NewAssetPackageName);
					OutObjectInfos.Reset();
					return false;
				}

				// Keep AssetPath /Game/MyFolder
				OutObjectInfos.NewAssetsDirectoryPath.Add(FPackageName::GetLongPackagePath(NewAssetPackageName));
			}
		}

		return true;
	}
}

/**
 *
 * Load operations
 *
 **/

// A wrapper around
//unreal.AssetRegistryHelpers.get_asset(unreal.AssetRegistryHelpers.get_asset_registry().get_asset_by_object_path("/Game/NewDataTable.NewDataTable"))
UObject* UEditorAssetLibrary::LoadAsset(const FString& AssetPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return nullptr;
	}

	FString FailureReason;
	UObject* Result = InternalEditorLevelLibrary::LoadAsset(AssetPath, false, FailureReason);
	if (Result == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("LoadAsset. Failed to load asset: %s"), *FailureReason);
	}
	return Result;
}

UClass* UEditorAssetLibrary::LoadBlueprintClass(const FString& AssetPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return nullptr;
	}

	FString FailureReason;
	UObject* LoadedAsset = InternalEditorLevelLibrary::LoadAsset(AssetPath, false, FailureReason);
	if (LoadedAsset == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("LoadBlueprintClass. Failed to load asset: %s"), *FailureReason);
		return nullptr;
	}

	UBlueprint* Blueprint = Cast<UBlueprint>(LoadedAsset);
	if (Blueprint == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("LoadBlueprintClass. The asset is not a Blueprint."));
		return nullptr;
	}
	return Blueprint->GeneratedClass.Get();
}

FString UEditorAssetLibrary::GetPathNameForLoadedAsset(UObject* LoadedAsset)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return FString();
	}

	FString FailureReason;
	if (!EditorScriptingUtils::IsAContentBrowserAsset(LoadedAsset, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("GetLoadedAssetPath. %s"), *FailureReason);
	}
	return LoadedAsset->GetPathName();
}

FAssetData UEditorAssetLibrary::FindAssetData(const FString& AssetPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	FAssetData Result;
	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return Result;
	}

	FString FailureReason;
	Result = EditorScriptingUtils::FindAssetDataFromAnyPath(AssetPath, FailureReason);
	if (!Result.IsValid())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("FindAssetData. Failed to find the AssetPath. %s"), *FailureReason);
	}
	return Result;
}

bool UEditorAssetLibrary::DoesAssetExist(const FString& AssetPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	FString ObjectPath = EditorScriptingUtils::ConvertAnyPathToObjectPath(AssetPath, FailureReason);
	if (ObjectPath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DoesAssetExists. %s"), *FailureReason);
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*ObjectPath);
	if (!AssetData.IsValid())
	{
		return false;
	}

	if (!EditorScriptingUtils::IsPackageFlagsSupportedForAssetLibrary(AssetData.PackageFlags))
	{
		UE_LOG(LogEditorScripting, Warning, TEXT("DoesAssetExists. The AssetData '%s' exist but is not accessible because it is of type Map/Level."), *ObjectPath);
	}
	return true;
}

bool UEditorAssetLibrary::DoAssetsExist(const TArray<FString>& AssetPaths)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	for (const FString& Path : AssetPaths)
	{
		FString ObjectPath = EditorScriptingUtils::ConvertAnyPathToObjectPath(Path, FailureReason);
		if (ObjectPath.IsEmpty())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("DoesAssetExists. %s"), *FailureReason);
			return false;
		}

		FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*ObjectPath);
		if (!AssetData.IsValid())
		{
			return false;
		}

		if (!EditorScriptingUtils::IsPackageFlagsSupportedForAssetLibrary(AssetData.PackageFlags))
		{
			UE_LOG(LogEditorScripting, Warning, TEXT("DoesAssetExists. The AssetData '%s' exists but is not accessible because it is of type Map/Level."), *ObjectPath);
		}
	}
	return true;
}

TArray<FString> UEditorAssetLibrary::FindPackageReferencersForAsset(const FString& AnyAssetPath, bool bLoadAssetsToConfirm)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TArray<FString> Result;
	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return Result;
	}

	FString FailureReason;
	FString AssetPath = EditorScriptingUtils::ConvertAnyPathToObjectPath(AnyAssetPath, FailureReason);
	if (AssetPath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("FindAssetPackageReferencers. %s"), *FailureReason);
		return Result;
	}

	// Find the reference in packages. Load them to confirm the reference.
	TArray<FName> PackageReferencers;
	{
		EAssetRegistryDependencyType::Type ReferenceType = EAssetRegistryDependencyType::Packages;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		AssetRegistryModule.Get().GetReferencers(*FPackageName::ObjectPathToPackageName(AssetPath), PackageReferencers, ReferenceType);
	}

	if (bLoadAssetsToConfirm)
	{
		// Load the asset to confirm 
		UObject* LoadedAsset = InternalEditorLevelLibrary::LoadAsset(AssetPath, false, FailureReason);
		if (LoadedAsset == nullptr)
		{
			UE_LOG(LogEditorScripting, Error, TEXT("FindAssetPackageReferencers. Failed to load asset: %s"), *FailureReason);
			return Result;
		}

		// Load the asset referencers to confirm 
		for (FName Referencer : PackageReferencers)
		{
			UObject* ReferencerAsset = InternalEditorLevelLibrary::LoadAsset(Referencer.ToString(), false, FailureReason);
			if (ReferencerAsset == nullptr)
			{
				UE_LOG(LogEditorScripting, Warning, TEXT("FindAssetPackageReferencers. Not able to confirm: %s"), *FailureReason);
				// Add it to the list anyway
				Result.AddUnique(Referencer.ToString());
			}
		}

		// Check what we have in memory (but not in undo memroy)
		FReferencerInformationList MemoryReferences;
		{
			if (GEditor && GEditor->Trans)
			{
				GEditor->Trans->DisableObjectSerialization();
			}
			IsReferenced(LoadedAsset, GARBAGE_COLLECTION_KEEPFLAGS, EInternalObjectFlags::GarbageCollectionKeepFlags, true, &MemoryReferences);
			if (GEditor && GEditor->Trans)
			{
				GEditor->Trans->EnableObjectSerialization();
			}
		}

		// Check if any references are in the list. Only confirm if the package is referencing it. An inner object of the asset could reference it.
		TArray<FName> ConfirmedReferencers;
		ConfirmedReferencers.Reserve(PackageReferencers.Num());

		for (const FReferencerInformation& RefInfo : MemoryReferences.InternalReferences)
		{
			FName PackageName = RefInfo.Referencer->GetOutermost()->GetFName();
			if (PackageReferencers.Contains(PackageName))
			{
				ConfirmedReferencers.AddUnique(PackageName);
			}
		}
		for (const FReferencerInformation& RefInfo : MemoryReferences.ExternalReferences)
		{
			FName PackageName = RefInfo.Referencer->GetOutermost()->GetFName();
			if (PackageReferencers.Contains(PackageName))
			{
				ConfirmedReferencers.AddUnique(PackageName);
			}
		}

		// Copy the confirm referencers list
		PackageReferencers.Empty();
		PackageReferencers = MoveTemp(ConfirmedReferencers);
	}

	// Copy the list. Result may already have Map packages.
	Result.Reserve(PackageReferencers.Num());
	for (FName PackagePath : PackageReferencers)
	{
		Result.Add(PackagePath.ToString());
	}

	return Result;
}

bool UEditorAssetLibrary::ConsolidateAssets(UObject* AssetToConsolidateTo, const TArray<UObject*>& AssetsToConsolidate)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	if (!EditorScriptingUtils::IsAContentBrowserAsset(AssetToConsolidateTo, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("ConsolidateAssets. Failed to validate the AssetToConsolidateTo. %s"), *FailureReason);
		return false;
	}
	if (AssetsToConsolidate.Num() == 0)
	{
		UE_LOG(LogEditorScripting, Warning, TEXT("ConsolidateAssets. There is no object to consolidate."));
		return false;
	}

	TArray<UObject*> ToConsolidationObjects;
	ToConsolidationObjects.Reserve(AssetsToConsolidate.Num());
	for (UObject* Object : AssetsToConsolidate)
	{
		if (!EditorScriptingUtils::IsAContentBrowserAsset(Object, FailureReason))
		{
			UE_LOG(LogEditorScripting, Error, TEXT("ConsolidateAssets. Failed to validate the object '%s'. %s"), *Object->GetName(), *FailureReason);
			return false;
		}
		if (AssetToConsolidateTo->GetClass() != Object->GetClass())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("ConsolidateAssets. The object '%s' doesn't have the same class as the AssetToConsolidateTo."), *Object->GetName());
			return false;
		}
		ToConsolidationObjects.Add(Object);
	}

	// Perform the object consolidation
	bool bShowDeleteConfirmation = false;
	ObjectTools::FConsolidationResults ConsResults = ObjectTools::ConsolidateObjects(AssetToConsolidateTo, ToConsolidationObjects, bShowDeleteConfirmation);

	// If the consolidation went off successfully with no failed objects
	if (ConsResults.DirtiedPackages.Num() > 0 && ConsResults.FailedConsolidationObjs.Num() == 0)
	{
		bool bOnlyIfIsDirty = false;
		UEditorLoadingAndSavingUtils::SavePackages(ConsResults.DirtiedPackages, bOnlyIfIsDirty);
	}
	// If the consolidation resulted in failed (partially consolidated) objects, do not save, and inform the user no save attempt was made
	else if (ConsResults.FailedConsolidationObjs.Num() > 0)
	{
		UE_LOG(LogEditorScripting, Warning, TEXT("ConsolidateAssets. Not all objects could be consolidated, no save has occurred"));
		return false;
	}

	return true;
}

/**
 *
 * Delete operations
 *
 **/

bool UEditorAssetLibrary::DeleteLoadedAsset(UObject* AssetToDelete)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	if (!EditorScriptingUtils::IsAContentBrowserAsset(AssetToDelete, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DeleteLoadedAsset. Failed to validate the source. %s"), *FailureReason);
		return false;
	}

	TArray<UObject*> AssetsToDelete;
	AssetsToDelete.Add(AssetToDelete);
	const bool bShowConfirmation = false;
	return ObjectTools::ForceDeleteObjects(AssetsToDelete, bShowConfirmation) == AssetsToDelete.Num();
}

bool UEditorAssetLibrary::DeleteLoadedAssets(const TArray<UObject*>& AssetsToDelete)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	for (UObject* Asset : AssetsToDelete)
	{
		if (!EditorScriptingUtils::IsAContentBrowserAsset(Asset, FailureReason))
		{
			UE_LOG(LogEditorScripting, Error, TEXT("DeleteLoadedAsset. Failed to validate the source. %s"), *FailureReason);
			return false;
		}
	}

	// make sure they are all from the content browser
	const bool bShowConfirmation = false;
	return ObjectTools::ForceDeleteObjects(AssetsToDelete, bShowConfirmation) == AssetsToDelete.Num();
}

bool UEditorAssetLibrary::DeleteAsset(const FString& AssetPathToDelete)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	// Load the asset and make sure it's a valid asset to work with
	FString FailureReason;
	UObject* AssetToDelete = InternalEditorLevelLibrary::LoadAsset(AssetPathToDelete, true, FailureReason);
	if (AssetToDelete == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DeleteAsset. Failed to find the source asset. %s"), *FailureReason);
		return false;
	}

	TArray<UObject*> AssetsToDelete;
	AssetsToDelete.Add(AssetToDelete);
	const bool bShowConfirmation = false;
	return ObjectTools::ForceDeleteObjects(AssetsToDelete, bShowConfirmation) == AssetsToDelete.Num();
}

bool UEditorAssetLibrary::DeleteDirectory(const FString& DirectoryPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	FString ValidDirectoryPath = EditorScriptingUtils::ConvertAnyPathToLongPackagePath(DirectoryPath, FailureReason);
	if (ValidDirectoryPath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DeleteDirectory. Failed to convert the path. %s"), *FailureReason);
		return false;
	}

	FString FilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(ValidDirectoryPath + TEXT("/")));
	if (FilePath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DeleteDirectory. Failed to convert the path '%s' to a full path. Was it rooted?"), *ValidDirectoryPath);
		return false;
	}

	// Ask the AssetRegistry if it's a folder
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

	TArray<FAssetData> AssetDatas;
	TArray<FAssetData> CouldNotLoadAssetDatas;
	if (!EditorScriptingUtils::GetAssetsInPath(ValidDirectoryPath, true, AssetDatas, CouldNotLoadAssetDatas, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DeleteDirectory. The internal search input were not valid."));
		return false;
	}

	AssetDatas.Append(CouldNotLoadAssetDatas);

	// Load all assets including MAP and Build
	TArray<UObject*> LoadedAssets;
	LoadedAssets.Reserve(AssetDatas.Num());
	for (const FAssetData& AssetData : AssetDatas)
	{
		bool bAllowMapAsset = true;
		FString LoadFailureReason;
		UObject* LoadedObject = EditorScriptingUtils::LoadAsset(AssetData, bAllowMapAsset, LoadFailureReason);
		if (LoadedObject)
		{
			LoadedAssets.Add(LoadedObject);
		}
		else
		{
			UE_LOG(LogEditorScripting, Error, TEXT("DeleteDirectory. Failed to delete the directory. Some Asset couldn't be loaded. %s"), *LoadFailureReason);
			return false;
		}
	}

	bool bShowConfirmation = false;
	if (ObjectTools::ForceDeleteObjects(LoadedAssets, bShowConfirmation) != LoadedAssets.Num())
	{
		UE_LOG(LogEditorScripting, Warning, TEXT("DeleteDirectory. Not all asset were deleted."));
		return false;
	}

	// Remove the path from the Content Browser
	if (!AssetRegistryModule.Get().RemovePath(ValidDirectoryPath))
	{
		UE_LOG(LogEditorScripting, Warning, TEXT("DeleteDirectory. The folder couldn't be removed from the Content Browser."));
	}

	// Delete the directory from the drive
	if (!EditorScriptingUtils::DeleteEmptyDirectoryFromDisk(ValidDirectoryPath))
	{
		UE_LOG(LogEditorScripting, Warning, TEXT("DeleteDirectory. Failed to remove the folder but the assets have been removed."));
		return false;
	}

	return true;
}

/**
 *
 * Duplicate operations
 *
 **/

namespace InternalEditorLevelLibrary
{
	UObject* DuplicateAsset(UObject* SourceObject, const FString& DestinationAssetPath)
	{
		check(SourceObject);
		if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
		{
			return nullptr;
		}

		FString FailureReason;
		FString DestinationObjectPath = EditorScriptingUtils::ConvertAnyPathToObjectPath(DestinationAssetPath, FailureReason);
		if (DestinationObjectPath.IsEmpty())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("DuplicateAsset. Failed to validate the destination. %s"), *FailureReason);
			return nullptr;
		}

		if (!EditorScriptingUtils::IsAValidPathForCreateNewAsset(DestinationObjectPath, FailureReason))
		{
			UE_LOG(LogEditorScripting, Error, TEXT("DuplicateAsset. Failed to validate the destination. %s"), *FailureReason);
			return nullptr;
		}

		// DuplicateAsset does it, but failed with a Modal
		if (FPackageName::DoesPackageExist(DestinationObjectPath, nullptr, nullptr))
		{
			UE_LOG(LogEditorScripting, Error, TEXT("DuplicateAsset. Failed to validate the destination '%s'. There's alreay an asset at the destination."), *DestinationObjectPath);
			return nullptr;
		}

		FString DestinationLongPackagePath = FPackageName::GetLongPackagePath(DestinationObjectPath);
		FString DestinationObjectName = FPackageName::ObjectPathToObjectName(DestinationObjectPath);

		// duplicate asset
		FAssetToolsModule& Module = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		UObject* DuplicatedAsset = Module.Get().DuplicateAsset(DestinationObjectName, DestinationLongPackagePath, SourceObject);

		return DuplicatedAsset;
	}
}

UObject* UEditorAssetLibrary::DuplicateLoadedAsset(UObject* SourceAsset, const FString& DestinationAssetPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	FString FailureReason;

	// Make sure the asset is from the ContentBrowser
	if (!EditorScriptingUtils::IsAContentBrowserAsset(SourceAsset, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DuplicateAsset. Failed to validate the source. %s"), *FailureReason);
		return nullptr;
	}

	return InternalEditorLevelLibrary::DuplicateAsset(SourceAsset, DestinationAssetPath);
}

UObject* UEditorAssetLibrary::DuplicateAsset(const FString& SourceAssetPath, const FString& DestinationAssetPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	FString FailureReason;

	// Load the asset and make sure it's a valid asset to work with
	UObject* SourceObject = InternalEditorLevelLibrary::LoadAsset(SourceAssetPath, false, FailureReason);
	if (SourceObject == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DuplicateAsset. Failed to find the source asset. %s"), *FailureReason);
		return nullptr;
	}

	return InternalEditorLevelLibrary::DuplicateAsset(SourceObject, DestinationAssetPath);
}

bool UEditorAssetLibrary::DuplicateDirectory(const FString& SourceDirectoryPath, const FString& DestinationDirectoryPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	const bool bNoFailureWithGetAssetInPaths = false;
	InternalEditorLevelLibrary::FValidatedPaths ValidatedPaths;
	InternalEditorLevelLibrary::FValidatedObjectInfos ValidatedObjectInfos;
	if (!InternalEditorLevelLibrary::ValidateSourceAndDestinationForOperation(SourceDirectoryPath, DestinationDirectoryPath, bNoFailureWithGetAssetInPaths, TEXT("DuplicateDirectory"), ValidatedPaths, ValidatedObjectInfos))
	{
		return false;
	}

	// Prepare data
	bool bHaveAFailedAsset = false;
	if (ValidatedObjectInfos.PreviousLoadedAssets.Num() > 0)
	{
		FAssetToolsModule& Module = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

		for (int32 Index = 0; Index < ValidatedObjectInfos.PreviousLoadedAssets.Num(); ++Index)
		{
			UObject* DuplicatedAsset = Module.Get().DuplicateAsset(ValidatedObjectInfos.PreviousLoadedAssets[Index]->GetName(), ValidatedObjectInfos.NewAssetsDirectoryPath[Index], ValidatedObjectInfos.PreviousLoadedAssets[Index]);
			if (DuplicatedAsset == nullptr)
			{
				UE_LOG(LogEditorScripting, Warning, TEXT("DuplicateDirectory. Failed to duplicate object '%s'"), *ValidatedObjectInfos.PreviousLoadedAssets[Index]->GetPathName());
				bHaveAFailedAsset = true;
			}
		}
	}
	else
	{
		UE_LOG(LogEditorScripting, Log, TEXT("DuplicateDirectory. No asset to duplicate."));
	}

	// Make sure the ContentBrowser knows about the new directory
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().AddPath(ValidatedPaths.DestinationValidDirectoryPath);

	return !bHaveAFailedAsset;
}

/**
 *
 * Rename operations
 *
 **/

namespace InternalEditorLevelLibrary
{
	bool RenameAsset(UObject* SourceObject, const FString& DestinationAssetPath)
	{
		check(SourceObject);
		if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
		{
			return false;
		}

		FString FailureReason;
		FString DestinationObjectPath = EditorScriptingUtils::ConvertAnyPathToObjectPath(DestinationAssetPath, FailureReason);
		if (DestinationObjectPath.IsEmpty())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("RenameAsset. Failed to validate the destination. %s"), *FailureReason);
			return false;
		}

		if (!EditorScriptingUtils::IsAValidPathForCreateNewAsset(DestinationObjectPath, FailureReason))
		{
			UE_LOG(LogEditorScripting, Error, TEXT("RenameAsset. Failed to validate the destination. %s"), *FailureReason);
			return false;
		}

		// Rename should do it, but will suggest another location via a Modal
		if (FPackageName::DoesPackageExist(DestinationObjectPath, nullptr, nullptr))
		{
			UE_LOG(LogEditorScripting, Error, TEXT("RenameAsset. Failed to validate the destination %s. There's alreay an asset at the destination."), *DestinationAssetPath);
			return false;
		}

		FString DestinationLongPackagePath = FPackageName::GetLongPackagePath(DestinationObjectPath);
		FString DestinationObjectName = FPackageName::ObjectPathToObjectName(DestinationObjectPath);

		// rename asset
		TArray<FAssetRenameData> AssetToRename;
		AssetToRename.Add(FAssetRenameData(SourceObject, DestinationLongPackagePath, DestinationObjectName));

		FAssetToolsModule& Module = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		return Module.Get().RenameAssets(AssetToRename);
	}
}

bool UEditorAssetLibrary::RenameLoadedAsset(UObject* SourceAsset, const FString& DestinationAssetPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	// Make sure the asset is from the ContentBrowser
	FString FailureReason;
	if (!EditorScriptingUtils::IsAContentBrowserAsset(SourceAsset, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("RenameAsset. Failed to validate the source. %s"), *FailureReason);
		return false;
	}

	return InternalEditorLevelLibrary::RenameAsset(SourceAsset, DestinationAssetPath);
}

bool UEditorAssetLibrary::RenameAsset(const FString& SourceAssetPath, const FString& DestinationAssetPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	// Load the asset and make sure it's a valid asset to work with
	FString FailureReason;
	UObject* SourceObject = InternalEditorLevelLibrary::LoadAsset(SourceAssetPath, false, FailureReason);
	if (SourceObject == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("RenameAsset. Failed to find the source asset. %s"), *FailureReason);
		return false;
	}

	return InternalEditorLevelLibrary::RenameAsset(SourceObject, DestinationAssetPath);
}

bool UEditorAssetLibrary::RenameDirectory(const FString& SourceDirectoryPath, const FString& DestinationDirectoryPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	const bool bNoFailureWithGetAssetInPaths = true;
	InternalEditorLevelLibrary::FValidatedPaths ValidatedPaths;
	InternalEditorLevelLibrary::FValidatedObjectInfos ValidatedObjectInfos;
	if (!InternalEditorLevelLibrary::ValidateSourceAndDestinationForOperation(SourceDirectoryPath, DestinationDirectoryPath, bNoFailureWithGetAssetInPaths, TEXT("RenameDirectory"), ValidatedPaths, ValidatedObjectInfos))
	{
		return false;
	}

	// Prepare data
	if (ValidatedObjectInfos.PreviousLoadedAssets.Num() > 0)
	{
		TArray<FAssetRenameData> AssetsToRename;
		AssetsToRename.Reserve(ValidatedObjectInfos.PreviousLoadedAssets.Num());

		for (int32 Index = 0; Index < ValidatedObjectInfos.PreviousLoadedAssets.Num(); ++Index)
		{
			AssetsToRename.Add(FAssetRenameData(ValidatedObjectInfos.PreviousLoadedAssets[Index], ValidatedObjectInfos.NewAssetsDirectoryPath[Index], ValidatedObjectInfos.PreviousLoadedAssets[Index]->GetName()));
		}

		// Rename the assets
		FAssetToolsModule& Module = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
		if (!Module.Get().RenameAssets(AssetsToRename))
		{
			UE_LOG(LogEditorScripting, Error, TEXT("RemameDirectory. Failed to rename the assets."));
			return false;
		}
	}
	else
	{
		UE_LOG(LogEditorScripting, Log, TEXT("RemameDirectory. No asset to rename."));
	}

	// Delete the old directory
	if (!EditorScriptingUtils::DeleteEmptyDirectoryFromDisk(ValidatedPaths.SourceValidDirectoryPath))
	{
		UE_LOG(LogEditorScripting, Warning, TEXT("RemameDirectory. Failed to rename the folder but the assets have been renamed."));
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	AssetRegistryModule.Get().RemovePath(ValidatedPaths.SourceValidDirectoryPath);
	AssetRegistryModule.Get().AddPath(ValidatedPaths.DestinationValidDirectoryPath);
	return true;
}

/**
 *
 * Checkout operations
 *
 **/

namespace InternalEditorLevelLibrary
{
	bool Checkout(const TArray<UPackage*>& Packages)
	{
		if (Packages.Num() > 0)
		{
			// Checkout without a prompt
			TArray<UPackage*>* PackagesCheckedOut = nullptr;
			const bool bErrorIfAlreadyCheckedOut = false;
			ECommandResult::Type Result = FEditorFileUtils::CheckoutPackages(Packages, PackagesCheckedOut, bErrorIfAlreadyCheckedOut);
			return Result == ECommandResult::Succeeded;
		}
		return true;
	}
}

bool UEditorAssetLibrary::CheckoutLoadedAsset(UObject* AssetToCheckout)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	if (!EditorScriptingUtils::IsAContentBrowserAsset(AssetToCheckout, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("CheckoutLoadedAsset. Failed to validate the source. %s"), *FailureReason);
		return false;
	}

	TArray<UPackage*> Packages;
	Packages.Add(AssetToCheckout->GetOutermost()); // Fully load and check out is done in FEditorFileUtils::CheckoutPackages

	return InternalEditorLevelLibrary::Checkout(Packages);
}

bool UEditorAssetLibrary::CheckoutLoadedAssets(const TArray<UObject*>& AssetsToCheckout)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	TArray<UPackage*> Packages;
	Packages.Reserve(AssetsToCheckout.Num());
	for (UObject* AssetToCheckout: AssetsToCheckout)
	{
		if (!EditorScriptingUtils::IsAContentBrowserAsset(AssetToCheckout, FailureReason))
		{
			UE_LOG(LogEditorScripting, Warning, TEXT("CheckoutLoadedAssets. The validation of a source asset failed. %s"), *FailureReason);
		}
		else
		{
			Packages.Add(AssetToCheckout->GetOutermost());
		}
	}

	return InternalEditorLevelLibrary::Checkout(Packages);
}

bool UEditorAssetLibrary::CheckoutAsset(const FString& AssetsToCheckout)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	UObject* Result = InternalEditorLevelLibrary::LoadAsset(AssetsToCheckout, false, FailureReason);
	if (Result == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("CheckoutAsset. Failed to load the asset: %s"), *FailureReason);
		return false;
	}

	TArray<UPackage*> Packages;
	Packages.Add(Result->GetOutermost()); // Fully load and check out is done in UEditorLoadingAndSavingUtils::SavePackages

	return InternalEditorLevelLibrary::Checkout(Packages);
}

bool UEditorAssetLibrary::CheckoutDirectory(const FString& DirectoryPath, bool bRecursive)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	const bool bOnlyIfIsDirty = false;
	FString FailureReason;
	TArray<UPackage*> Packages;
	if (!InternalEditorLevelLibrary::GetContentBrowserPackagesForDirectory(DirectoryPath, bOnlyIfIsDirty, bRecursive, Packages, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("CheckoutAssets. Failed to checkout. %s"), *FailureReason);
		return false;
	}

	return InternalEditorLevelLibrary::Checkout(Packages);
}

/**
 *
 * Save operation
 *
 **/

bool UEditorAssetLibrary::SaveLoadedAsset(UObject* AssetToSave, bool bOnlyIfIsDirty)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	if (!EditorScriptingUtils::IsAContentBrowserAsset(AssetToSave, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SaveLoadedAsset. Failed to validate the source. %s"), *FailureReason);
		return false;
	}

	TArray<UPackage*> Packages;
	Packages.Add(AssetToSave->GetOutermost()); // Fully load and check out is done in UEditorLoadingAndSavingUtils::SavePackages

	// Save without a prompt
	return UEditorLoadingAndSavingUtils::SavePackages(Packages, bOnlyIfIsDirty);
}

bool UEditorAssetLibrary::SaveLoadedAssets(const TArray<UObject*>& AssetsToSave, bool bOnlyIfIsDirty)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	TArray<UPackage*> Packages;
	Packages.Reserve(AssetsToSave.Num());
	for (UObject* AssetToSave : AssetsToSave)
	{
		if (!EditorScriptingUtils::IsAContentBrowserAsset(AssetToSave, FailureReason))
		{
			UE_LOG(LogEditorScripting, Warning, TEXT("SaveLoadedAsset. The validation of a source failed. %s"), *FailureReason);
		}
		else
		{
			Packages.Add(AssetToSave->GetOutermost());
		}
	}

	// Save without a prompt
	return UEditorLoadingAndSavingUtils::SavePackages(Packages, bOnlyIfIsDirty);
}

bool UEditorAssetLibrary::SaveAsset(const FString& AssetsToSave, bool bOnlyIfIsDirty)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	UObject* Result = InternalEditorLevelLibrary::LoadAsset(AssetsToSave, false, FailureReason);
	if (Result == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SaveAsset. Failed to load asset: %s"), *FailureReason);
		return false;
	}

	TArray<UPackage*> Packages;
	Packages.Add(Result->GetOutermost()); // Fully load and check out is done in UEditorLoadingAndSavingUtils::SavePackages

	// Save without a prompt
	return UEditorLoadingAndSavingUtils::SavePackages(Packages, bOnlyIfIsDirty);
}

bool UEditorAssetLibrary::SaveDirectory(const FString& DirectoryPath, bool bOnlyIfIsDirty, bool bRecursive)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	TArray<UPackage*> Packages;
	if (!InternalEditorLevelLibrary::GetContentBrowserPackagesForDirectory(DirectoryPath, bOnlyIfIsDirty, bRecursive, Packages, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SaveDirectory. Failed to save. %s"), *FailureReason);
		return false;
	}

	// Save without a prompt
	return UEditorLoadingAndSavingUtils::SavePackages(Packages, bOnlyIfIsDirty);
}

/**
 *
 * Directory operations
 *
 **/

bool UEditorAssetLibrary::DoesDirectoryExist(const FString& DirectoryPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	FString ValidDirectoryPath = EditorScriptingUtils::ConvertAnyPathToLongPackagePath(DirectoryPath, FailureReason);
	if (ValidDirectoryPath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DoesDirectoryExists. Failed to convert the path. %s"), *FailureReason);
		return false;
	}

	FString FilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(ValidDirectoryPath + TEXT("/")));
	if (FilePath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DoesDirectoryExists. Failed to convert the path '%s' to a full path. Was it rooted?"), *ValidDirectoryPath);
		return false;
	}

	bool bResult = IFileManager::Get().DirectoryExists(*FilePath);
	if (bResult)
	{
		// The folder may not exist in the ContentBrowser
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().AddPath(ValidDirectoryPath);
	}
	return bResult;
}

bool UEditorAssetLibrary::DoesDirectoryHaveAssets(const FString& DirectoryPath, bool bRecursive)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	FString ValidDirectoryPath = EditorScriptingUtils::ConvertAnyPathToLongPackagePath(DirectoryPath, FailureReason);
	if (ValidDirectoryPath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DoesDirectoryHasAssets. Failed to convert the path. %s"), *FailureReason);
		return false;
	}

	FString FilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(ValidDirectoryPath + TEXT("/")));
	if (FilePath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DoesDirectoryHasAssets. Failed to convert the path '%s' to a full path. Was it rooted?"), *ValidDirectoryPath);
		return false;
	}

	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	return AssetRegistryModule.Get().HasAssets(*ValidDirectoryPath, bRecursive);
}

bool UEditorAssetLibrary::MakeDirectory(const FString& DirectoryPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return false;
	}

	FString FailureReason;
	FString ValidDirectoryPath = EditorScriptingUtils::ConvertAnyPathToLongPackagePath(DirectoryPath, FailureReason);
	if (ValidDirectoryPath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("MakeDirectory. Failed to convert the path. %s"), *FailureReason);
		return false;
	}

	FString FilePath = FPaths::ConvertRelativePathToFull(FPackageName::LongPackageNameToFilename(ValidDirectoryPath + TEXT("/")));
	if (FilePath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("MakeDirectory. Failed to convert the path '%s' to a full path. Was it rooted?"), *ValidDirectoryPath);
		return false;
	}

	// If the folder has not yet been created, create it before we try to add it to the AssetRegistry.
	bool bResult = true;
	if (!IFileManager::Get().DirectoryExists(*FilePath))
	{
		const bool bTree = true;
		bResult = IFileManager::Get().MakeDirectory(*FilePath, bTree);
	}

	if (bResult)
	{
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		AssetRegistryModule.Get().AddPath(ValidDirectoryPath);
	}
	return bResult;
}

/**
 *
 * List operations
 *
 **/

TArray<FString> UEditorAssetLibrary::ListAssets(const FString& DirectoryPath, bool bRecursive /*= false*/, bool bIncludeFolder /*= false*/)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TArray<FString> Result;
	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return Result;
	}

	TArray<FAssetData> AssetDatas;
	bool bIsFolder = false;
	FString ValidDirectoryPath;
	FString FailureReason;
	if (!InternalEditorLevelLibrary::ListAssetDatas(DirectoryPath, bRecursive, bIsFolder, AssetDatas, ValidDirectoryPath, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("ListAssets. Failed to find a valid folder. %s"), *FailureReason);
		return Result;
	}

	if (AssetDatas.Num() > 0)
	{
		AssetDatas.Reserve(AssetDatas.Num());
		for (const FAssetData& AssetData : AssetDatas)
		{
			Result.Add(AssetData.ObjectPath.ToString());
		}
	}

	if (bIncludeFolder && bIsFolder)
	{
		TArray<FString> SubPaths;
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(FName("AssetRegistry"));
		AssetRegistryModule.Get().GetSubPaths(ValidDirectoryPath, SubPaths, bRecursive);

		for (const FString& SubPath : SubPaths)
		{
			if (SubPath.Contains(DirectoryPath) && SubPath != DirectoryPath)
			{
				Result.Add(SubPath + TEXT('/'));
			}
		}
	}

	Result.Sort();
	return Result;
}

TArray<FString> UEditorAssetLibrary::ListAssetByTagValue(FName TagName, const FString& TagValue)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TArray<FString> Result;
	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return Result;
	}

	if (TagName == NAME_None)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("ListAssetByTagValue. The Tag '' is not valid."));
		return Result;
	}

	TMultiMap<FName, FString> TagValues;
	TagValues.Add(TagName, TagValue);

	TArray<FAssetData> FoundAssets;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	if (!AssetRegistryModule.Get().GetAssetsByTagValues(TagValues, FoundAssets))
	{
		UE_LOG(LogEditorScripting, Warning, TEXT("ListAssetByTagValue failed. The internal search input were not valid."));
		return Result;
	}

	for (const FAssetData& AssetData : FoundAssets)
	{
		FString ObjectPathStr = AssetData.PackageName.ToString();
		Result.Add(ObjectPathStr);
	}

	return Result;
}

TMap<FName, FString> UEditorAssetLibrary::GetTagValues(const FString& AssetPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TMap<FName, FString> Result;
	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return Result;
	}

	FString FailureReason;
	FAssetData AssetData = EditorScriptingUtils::FindAssetDataFromAnyPath(AssetPath, FailureReason);
	if (!AssetData.IsValid())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("UEditorAssetLibrary. Failed to find the AssetPath. %s"), *FailureReason);
		return Result;
	}

	const FAssetDataTagMap& DataTagMap = AssetData.TagsAndValues.GetMap();
	for (const auto& Itt : DataTagMap)
	{
		Result.Add(Itt.Key, Itt.Value);
	}
	return Result;
}

TMap<FName, FString> UEditorAssetLibrary::GetMetadataTagValues(UObject* Object)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TMap<FName, FString> Result;
	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return Result;
	}

#if WITH_EDITORONLY_DATA
	if (Object)
	{
		TMap<FName, FString>* TagValues = UMetaData::GetMapForObject(Object);
		if (TagValues != nullptr)
		{
			return *TagValues;
		}
	}
	else
	{
		UE_LOG(LogEditorScripting, Error, TEXT("GetMetadataTagValues failed: Object is null."));
	}
#endif // WITH_EDITORONLY_DATA
	return Result;
}

FString UEditorAssetLibrary::GetMetadataTag(UObject* Object, FName Tag)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return FString();
	}

#if WITH_EDITORONLY_DATA
	if (Object)
	{
		return Object->GetOutermost()->GetMetaData()->GetValue(Object, Tag);
	}
	else
	{
		UE_LOG(LogEditorScripting, Error, TEXT("GetMetadataTag failed: Object is null."));
	}
#endif // WITH_EDITORONLY_DATA
	return FString();
}

void UEditorAssetLibrary::SetMetadataTag(UObject* Object, FName Tag, const FString& Value)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	if (Object)
	{
		Object->Modify();
		Object->GetOutermost()->GetMetaData()->SetValue(Object, Tag, *Value);
	}
	else
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SetMetadataTag failed: Object is null."));
	}
#endif // WITH_EDITORONLY_DATA
}

void UEditorAssetLibrary::RemoveMetadataTag(UObject* Object, FName Tag)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return;
	}

#if WITH_EDITORONLY_DATA
	if (Object)
	{
		Object->Modify();
		Object->GetOutermost()->GetMetaData()->RemoveValue(Object, Tag);
	}
	else
	{
		UE_LOG(LogEditorScripting, Error, TEXT("RemoveMetadataTag failed: Object is null."));
	}
#endif // WITH_EDITORONLY_DATA
}

void UEditorAssetLibrary::SyncBrowserToObjects(const TArray<FString>& AssetPaths)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE() || !InternalEditorLevelLibrary::IsAssetRegistryModuleLoading())
	{
		return;
	}

	if (GEditor)
	{
		TArray<FAssetData> Assets;
		for (const FString& AssetPath : AssetPaths)
		{
			FString FailureReason;
			FAssetData Asset = EditorScriptingUtils::FindAssetDataFromAnyPath(AssetPath, FailureReason);
			if (Asset.IsValid())
			{
				Assets.Add(Asset);
			}
			else
			{
				UE_LOG(LogEditorScripting, Warning, TEXT("SyncBrowserToObjects. Cannot sync: %s"), *AssetPath, *FailureReason);
			}
		}
		if (Assets.Num() > 0)
		{
			GEditor->SyncBrowserToObjects(Assets);
		}
	}
}

#undef LOCTEXT_NAMESPACE

