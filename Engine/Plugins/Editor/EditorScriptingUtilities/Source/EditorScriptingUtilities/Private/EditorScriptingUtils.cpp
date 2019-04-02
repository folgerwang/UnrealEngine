// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditorScriptingUtils.h"
#include "Algo/Count.h"
#include "AssetRegistryModule.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "ObjectTools.h"
#include "UnrealEdGlobals.h"


DEFINE_LOG_CATEGORY(LogEditorScripting);

namespace EditorScriptingUtils
{
	bool CheckIfInEditorAndPIE()
	{
		if (!IsInGameThread())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("You are not on the main thread."));
			return false;
		}
		if (!GIsEditor)
		{
			UE_LOG(LogEditorScripting, Error, TEXT("You are not in the Editor."));
			return false;
		}
		if (GEditor->PlayWorld || GIsPlayInEditorWorld)
		{
			UE_LOG(LogEditorScripting, Error, TEXT("The Editor is currently in a play mode."));
			return false;
		}
		return true;
	}

	bool IsPackageFlagsSupportedForAssetLibrary(uint32 PackageFlags)
	{
		return (PackageFlags & (PKG_ContainsMap | PKG_PlayInEditor | PKG_ContainsMapData)) == 0;
	}

	// Test for invalid characters
	bool IsAValidPath(const FString& Path, const TCHAR* InvalidChar, FString& OutFailureReason)
	{
		// Like !FName::IsValidGroupName(Path)), but with another list and no conversion to from FName
		// InvalidChar may be INVALID_OBJECTPATH_CHARACTERS or INVALID_LONGPACKAGE_CHARACTERS or ...
		const int32 StrLen = FCString::Strlen(InvalidChar);
		for (int32 Index = 0; Index < StrLen; ++Index)
		{
			int32 FoundIndex = 0;
			if (Path.FindChar(InvalidChar[Index], FoundIndex))
			{
				OutFailureReason = FString::Printf(TEXT("Can't convert the path %s because it contains invalid characters."), *Path);
				return false;
			}
		}

		if (Path.Len() > FPlatformMisc::GetMaxPathLength())
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert the path %s because it is too long; this may interfere with cooking for consoles. Unreal filenames should be no longer than %d characters."), *Path, FPlatformMisc::GetMaxPathLength());
			return false;
		}
		return true;
	}

	bool IsAValidPathForCreateNewAsset(const FString& ObjectPath, FString& OutFailureReason)
	{
		const FString ObjectName = FPackageName::ObjectPathToObjectName(ObjectPath);

		// Make sure the name is not already a class or otherwise invalid for saving
		FText FailureReason;
		if (!FFileHelper::IsFilenameValidForSaving(ObjectName, FailureReason))
		{
			OutFailureReason = FailureReason.ToString();
			return false;
		}

		// Make sure the new name only contains valid characters
		if (!FName::IsValidXName(ObjectName, INVALID_OBJECTNAME_CHARACTERS INVALID_LONGPACKAGE_CHARACTERS, &FailureReason))
		{
			OutFailureReason = FailureReason.ToString();
			return false;
		}

		// Make sure we are not creating an FName that is too large
		if (ObjectPath.Len() > NAME_SIZE)
		{
			OutFailureReason = TEXT("This asset name is too long. Please choose a shorter name.");
			return false;
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*ObjectPath);
		if (AssetData.IsValid())
		{
			OutFailureReason = TEXT("An asset already exists at this location.");
			return false;
		}

		return true;
	}

	bool HasValidRoot(const FString& ObjectPath)
	{
		FString Filename;
		bool bValidRoot = true;
		if (!ObjectPath.IsEmpty() && ObjectPath[ObjectPath.Len() - 1] == TEXT('/'))
		{
			bValidRoot = FPackageName::TryConvertLongPackageNameToFilename(ObjectPath, Filename);
		}
		else
		{
			FString ObjectPathWithSlash = ObjectPath;
			ObjectPathWithSlash.AppendChar(TEXT('/'));
			bValidRoot = FPackageName::TryConvertLongPackageNameToFilename(ObjectPathWithSlash, Filename);
		}

		return bValidRoot;
	}

	/** Remove Class from "Class /Game/MyFolder/MyAsset" */
	FString RemoveFullName(const FString& AnyAssetPath, FString& OutFailureReason)
	{
		FString Result = AnyAssetPath.TrimStartAndEnd();
		int32 NumberOfSpace = Algo::Count(AnyAssetPath, TEXT(' '));

		if (NumberOfSpace == 0)
		{
			return MoveTemp(Result);
		}
		else if (NumberOfSpace > 1)
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert path '%s' because there are too many spaces."), *AnyAssetPath);
			return FString();
		}
		else// if (NumberOfSpace == 1)
		{
			int32 FoundIndex = 0;
			AnyAssetPath.FindChar(TEXT(' '), FoundIndex);
			check(FoundIndex > INDEX_NONE && FoundIndex < AnyAssetPath.Len()); // because of TrimStartAndEnd

			// Confirm that it's a valid Class
			FString ClassName = AnyAssetPath.Left(FoundIndex);

			// Convert \ to /
			ClassName.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);

			// Test ClassName for invalid Char
			const int32 StrLen = FCString::Strlen(INVALID_OBJECTNAME_CHARACTERS);
			for (int32 Index = 0; Index < StrLen; ++Index)
			{
				int32 InvalidFoundIndex = 0;
				if (ClassName.FindChar(INVALID_OBJECTNAME_CHARACTERS[Index], InvalidFoundIndex))
				{
					OutFailureReason = FString::Printf(TEXT("Can't convert the path %s because it contains invalid characters (probably spaces)."), *AnyAssetPath);
					return FString();
				}
			}

			// Return the path without the Class name
			return AnyAssetPath.Mid(FoundIndex + 1);
		}
	}

	FString ConvertAnyPathToObjectPath(const FString& AnyAssetPath, FString& OutFailureReason)
	{
		if (AnyAssetPath.Len() < 2) // minimal length to have /G
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert the path '%s' because the Root path need to be specified. ie /Game/"), *AnyAssetPath);
			return FString();
		}

		// Remove class name from Reference Path
		FString TextPath = FPackageName::ExportTextPathToObjectPath(AnyAssetPath);

		// Remove class name Fullname
		TextPath = RemoveFullName(TextPath, OutFailureReason);
		if (TextPath.IsEmpty())
		{
			return FString();
		}

		// Extract the subobject path if any
		FString SubObjectPath;
		int32 SubObjectDelimiterIdx;
		if (TextPath.FindChar(SUBOBJECT_DELIMITER_CHAR, SubObjectDelimiterIdx))
		{
			SubObjectPath = TextPath.Mid(SubObjectDelimiterIdx + 1);
			TextPath = TextPath.Left(SubObjectDelimiterIdx);
		}

		// Convert \ to /
		TextPath.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
		FPaths::RemoveDuplicateSlashes(TextPath);

		// Get asset full name, i.e."PackageName.ObjectName:InnerAssetName.2ndInnerAssetName" from "/Game/Folder/PackageName.ObjectName:InnerAssetName.2ndInnerAssetName"
		FString AssetFullName;
		{
			// Get everything after the last slash
			int32 IndexOfLastSlash = INDEX_NONE;
			TextPath.FindLastChar('/', IndexOfLastSlash);

			FString Folders = TextPath.Left(IndexOfLastSlash);
			// Test for invalid characters
			if (!IsAValidPath(Folders, INVALID_LONGPACKAGE_CHARACTERS, OutFailureReason))
			{
				return FString();
			}

			AssetFullName = TextPath.Mid(IndexOfLastSlash + 1);
		}

		// Get the object name
		FString ObjectName = FPackageName::ObjectPathToObjectName(AssetFullName);
		if (ObjectName.IsEmpty())
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert the path '%s' because it doesn't contain an asset name."), *AnyAssetPath);
			return FString();
		}

		// Test for invalid characters
		if (!IsAValidPath(ObjectName, INVALID_OBJECTNAME_CHARACTERS, OutFailureReason))
		{
			return FString();
		}

		// Confirm that we have a valid Root Package and get the valid PackagePath /Game/MyFolder/MyAsset
		FString PackagePath;
		if (!FPackageName::TryConvertFilenameToLongPackageName(TextPath, PackagePath, &OutFailureReason))
		{
			return FString();
		}

		if (PackagePath.Len() == 0)
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert path '%s' because the PackagePath is empty."), *AnyAssetPath);
			return FString();
		}

		if (PackagePath[0] != TEXT('/'))
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert path '%s' because the PackagePath '%s' doesn't start with a '/'."), *AnyAssetPath, *PackagePath);
			return FString();
		}

		FString ObjectPath = FString::Printf(TEXT("%s.%s"), *PackagePath, *ObjectName);

		if (FPackageName::IsScriptPackage(ObjectPath))
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert the path '%s' because it start with /Script/"), *AnyAssetPath);
			return FString();
		}
		if (FPackageName::IsMemoryPackage(ObjectPath))
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert the path '%s' because it start with /Memory/"), *AnyAssetPath);
			return FString();
		}

		// Confirm that the PackagePath starts with a valid root
		if (!HasValidRoot(PackagePath))
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert the path '%s' because it does not map to a root."), *AnyAssetPath);
			return FString();
		}

		return ObjectPath;
	}

	FString ConvertAnyPathToLongPackagePath(const FString& AnyPath, FString& OutFailureReason)
	{
		if (AnyPath.Len() < 2) // minimal length to have /G
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert the path '%s' because the Root path need to be specified. ie /Game/"), *AnyPath);
			return FString();
		}

		// Prepare for TryConvertFilenameToLongPackageName

		// Remove class name from Reference Path
		FString TextPath = FPackageName::ExportTextPathToObjectPath(AnyPath);

		// Remove class name Fullname
		TextPath = RemoveFullName(TextPath, OutFailureReason);
		if (TextPath.IsEmpty())
		{
			return FString();
		}

		// Convert \ to /
		TextPath.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
		FPaths::RemoveDuplicateSlashes(TextPath);

		{
			// Remove .
			int32 ObjectDelimiterIdx;
			if (TextPath.FindChar(TEXT('.'), ObjectDelimiterIdx))
			{
				TextPath = TextPath.Left(ObjectDelimiterIdx);
			}

			// Remove :
			if (TextPath.FindChar(TEXT(':'), ObjectDelimiterIdx))
			{
				TextPath = TextPath.Left(ObjectDelimiterIdx);
			}
		}

		// Test for invalid characters
		if (!IsAValidPath(TextPath, INVALID_LONGPACKAGE_CHARACTERS, OutFailureReason))
		{
			return FString();
		}

		// Confirm that we have a valid Root Package and get the valid PackagePath /Game/MyFolder
		FString PackagePath;
		if (!FPackageName::TryConvertFilenameToLongPackageName(TextPath, PackagePath, &OutFailureReason))
		{
			return FString();
		}

		if (PackagePath.Len() == 0)
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert the path '%s' because of an internal error. TryConvertFilenameToLongPackageName should have return false."), *AnyPath);
			return FString();
		}

		if (PackagePath[0] != TEXT('/'))
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert path '%s' because the PackagePath '%s' doesn't start with a '/'."), *AnyPath, *PackagePath);
			return FString();
		}

		if (PackagePath[PackagePath.Len() - 1] == TEXT('/'))
		{
			PackagePath.RemoveAt(PackagePath.Len() - 1);
		}

		if (FPackageName::IsScriptPackage(PackagePath))
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert the path '%s' because it starts with /Script/"), *AnyPath);
			return FString();
		}
		if (FPackageName::IsMemoryPackage(PackagePath))
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert the path '%s' because it starts with /Memory/"), *AnyPath);
			return FString();
		}

		// Confirm that the PackagePath start with a valid root
		if (!HasValidRoot(PackagePath))
		{
			OutFailureReason = FString::Printf(TEXT("Can't convert the path '%s' because it does not map to a root."), *AnyPath);
			return FString();
		}

		return PackagePath;
	}

	FAssetData FindAssetDataFromAnyPath(const FString& AnyAssetPath, FString& OutFailureReason)
	{
		FString ObjectPath = ConvertAnyPathToObjectPath(AnyAssetPath, OutFailureReason);
		if (ObjectPath.IsEmpty())
		{
			return FAssetData();
		}

		if (FEditorFileUtils::IsMapPackageAsset(ObjectPath))
		{
			OutFailureReason = FString::Printf(TEXT("The AssetData '%s' is not accessible because it is of type Map/Level."), *ObjectPath);
			return FAssetData();
		}

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(*ObjectPath);
		if (!AssetData.IsValid())
		{
			OutFailureReason = FString::Printf(TEXT("The AssetData '%s' could not be found in the Content Browser."), *ObjectPath);
			return FAssetData();
		}

		// Prevent loading a umap...
		if (!IsPackageFlagsSupportedForAssetLibrary(AssetData.PackageFlags))
		{
			OutFailureReason = FString::Printf(TEXT("The AssetData '%s' is not accessible because it is of type Map/Level."), *ObjectPath);
			return FAssetData();
		}
		return AssetData;
	}

	bool IsAContentBrowserAsset(UObject* Object, FString& OutFailureReason)
	{
		if (Object == nullptr || Object->IsPendingKill())
		{
			OutFailureReason = TEXT("The Asset is not valid.");
			return false;
		}

		if (Cast<UField>(Object))
		{
			OutFailureReason = FString::Printf(TEXT("The object is of the base class type '%s'"), *Object->GetName());
			return false;
		}

		if (!ObjectTools::IsObjectBrowsable(Object))
		{
			OutFailureReason = FString::Printf(TEXT("The object %s is not an asset."), *Object->GetName());
			return false;
		}

		UPackage* NewPackage = Object->GetOutermost();

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		FAssetData AssetData = AssetRegistryModule.Get().GetAssetByObjectPath(NewPackage->GetFName());
		if (!AssetData.IsValid())
		{
			OutFailureReason = FString::Printf(TEXT("The AssetData '%s' could not be found in the Content Browser."), *NewPackage->GetName());
			return false;
		}

		if (FEditorFileUtils::IsMapPackageAsset(AssetData.ObjectPath.ToString()))
		{
			OutFailureReason = FString::Printf(TEXT("The AssetData '%s' is not accessible because it is of type Map/Level."), *AssetData.ObjectPath.ToString());
			return false;
		}

		// check if it's a umap
		if (!IsPackageFlagsSupportedForAssetLibrary(AssetData.PackageFlags))
		{
			OutFailureReason = FString::Printf(TEXT("The AssetData '%s' is not accessible because it is of type Map/Level."), *NewPackage->GetName());
			return false;
		}

		return true;
	}

	bool GetAssetsInPath(const FString& LongPackagePath, bool bRecursive, TArray<FAssetData>& OutAssetDatas, TArray<FAssetData>& OutMapAssetDatas, FString& OutFailureReason)
	{
		OutAssetDatas.Reset();
		OutMapAssetDatas.Reset();

		// Ask the AssetRegistry if it's a folder
		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		if (!AssetRegistryModule.Get().GetAssetsByPath(*LongPackagePath, OutAssetDatas, bRecursive))
		{	// GetAssetsByPath want this syntax /Game/MyFolder
			OutFailureReason = TEXT("The internal search input were not valid.");
			return false;
		}

		// Remove Map & PlayInEditor package
		for (int32 Index = OutAssetDatas.Num() - 1; Index >= 0; --Index)
		{
			if (FEditorFileUtils::IsMapPackageAsset(OutAssetDatas[Index].ObjectPath.ToString())
				|| !IsPackageFlagsSupportedForAssetLibrary(OutAssetDatas[Index].PackageFlags))
			{
				OutMapAssetDatas.Add(OutAssetDatas[Index]);
				OutAssetDatas.RemoveAtSwap(Index);
			}
		}

		return true;
	}

	bool GetAssetsInPath(const FString& LongPackagePath, bool bRecursive, TArray<UObject*>& OutAssets, TArray<FAssetData>& OutCouldNotLoadAssetData, TArray<FString>& OutFailureReasons)
	{
		OutAssets.Reset();
		OutCouldNotLoadAssetData.Reset();
		OutFailureReasons.Reset();

		TArray<FAssetData> AssetDatas;
		FString FailureReason;
		if (!GetAssetsInPath(LongPackagePath, bRecursive, AssetDatas, OutCouldNotLoadAssetData, FailureReason))
		{
			OutFailureReasons.Add(MoveTemp(FailureReason));
			return false;
		}

		for (const FAssetData& AssetData : AssetDatas)
		{
			FString LoadFailureReason;
			UObject* LoadedObject = LoadAsset(AssetData, false, LoadFailureReason);
			if (LoadedObject)
			{
				OutAssets.Add(LoadedObject);
			}
			else
			{
				OutFailureReasons.Add(MoveTemp(LoadFailureReason));
				OutCouldNotLoadAssetData.Add(AssetData);
			}
		}

		return true;
	}

	UObject* LoadAsset(const FAssetData& AssetData, bool bAllowMapAsset, FString& OutFailureReason)
	{
		if (!AssetData.IsValid())
		{
			return nullptr;
		}

		if (!bAllowMapAsset)
		{
			if (FEditorFileUtils::IsMapPackageAsset(AssetData.ObjectPath.ToString())
				|| !IsPackageFlagsSupportedForAssetLibrary(AssetData.PackageFlags))
			{
				OutFailureReason = FString::Printf(TEXT("The AssetData '%s' is not accessible because it is of type Map/Level."), *AssetData.ObjectPath.ToString());
				return nullptr;
			}
		}

		UObject* FoundObject = AssetData.GetAsset();
		if (!FoundObject || FoundObject->IsPendingKill())
		{
			OutFailureReason = FString::Printf(TEXT("The asset '%s' exists but was not able to be loaded."), *AssetData.ObjectPath.ToString());
		}
		else if (!FoundObject->IsAsset())
		{
			OutFailureReason = FString::Printf(TEXT("'%s' is not a valid asset."), *AssetData.ObjectPath.ToString());
			FoundObject = nullptr;
		}
		return FoundObject;
	}


	bool DeleteEmptyDirectoryFromDisk(const FString& LongPackagePath)
	{
		struct FEmptyFolderVisitor : public IPlatformFile::FDirectoryVisitor
		{
			bool bIsEmpty;

			FEmptyFolderVisitor()
				: bIsEmpty(true)
			{
			}

			virtual bool Visit(const TCHAR* FilenameOrDirectory, bool bIsDirectory) override
			{
				if (!bIsDirectory)
				{
					bIsEmpty = false;
					return false; // abort searching
				}

				return true; // continue searching
			}
		};

		FString PathToDeleteOnDisk;
		if (FPackageName::TryConvertLongPackageNameToFilename(LongPackagePath, PathToDeleteOnDisk))
		{
			// Look for files on disk in case the folder contains things not tracked by the asset registry
			FEmptyFolderVisitor EmptyFolderVisitor;
			IFileManager::Get().IterateDirectoryRecursively(*PathToDeleteOnDisk, EmptyFolderVisitor);

			if (EmptyFolderVisitor.bIsEmpty)
			{
				return IFileManager::Get().DeleteDirectory(*PathToDeleteOnDisk, false, true);
			}
		}

		return false;
	}
}
