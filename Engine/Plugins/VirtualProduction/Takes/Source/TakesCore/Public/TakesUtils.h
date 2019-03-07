// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieScene.h"
#include "Modules/ModuleManager.h"
#include "IAssetRegistry.h"
#include "AssetRegistryModule.h"
#include "UObject/UObjectGlobals.h"
#include "TakesCoreFwd.h"

class UWorld;
class ULevelSequence;
class UMovieScene;

namespace TakesUtils
{
	/*
	 * Get the first PIE world (or first PIE client world if there is more than one)
	 */

	TAKESCORE_API UWorld* GetFirstPIEWorld();

	TAKESCORE_API void ClampPlaybackRangeToEncompassAllSections(UMovieScene* InMovieScene);

	TAKESCORE_API void SaveAsset(UObject* InObject);

	TAKESCORE_API void CreateCameraCutTrack(ULevelSequence* LevelSequence, const FGuid& RecordedCameraGuid, const FMovieSceneSequenceID& SequenceID, const TRange<FFrameNumber>& InRange);

	/*
	* Creates a new Package with the given Package Name (ie: /Game/Test/Foo) of the specified AssetType. If a package already exists at that name the package name will
	* have a number appended and iterated on until an unused package name is found. InPackageName will be modified in this case and will return the package name that the
	* asset was actually created at.
	*
	* You should consider calling MarkPackageDirty() on the returned asset if you further modify it, and you shouldstill notify the FAssetRegistryModule that the asset was 
	* created after this by calling FAssetRegistryModule::AssetCreated
	* 
	* @param InPackageName	- The desired package name (path and asset name) for the new asset. May be mutated by this function if that package name is already taken.
	* @param OutAsset		- The resulting asset if we were able to successfully create an asset.
	* @param OutError		- Human readable error string to describe what went wrong (if anything). Can be nullptr if you don't care about the error message.
	* @param OptionalBase	- Optional asset reference to duplicate the new asset from. If nullptr a brand new asset will be created.
	* @return True if the asset was created successfully, false if there was an error.
	*/
	template<typename AssetType>
	static bool CreateNewAssetPackage(FString &InPackageName, AssetType*& OutAsset, FText* OutError, AssetType* OptionalBase = nullptr)
	{
		if (!FPackageName::IsValidLongPackageName(InPackageName))
		{
			if (OutError)
			{
				*OutError = FText::Format(NSLOCTEXT("TakeRecorderUtils", "InvalidPathError", "{0} is not a valid asset path."), FText::FromString(InPackageName));
			}
			return false;
		}

		int32 UniqueIndex = 2;
		int32 BasePackageLength = InPackageName.Len();

		// Generate a unique level sequence name for this take if there are already assets of the same name
		IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		while (AssetRegistry.GetAssetByObjectPath(*InPackageName).IsValid())
		{
			int32 TrimCount = InPackageName.Len() - BasePackageLength;
			if (TrimCount > 0)
			{
				InPackageName.RemoveAt(BasePackageLength, TrimCount, false);
			}

			InPackageName += FString::Printf(TEXT("_%04d"), UniqueIndex++);
		}

		// Create the asset to record into
		const FString NewAssetName = FPackageName::GetLongPackageAssetName(InPackageName);
		UPackage*     NewPackage = CreatePackage(nullptr, *InPackageName);

		if (OptionalBase)
		{
			// Duplicate the level sequence into the asset package
			OutAsset = Cast<AssetType>(StaticDuplicateObject(OptionalBase, NewPackage, *NewAssetName, RF_NoFlags));
			OutAsset->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
		}
		else
		{
			// Create a new level sequence from scratch
			OutAsset = NewObject<AssetType>(NewPackage, *NewAssetName, RF_Public | RF_Standalone | RF_Transactional);
		}

		return true;
	}

	/**
	 * Utility function that creates an asset with the specified asset path and name.
	 * If the asset cannot be created (as one already exists), we try to postfix the asset
	 * name until we can successfully create the asset.
	 */
	template<typename AssetType>
	static AssetType* MakeNewAsset(const FString& BaseAssetPath, const FString& BaseAssetName)
	{
		const FString Dot(TEXT("."));
		FString AssetPath = BaseAssetPath;
		FString AssetName = BaseAssetName;
		AssetName = AssetName.Replace(TEXT("."), TEXT("_"));

		AssetPath /= AssetName;
		AssetPath += Dot + AssetName;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
		FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*AssetPath);

		// if object with same name exists, try a different name until we don't find one
		int32 ExtensionIndex = 0;
		while (AssetData.IsValid() && AssetData.GetClass() == AssetType::StaticClass())
		{
			AssetName = FString::Printf(TEXT("%s_%d"), *AssetName, ExtensionIndex);
			AssetPath = (BaseAssetPath / AssetName) + Dot + AssetName;
			AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*AssetPath);

			ExtensionIndex++;
		}

		// Create the new asset in the package we just made
		AssetPath = (BaseAssetPath / AssetName);

		FString FileName;
		if (FPackageName::TryConvertLongPackageNameToFilename(AssetPath, FileName))
		{
			UObject* Package = CreatePackage(nullptr, *AssetPath);
			return NewObject<AssetType>(Package, *AssetName, RF_Public | RF_Standalone);
		}

		UE_LOG(LogTakesCore, Error, TEXT("Couldn't create file for package %s"), *AssetPath);

		return nullptr;
	}
}