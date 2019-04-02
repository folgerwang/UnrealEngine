// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/Interface.h"
#include "UObject/SoftObjectPath.h"
#include "AssetTypeCategories.h"
#include "IAssetTypeActions.h"
#include "AutomatedAssetImportData.h"
#include "ARFilter.h"
#include "IAssetTools.generated.h"


struct FAssetData;
class IAssetTypeActions;
class IClassTypeActions;
class UFactory;
class UAssetImportTask;
class UAdvancedCopyCustomization;

USTRUCT(BlueprintType)
struct FAssetRenameData
{
	GENERATED_BODY()

	/** Object being renamed */
	UPROPERTY(BlueprintReadWrite, Category=AssetRenameData)
	TWeakObjectPtr<UObject> Asset;

	/** New path to package without package name, ie /Game/SubDirectory */
	UPROPERTY(BlueprintReadWrite, Category = AssetRenameData)
	FString NewPackagePath;

	/** New package and asset name, new object path will be PackagePath/NewName.NewName */
	UPROPERTY(BlueprintReadWrite, Category = AssetRenameData)
	FString NewName;

	/** Full path to old name, in form /Game/SubDirectory/OldName.OldName:SubPath*/
	UPROPERTY()
	FSoftObjectPath OldObjectPath;

	/** New full path, may be a SubObject */
	UPROPERTY()
	FSoftObjectPath NewObjectPath;

	/** If true, only fix soft references. This will work even if Asset is null because it has already been renamed */
	UPROPERTY()
	bool bOnlyFixSoftReferences;

	FAssetRenameData()
		: bOnlyFixSoftReferences(false)
	{}

	/** These constructors leave some fields empty, they are fixed up inside AssetRenameManager */
	FAssetRenameData(const TWeakObjectPtr<UObject>& InAsset, const FString& InNewPackagePath, const FString& InNewName)
		: Asset(InAsset)
		, NewPackagePath(InNewPackagePath)
		, NewName(InNewName)
		, bOnlyFixSoftReferences(false)
	{
	}
	
	FAssetRenameData(const FSoftObjectPath& InOldObjectPath, const FSoftObjectPath& InNewObjectPath, bool bInOnlyFixSoftReferences = false)
		: OldObjectPath(InOldObjectPath)
		, NewObjectPath(InNewObjectPath)
		, bOnlyFixSoftReferences(bInOnlyFixSoftReferences)
	{
	}
};


DECLARE_MULTICAST_DELEGATE_OneParam(FAssetPostRenameEvent, const TArray<FAssetRenameData>&);


struct FAdvancedAssetCategory
{
	EAssetTypeCategories::Type CategoryType;
	FText CategoryName;

	FAdvancedAssetCategory(EAssetTypeCategories::Type InCategoryType, FText InCategoryName)
		: CategoryType(InCategoryType)
		, CategoryName(InCategoryName)
	{
	}
};

USTRUCT()
struct FAdvancedCopyParams
{
	GENERATED_USTRUCT_BODY()

	bool bShouldForceSave;
	bool bCopyOverAllDestinationOverlaps;
	bool bGenerateUniqueNames;
	bool bShouldSuppressUI;
	bool bShouldCheckForDependencies;

	const TArray<FName>& GetSelectedPackageNames() const
	{
		return SelectedPackageNames;
	}

	const FString& GetDropLocationForAdvancedCopy() const
	{
		return DropLocationForAdvancedCopy;
	}

	const TArray<UAdvancedCopyCustomization*>& GetCustomizationsToUse() const
	{
		return CustomizationsToUse;
	}

	void AddCustomization(UAdvancedCopyCustomization* InCustomization)
	{
		CustomizationsToUse.Add(InCustomization);
	}

	FAdvancedCopyParams(TArray<FName> InSelectedPackageNames, FString InDropLocationForAdvancedCopy)
		: bShouldForceSave(false)
		, bCopyOverAllDestinationOverlaps(true)
		, bGenerateUniqueNames(false)
		, bShouldSuppressUI(false)
		, bShouldCheckForDependencies(true)
		, SelectedPackageNames(InSelectedPackageNames)
		, DropLocationForAdvancedCopy(InDropLocationForAdvancedCopy)
	{
	}

	FAdvancedCopyParams() {}

private:
	TArray<FName> SelectedPackageNames;
	TArray<UAdvancedCopyCustomization*> CustomizationsToUse;
	FString DropLocationForAdvancedCopy;

};

UINTERFACE(MinimalApi, BlueprintType, meta = (CannotImplementInterfaceInBlueprint))
class UAssetTools : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IAssetTools
{
	GENERATED_IINTERFACE_BODY()

public:


	/** Registers an asset type actions object so it can provide information about and actions for asset types. */
	virtual void RegisterAssetTypeActions(const TSharedRef<IAssetTypeActions>& NewActions) = 0;

	/** Unregisters an asset type actions object. It will no longer provide information about or actions for asset types. */
	virtual void UnregisterAssetTypeActions(const TSharedRef<IAssetTypeActions>& ActionsToRemove) = 0;

	/** Generates a list of currently registered AssetTypeActions */
	virtual void GetAssetTypeActionsList(TArray<TWeakPtr<IAssetTypeActions>>& OutAssetTypeActionsList) const = 0;

	/** Gets the appropriate AssetTypeActions for the supplied class */
	virtual TWeakPtr<IAssetTypeActions> GetAssetTypeActionsForClass(UClass* Class) const = 0;

	/** Gets the list of appropriate AssetTypeActions for the supplied class */
	virtual TArray<TWeakPtr<IAssetTypeActions>> GetAssetTypeActionsListForClass(UClass* Class) const = 0;

	/**
	* Allocates a Category bit for a user-defined Category, or EAssetTypeCategories::Misc if all available bits are allocated.
	* Ignores duplicate calls with the same CategoryKey (returns the existing bit but does not change the display name).
	*/
	virtual EAssetTypeCategories::Type RegisterAdvancedAssetCategory(FName CategoryKey, FText CategoryDisplayName) = 0;

	/** Returns the allocated Category bit for a user-specified Category, or EAssetTypeCategories::Misc if it doesn't exist */
	virtual EAssetTypeCategories::Type FindAdvancedAssetCategory(FName CategoryKey) const = 0;

	/** Returns the list of all advanced asset categories */
	virtual void GetAllAdvancedAssetCategories(TArray<FAdvancedAssetCategory>& OutCategoryList) const = 0;

	/** Registers a class type actions object so it can provide information about and actions for class asset types. */
	virtual void RegisterClassTypeActions(const TSharedRef<IClassTypeActions>& NewActions) = 0;

	/** Unregisters a class type actions object. It will no longer provide information about or actions for class asset types. */
	virtual void UnregisterClassTypeActions(const TSharedRef<IClassTypeActions>& ActionsToRemove) = 0;

	/** Generates a list of currently registered ClassTypeActions */
	virtual void GetClassTypeActionsList(TArray<TWeakPtr<IClassTypeActions>>& OutClassTypeActionsList) const = 0;

	/** Gets the appropriate ClassTypeActions for the supplied class */
	virtual TWeakPtr<IClassTypeActions> GetClassTypeActionsForClass(UClass* Class) const = 0;

	/**
	 * Fills out a menubuilder with a list of commands that can be applied to the specified objects.
	 *
	 * @param InObjects the objects for which to generate type-specific menu options
	 * @param MenuBuilder the menu in which to build options
	 * @param bIncludeHeader if true, will include a heading in the menu if any options were found
	 * @return true if any options were added to the MenuBuilder
	 */
	virtual bool GetAssetActions(const TArray<UObject*>& InObjects, class FMenuBuilder& MenuBuilder, bool bIncludeHeading = true) = 0;

	/**
	 * Creates an asset with the specified name, path, and factory
	 *
	 * @param AssetName the name of the new asset
	 * @param PackagePath the package that will contain the new asset
	 * @param AssetClass the class of the new asset
	 * @param Factory the factory that will build the new asset
	 * @param CallingContext optional name of the module or method calling CreateAsset() - this is passed to the factory
	 * @return the new asset or NULL if it fails
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	virtual UObject* CreateAsset(const FString& AssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory, FName CallingContext = NAME_None) = 0;

	/** Opens an asset picker dialog and creates an asset with the specified name and path */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	virtual UObject* CreateAssetWithDialog(const FString& AssetName, const FString& PackagePath, UClass* AssetClass, UFactory* Factory, FName CallingContext = NAME_None) = 0;

	/** Opens an asset picker dialog and creates an asset with the chosen path */
	UE_DEPRECATED(4.17, "This version of CreateAsset has been deprecated.  Use CreateAssetWithDialog instead")
	virtual UObject* CreateAsset(UClass* AssetClass, UFactory* Factory, FName CallingContext = NAME_None) = 0;

	/** Opens an asset picker dialog and creates an asset with the path chosen in the dialog */
	virtual UObject* CreateAssetWithDialog(UClass* AssetClass, UFactory* Factory, FName CallingContext = NAME_None) = 0;

	/** Opens an asset picker dialog and creates an asset with the specified name and path. Uses OriginalObject as the duplication source. */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	virtual UObject* DuplicateAssetWithDialog(const FString& AssetName, const FString& PackagePath, UObject* OriginalObject) = 0;

	/** Creates an asset with the specified name and path. Uses OriginalObject as the duplication source. */
	UFUNCTION(BlueprintCallable, Category="Editor Scripting | Asset Tools")
	virtual UObject* DuplicateAsset(const FString& AssetName, const FString& PackagePath, UObject* OriginalObject) = 0;

	/** Renames assets using the specified names. */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	virtual bool RenameAssets(const TArray<FAssetRenameData>& AssetsAndNames) const = 0;

	/** Renames assets using the specified names. */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	virtual void RenameAssetsWithDialog(const TArray<FAssetRenameData>& AssetsAndNames, bool bAutoCheckout = false) const = 0;

	/** Returns list of objects that soft reference the given soft object path. This will load assets into memory to verify */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	virtual void FindSoftReferencesToObject(FSoftObjectPath TargetObject, TArray<UObject*>& ReferencingObjects) const = 0;

	/**
	 * Function that renames all FSoftObjectPath object with the old asset path to the new one.
	 *
	 * @param PackagesToCheck Packages to check for referencing FSoftObjectPath.
	 * @param AssetRedirectorMap Map from old asset path to new asset path
	 */
	virtual void RenameReferencingSoftObjectPaths(const TArray<UPackage *> PackagesToCheck, const TMap<FSoftObjectPath, FSoftObjectPath>& AssetRedirectorMap) const = 0;

	/** Event issued at the end of the rename process */
	virtual FAssetPostRenameEvent& OnAssetPostRename() = 0;

	UE_DEPRECATED(4.17, "This version of ImportAssets has been deprecated.  Use ImportAssetsWithDialog instead")
	virtual TArray<UObject*> ImportAssets(const FString& DestinationPath) = 0;

	/**
	 * Opens a file open dialog to choose files to import to the destination path.
	 *
	 * @param DestinationPath	Path to import files to
	 * @return list of successfully imported assets
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	virtual TArray<UObject*> ImportAssetsWithDialog(const FString& DestinationPath) = 0;

	/**
	 * Imports the specified files to the destination path.
	 *
	 * @param Files				Files to import
	 * @param DestinationPath	destination path for imported files
	 * @param ChosenFactory		Specific factory to use for object creation
	 * @param bSyncToBrowser	If true sync content browser to first imported asset after import
	 * @return list of successfully imported assets
	 */
	virtual TArray<UObject*> ImportAssets(const TArray<FString>& Files, const FString& DestinationPath, UFactory* ChosenFactory = NULL, bool bSyncToBrowser = true, TArray<TPair<FString, FString>>* FilesAndDestinations = nullptr) const = 0;

	/**
	 * Imports assets using data specified completely up front.  Does not ever ask any questions of the user or show any modal error messages
	 *
	 * @param AutomatedImportData	Data that specifies how to import each file
	 * @return list of successfully imported assets
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	virtual TArray<UObject*> ImportAssetsAutomated( const UAutomatedAssetImportData* ImportData) const = 0;

	/**
	* Imports assets using tasks specified.
	*
	* @param ImportTasks	Tasks that specify how to import each file
	*/
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	virtual void ImportAssetTasks(const TArray<UAssetImportTask*>& ImportTasks) const = 0;

	/**
	 * Exports the specified objects to file.
	 *
	 * @param	AssetsToExport					List of full asset names (e.g /Game/Path/Asset) to export
	 * @param	ExportPath						The directory path to export to.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	virtual void ExportAssets(const TArray<FString>& AssetsToExport, const FString& ExportPath) const = 0;

	/**
	 * Exports the specified objects to file.
	 *
	 * @param	AssetsToExport					List of assets to export 
	 * @param	ExportPath						The directory path to export to.
	 */
	virtual void ExportAssets(const TArray<UObject*>& AssetsToExport, const FString& ExportPath) const = 0;
	
	/**
	 * Exports the specified objects to file. First prompting the user to pick an export directory and optionally prompting the user to pick a unique directory per file
	 *
	 * @param	AssetsToExport					List of assets to export
	 * @param	ExportPath						The directory path to export to.
	 */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	virtual void ExportAssetsWithDialog(const TArray<FString>& AssetsToExport, bool bPromptForIndividualFilenames) const = 0;
	
	/**
	 * Exports the specified objects to file. First prompting the user to pick an export directory and optionally prompting the user to pick a unique directory per file
	 *
	 * @param	AssetsToExport					List of full asset names (e.g /Game/Path/Asset) to export
	 * @param	ExportPath						The directory path to export to.
	 */
	virtual void ExportAssetsWithDialog(const TArray<UObject*>& AssetsToExport, bool bPromptForIndividualFilenames) const = 0;

	/** Creates a unique package and asset name taking the form InBasePackageName+InSuffix */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	virtual void CreateUniqueAssetName(const FString& InBasePackageName, const FString& InSuffix, FString& OutPackageName, FString& OutAssetName) const = 0;

	/** Returns true if the specified asset uses a stock thumbnail resource */
	virtual bool AssetUsesGenericThumbnail(const FAssetData& AssetData) const = 0;

	/**
	 * Try to diff the local version of an asset against the latest one from the depot
	 *
	 * @param InObject - The object we want to compare against the depot
	 * @param InPackagePath - The fullpath to the package
	 * @param InPackageName - The name of the package
	 */
	virtual void DiffAgainstDepot(UObject* InObject, const FString& InPackagePath, const FString& InPackageName) const = 0;

	/** Try and diff two assets using class-specific tool. Will do nothing if either asset is NULL, or they are not the same class. */
	virtual void DiffAssets(UObject* OldAsset, UObject* NewAsset, const struct FRevisionInfo& OldRevision, const struct FRevisionInfo& NewRevision) const = 0;

	/** Util for dumping an asset to a temporary text file. Returns absolute filename to temp file */
	virtual FString DumpAssetToTempFile(UObject* Asset) const = 0;

	/** Attempt to spawn Diff tool as external process
	 *
	 * @param DiffCommand -		Command used to launch the diff tool
	 * @param OldTextFilename - File path to original file
	 * @param NewTextFilename - File path to new file
	 * @param DiffArgs -		Any extra command line arguments (defaulted to empty)
	 *
	 * @return Returns true if the process has successfully been created.
	 */
	virtual bool CreateDiffProcess(const FString& DiffCommand, const FString& OldTextFilename, const FString& NewTextFilename, const FString& DiffArgs = FString("")) const = 0;

	/* Migrate packages to another game content folder */
	virtual void MigratePackages(const TArray<FName>& PackageNamesToMigrate) const = 0;

	/* Copy packages and dependencies to another folder */
	virtual void BeginAdvancedCopyPackages(const TArray<FName>& InputNamesToCopy, const FString& TargetPath) const = 0;

	/** Fix up references to the specified redirectors */
	virtual void FixupReferencers(const TArray<UObjectRedirector*>& Objects) const = 0;

	/** Expands any folders found in the files list, and returns a flattened list of destination paths and files.  Mirrors directory structure. */
	virtual void ExpandDirectories(const TArray<FString>& Files, const FString& DestinationPath, TArray<TPair<FString, FString>>& FilesAndDestinations) const = 0;

	/** Copies files after the final set of maps of sources and destinations was confirmed */
	virtual bool AdvancedCopyPackages(const FAdvancedCopyParams& CopyParams, const TArray<TMap<FString, FString>> PackagesAndDestinations) const = 0;

	/** Copies files after the flattened map of sources and destinations was confirmed */
	virtual bool AdvancedCopyPackages(const TMap<FString, FString>& SourceAndDestPackages, const bool bForceAutosave = false, const bool bCopyOverAllDestinationOverlaps = true) const = 0;

	/* Given a set of packages to copy, generate the map of those packages to destination filenames */
	virtual void GenerateAdvancedCopyDestinations(FAdvancedCopyParams& InParams, const TArray<FName>& InPackageNamesToCopy, const UAdvancedCopyCustomization* CopyCustomization, TMap<FString, FString>& OutPackagesAndDestinations) const = 0;

	/* Flattens the maps for each selected package into one complete map to pass to the final copy function while checking for collisions */
	virtual bool FlattenAdvancedCopyDestinations(const TArray<TMap<FString, FString>> PackagesAndDestinations, TMap<FString, FString>& FlattenedPackagesAndDestinations) const = 0;

	/* Validate the destinations for advanced copy once the map has been flattened */
	virtual bool ValidateFlattenedAdvancedCopyDestinations(const TMap<FString, FString>& FlattenedPackagesAndDestinations) const = 0;

	/* Find all the dependencies that also need to be copied in the advanced copy, mapping them to the file that depends on them and excluding any that don't pass the ARFilter stored on CopyParams */
	virtual void GetAllAdvancedCopySources(FName SelectedPackage, FAdvancedCopyParams& CopyParams, TArray<FName>& OutPackageNamesToCopy, TMap<FName, FName>& DependencyMap, const class UAdvancedCopyCustomization* CopyCustomization) const = 0;

	/* Given a complete set of copy parameters, which includes the selected package set, start the advanced copy process */
	virtual void InitAdvancedCopyFromCopyParams(FAdvancedCopyParams CopyParams) const = 0;

	/** Opens editor for assets */
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	virtual void OpenEditorForAssets(const TArray<UObject*>& Assets) const = 0;
};

UCLASS(transient)
class UAssetToolsHelpers : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Editor Scripting | Asset Tools")
	static TScriptInterface<IAssetTools> GetAssetTools();
};
