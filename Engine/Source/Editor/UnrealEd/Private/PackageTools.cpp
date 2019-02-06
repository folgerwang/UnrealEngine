// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "PackageTools.h"
#include "BlueprintCompilationManager.h"
#include "UObject/PackageReload.h"
#include "Misc/MessageDialog.h"
#include "Misc/Paths.h"
#include "Misc/Guid.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/FeedbackContext.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/UObjectHash.h"
#include "UObject/GCObjectScopeGuard.h"
#include "Serialization/ArchiveFindCulprit.h"
#include "Misc/PackageName.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "ISourceControlOperation.h"
#include "SourceControlOperations.h"
#include "ISourceControlProvider.h"
#include "ISourceControlModule.h"
#include "SourceControlHelpers.h"
#include "Editor.h"
#include "Dialogs/Dialogs.h"
#include "Toolkits/AssetEditorManager.h"

#include "ObjectTools.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "BusyCursor.h"

#include "FileHelpers.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"

#include "AssetRegistryModule.h"
#include "Logging/MessageLog.h"
#include "UObject/UObjectIterator.h"
#include "ComponentReregisterContext.h"
#include "Engine/Selection.h"
#include "Engine/GameEngine.h"
#include "Engine/LevelStreaming.h"
#include "Engine/MapBuildDataRegistry.h"

#include "ShaderCompiler.h"
#include "DistanceFieldAtlas.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "PackageTools"

DEFINE_LOG_CATEGORY_STATIC(LogPackageTools, Log, All);

/** State passed to RestoreStandaloneOnReachableObjects. */
UPackage* UPackageTools::PackageBeingUnloaded = nullptr;
TMap<UObject*, UObject*> UPackageTools::ObjectsThatHadFlagsCleared;
FDelegateHandle UPackageTools::ReachabilityCallbackHandle;

UPackageTools::UPackageTools(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		FCoreUObjectDelegates::OnPackageReloaded.AddStatic(&UPackageTools::HandlePackageReloaded);
	}
}

	/**
	 * Called during GC, after reachability analysis is performed but before garbage is purged.
	 * Restores RF_Standalone to objects in the package-to-be-unloaded that are still reachable.
	 */
	void UPackageTools::RestoreStandaloneOnReachableObjects()
	{
		check(GIsEditor);

		ForEachObjectWithOuter(PackageBeingUnloaded, [](UObject* Object)
			{
				if ( ObjectsThatHadFlagsCleared.Find(Object) )
				{
					Object->SetFlags(RF_Standalone);
				}
			},
			true, RF_NoFlags, EInternalObjectFlags::Unreachable);
	}

	/**
	 * Filters the global set of packages.
	 *
	 * @param	OutGroupPackages			The map that receives the filtered list of group packages.
	 * @param	OutPackageList				The array that will contain the list of filtered packages.
	 */
	void UPackageTools::GetFilteredPackageList(TSet<UPackage*>& OutFilteredPackageMap)
	{
		// The UObject list is iterated rather than the UPackage list because we need to be sure we are only adding
		// group packages that contain things the generic browser cares about.  The packages are derived by walking
		// the outer chain of each object.

		// Assemble a list of packages.  Only show packages that match the current resource type filter.
		for (UObject* Obj : TObjectRange<UObject>())
		{
			// This is here to hopefully catch a bit more info about a spurious in-the-wild problem which ultimately
			// crashes inside UObjectBaseUtility::GetOutermost(), which is called inside IsObjectBrowsable().
			checkf(Obj->IsValidLowLevel(), TEXT("GetFilteredPackageList: bad object found, address: %p, name: %s"), Obj, *Obj->GetName());

			// Make sure that we support displaying this object type
			bool bIsSupported = ObjectTools::IsObjectBrowsable( Obj );
			if( bIsSupported )
			{
				UPackage* ObjectPackage = Obj->GetOutermost();
				if( ObjectPackage != NULL )
				{
					OutFilteredPackageMap.Add( ObjectPackage );
				}
			}
		}
	}

	/**
	 * Fills the OutObjects list with all valid objects that are supported by the current
	 * browser settings and that reside withing the set of specified packages.
	 *
	 * @param	InPackages			Filters objects based on package.
	 * @param	OutObjects			[out] Receives the list of objects
	 * @param	bMustBeBrowsable	If specified, does a check to see if object is browsable. Defaults to true.
	 */
	void UPackageTools::GetObjectsInPackages( const TArray<UPackage*>* InPackages, TArray<UObject*>& OutObjects )
	{
		if (InPackages)
		{
			for (UPackage* Package : *InPackages)
			{
				ForEachObjectWithOuter(Package,[&OutObjects](UObject* Obj)
					{
						if (ObjectTools::IsObjectBrowsable(Obj))
						{
							OutObjects.Add(Obj);
						}
					});
			}
		}
		else
		{
			for (TObjectIterator<UObject> It; It; ++It)
			{
				UObject* Obj = *It;

				if (ObjectTools::IsObjectBrowsable(Obj))
				{
					OutObjects.Add(Obj);
				}
			}
		}
	}

	bool UPackageTools::HandleFullyLoadingPackages( const TArray<UPackage*>& TopLevelPackages, const FText& OperationText )
	{
		bool bSuccessfullyCompleted = true;

		// whether or not to suppress the ask to fully load message
		bool bSuppress = GetDefault<UEditorPerProjectUserSettings>()->bSuppressFullyLoadPrompt;

		// Make sure they are all fully loaded.
		bool bNeedsUpdate = false;
		for( int32 PackageIndex=0; PackageIndex<TopLevelPackages.Num(); PackageIndex++ )
		{
			UPackage* TopLevelPackage = TopLevelPackages[PackageIndex];
			check( TopLevelPackage );
			check( TopLevelPackage->GetOuter() == NULL );

			if( !TopLevelPackage->IsFullyLoaded() )
			{	
				// Ask user to fully load or suppress the message and just fully load
				if(bSuppress || EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, EAppReturnType::Yes, FText::Format(
					NSLOCTEXT("UnrealEd", "NeedsToFullyLoadPackageF", "Package {0} is not fully loaded. Do you want to fully load it? Not doing so will abort the '{1}' operation."),
					FText::FromString(TopLevelPackage->GetName()), OperationText ) ) )
				{
					// Fully load package.
					const FScopedBusyCursor BusyCursor;
					GWarn->BeginSlowTask( NSLOCTEXT("UnrealEd", "FullyLoadingPackages", "Fully loading packages"), true );
					TopLevelPackage->FullyLoad();
					GWarn->EndSlowTask();
					bNeedsUpdate = true;
				}
				// User declined abort operation.
				else
				{
					bSuccessfullyCompleted = false;
					UE_LOG(LogPackageTools, Log, TEXT("Aborting operation as %s was not fully loaded."),*TopLevelPackage->GetName());
					break;
				}
			}
		}

		// no need to refresh content browser here as UPackage::FullyLoad() already does this
		return bSuccessfullyCompleted;
	}
	
	/**
	 * Loads the specified package file (or returns an existing package if it's already loaded.)
	 *
	 * @param	InFilename	File name of package to load
	 *
	 * @return	The loaded package (or NULL if something went wrong.)
	 */
	UPackage* UPackageTools::LoadPackage( FString InFilename )
	{
		// Detach all components while loading a package.
		// This is necessary for the cases where the load replaces existing objects which may be referenced by the attached components.
		FGlobalComponentReregisterContext ReregisterContext;

		// record the name of this file to make sure we load objects in this package on top of in-memory objects in this package
		GEditor->UserOpenedFile = InFilename;

		// clear any previous load errors
		FFormatNamedArguments Arguments;
		Arguments.Add(TEXT("PackageName"), FText::FromString(InFilename));
		FMessageLog("LoadErrors").NewPage(FText::Format(LOCTEXT("LoadPackageLogPage", "Loading package: {PackageName}"), Arguments));

		UPackage* Package = ::LoadPackage( NULL, *InFilename, 0 );

		// display any load errors that happened while loading the package
		FEditorDelegates::DisplayLoadErrors.Broadcast();

		// reset the opened package to nothing
		GEditor->UserOpenedFile = FString();

		// If a script package was loaded, update the
		// actor browser in case a script package was loaded
		if ( Package != NULL )
		{
			if (Package->HasAnyPackageFlags(PKG_ContainsScript))
			{
				GEditor->BroadcastClassPackageLoadedOrUnloaded();
			}
		}

		return Package;
	}


	bool UPackageTools::UnloadPackages( const TArray<UPackage*>& TopLevelPackages )
	{
		FText ErrorMessage;
		bool bResult = UnloadPackages(TopLevelPackages, ErrorMessage);
		if(!ErrorMessage.IsEmpty())
		{
			FMessageDialog::Open( EAppMsgType::Ok, ErrorMessage );
		}

		return bResult;
	}


	bool UPackageTools::UnloadPackages( const TArray<UPackage*>& TopLevelPackages, FText& OutErrorMessage )
	{
		bool bResult = false;

		// Get outermost packages, in case groups were selected.
		TArray<UPackage*> PackagesToUnload;

		// Split the set of selected top level packages into packages which are dirty (and thus cannot be unloaded)
		// and packages that are not dirty (and thus can be unloaded).
		TArray<UPackage*> DirtyPackages;
		for ( int32 PackageIndex = 0 ; PackageIndex < TopLevelPackages.Num() ; ++PackageIndex )
		{
			UPackage* Package = TopLevelPackages[PackageIndex];
			if( Package != NULL )
			{
				if ( Package->IsDirty() )
				{
					DirtyPackages.Add( Package );
				}
				else
				{
					PackagesToUnload.AddUnique( Package->GetOutermost() ? Package->GetOutermost() : Package );
				}
			}
		}

		// Inform the user that dirty packages won't be unloaded.
		if ( DirtyPackages.Num() > 0 )
		{
			FString DirtyPackagesList;
			for ( int32 PackageIndex = 0 ; PackageIndex < DirtyPackages.Num() ; ++PackageIndex )
			{
				DirtyPackagesList += FString::Printf( TEXT("\n    %s"), *DirtyPackages[PackageIndex]->GetName() );
			}

			FFormatNamedArguments Args;
			Args.Add( TEXT("DirtyPackages"),FText::FromString( DirtyPackagesList ) );

			OutErrorMessage = FText::Format( NSLOCTEXT("UnrealEd", "UnloadDirtyPackagesList", "The following assets have been modified and cannot be unloaded:{DirtyPackages}\nSaving these assets will allow them to be unloaded."), Args );
		}

		if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
		{
			// Is the currently loaded world being unloaded? If so, we just reset the current world.
			// We also need to skip the build data package as that will also be destroyed by the call to CreateNewMapForEditing.
			if (PackagesToUnload.Contains(EditorWorld->GetOutermost()))
			{
				// Remove the world package from the unload list
				PackagesToUnload.Remove(EditorWorld->GetOutermost());

				// Remove the level build data package from the unload list as creating a new map will unload build data for the current world
				for (int32 LevelIndex = 0; LevelIndex < EditorWorld->GetNumLevels(); ++LevelIndex)
				{
					ULevel* Level = EditorWorld->GetLevel(LevelIndex);
					if (Level->MapBuildData)
					{
						PackagesToUnload.Remove(Level->MapBuildData->GetOutermost());
					}
				}

				// Remove any streaming levels from the unload list as creating a new map will unload streaming levels for the current world
				for (ULevelStreaming* EditorStreamingLevel : EditorWorld->GetStreamingLevels())
				{
					if (EditorStreamingLevel->IsLevelLoaded())
					{
						UPackage* EditorStreamingLevelPackage = EditorStreamingLevel->GetLoadedLevel()->GetOutermost();
						PackagesToUnload.Remove(EditorStreamingLevelPackage);
					}
				}

				// Unload the current world
				GEditor->CreateNewMapForEditing();
			}
		}

		if ( PackagesToUnload.Num() > 0 )
		{
			const FScopedBusyCursor BusyCursor;

			// Complete any load/streaming requests, then lock IO.
			FlushAsyncLoading();
			(*GFlushStreamingFunc)();

			// Remove potential references to to-be deleted objects from the GB selection set.
			GEditor->GetSelectedObjects()->DeselectAll();

			// Set the callback for restoring RF_Standalone post reachability analysis.
			// GC will call this function before purging objects, allowing us to restore RF_Standalone
			// to any objects that have not been marked RF_Unreachable.
			ReachabilityCallbackHandle = FCoreUObjectDelegates::PostReachabilityAnalysis.AddStatic(RestoreStandaloneOnReachableObjects);

			bool bScriptPackageWasUnloaded = false;

			GWarn->BeginSlowTask( NSLOCTEXT("UnrealEd", "Unloading", "Unloading"), true );

			// First add all packages to unload to the root set so they don't get garbage collected while we are operating on them
			TArray<UPackage*> PackagesAddedToRoot;
			for ( int32 PackageIndex = 0 ; PackageIndex < PackagesToUnload.Num() ; ++PackageIndex )
			{
				UPackage* Pkg = PackagesToUnload[PackageIndex];
				if ( !Pkg->IsRooted() )
				{
					Pkg->AddToRoot();
					PackagesAddedToRoot.Add(Pkg);
				}
			}

			// Now try to clean up assets in all packages to unload.
			for ( int32 PackageIndex = 0 ; PackageIndex < PackagesToUnload.Num() ; ++PackageIndex )
			{
				PackageBeingUnloaded = PackagesToUnload[PackageIndex];

				GWarn->StatusUpdate( PackageIndex, PackagesToUnload.Num(), FText::Format(NSLOCTEXT("UnrealEd", "Unloadingf", "Unloading {0}..."), FText::FromString(PackageBeingUnloaded->GetName()) ) );

				// Flush all pending render commands, as unloading the package may invalidate render resources.
				FlushRenderingCommands();

				// Close any open asset editors
				ForEachObjectWithOuter(PackageBeingUnloaded, [](UObject* Obj)
				{
					if (Obj->IsAsset())
					{
						FAssetEditorManager::Get().CloseAllEditorsForAsset(Obj);
					}
				}, false);

				PackageBeingUnloaded->bHasBeenFullyLoaded = false;
				PackageBeingUnloaded->ClearFlags(RF_WasLoaded);
				if ( PackageBeingUnloaded->HasAnyPackageFlags(PKG_ContainsScript) )
				{
					bScriptPackageWasUnloaded = true;
				}

				// Clear RF_Standalone flag from objects in the package to be unloaded so they get GC'd.
				{
					TArray<UObject*> ObjectsInPackage;
					GetObjectsWithOuter(PackageBeingUnloaded, ObjectsInPackage);
					for ( UObject* Object : ObjectsInPackage )
					{
						if (Object->HasAnyFlags(RF_Standalone))
						{
							Object->ClearFlags(RF_Standalone);
							ObjectsThatHadFlagsCleared.Add(Object, Object);
						}
					}
				}

				// Reset loaders
				ResetLoaders(PackageBeingUnloaded);

				// Collect garbage.
				CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
				
				if( PackageBeingUnloaded->IsDirty() )
				{
					// The package was marked dirty as a result of something that happened above (e.g callbacks in CollectGarbage).  
					// Dirty packages we actually care about unloading were filtered above so if the package becomes dirty here it should still be unloaded
					PackageBeingUnloaded->SetDirtyFlag(false);
				}

				// Cleanup.
				ObjectsThatHadFlagsCleared.Empty();
				PackageBeingUnloaded = NULL;
				bResult = true;
			}
			
			// Now remove from root all the packages we added earlier so they may be GCed if possible
			for ( int32 PackageIndex = 0 ; PackageIndex < PackagesAddedToRoot.Num() ; ++PackageIndex )
			{
				PackagesAddedToRoot[PackageIndex]->RemoveFromRoot();
			}
			PackagesAddedToRoot.Empty();

			GWarn->EndSlowTask();

			// Remove the post reachability callback.
			FCoreUObjectDelegates::PostReachabilityAnalysis.Remove(ReachabilityCallbackHandle);

			// Clear the standalone flag on metadata objects that are going to be GC'd below.
			// This resolves the circular dependency between metadata and packages.
			TArray<TWeakObjectPtr<UMetaData>> PackageMetaDataWithClearedStandaloneFlag;
			for ( UPackage* PackageToUnload : PackagesToUnload )
			{
				UMetaData* PackageMetaData = PackageToUnload ? PackageToUnload->MetaData : nullptr;
				if ( PackageMetaData && PackageMetaData->HasAnyFlags(RF_Standalone) )
				{
					PackageMetaData->ClearFlags(RF_Standalone);
					PackageMetaDataWithClearedStandaloneFlag.Add(PackageMetaData);
				}
			}

			CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

			// Restore the standalone flag on any metadata objects that survived the GC
			for ( const TWeakObjectPtr<UMetaData>& WeakPackageMetaData : PackageMetaDataWithClearedStandaloneFlag )
			{
				UMetaData* MetaData = WeakPackageMetaData.Get();
				if ( MetaData )
				{
					MetaData->SetFlags(RF_Standalone);
				}
			}

			// Update the actor browser if a script package was unloaded
			if ( bScriptPackageWasUnloaded )
			{
				GEditor->BroadcastClassPackageLoadedOrUnloaded();
			}
		}
		return bResult;
	}


	bool UPackageTools::ReloadPackages( const TArray<UPackage*>& TopLevelPackages )
	{
		FText ErrorMessage;
		const bool bResult = ReloadPackages(TopLevelPackages, ErrorMessage, EReloadPackagesInteractionMode::Interactive);
		
		if (!ErrorMessage.IsEmpty())
		{
			FMessageDialog::Open(EAppMsgType::Ok, ErrorMessage);
		}

		return bResult;
	}


	bool UPackageTools::ReloadPackages( const TArray<UPackage*>& TopLevelPackages, FText& OutErrorMessage, const bool bInteractive )
	{
		return ReloadPackages(TopLevelPackages, OutErrorMessage, bInteractive ? EReloadPackagesInteractionMode::Interactive : EReloadPackagesInteractionMode::AssumeNegative);
	}


	bool UPackageTools::ReloadPackages( const TArray<UPackage*>& TopLevelPackages, FText& OutErrorMessage, const EReloadPackagesInteractionMode InteractionMode )
	{
		bool bResult = false;

		FTextBuilder ErrorMessageBuilder;

		// Split the set of selected top level packages into packages which are dirty or in-memory (and thus cannot be reloaded) and packages that are not dirty (and thus can be reloaded).
		TArray<UPackage*> PackagesToReload;
		{
			TArray<UPackage*> DirtyPackages;
			TArray<UPackage*> InMemoryPackages;
			for (UPackage* TopLevelPackage : TopLevelPackages)
			{
				if (TopLevelPackage)
				{
					// Get outermost packages, in case groups were selected.
					UPackage* RealPackage = TopLevelPackage->GetOutermost() ? TopLevelPackage->GetOutermost() : TopLevelPackage;

					if (RealPackage->IsDirty())
					{
						DirtyPackages.AddUnique(RealPackage);
					}
					else if (RealPackage->HasAnyPackageFlags(PKG_InMemoryOnly))
					{
						InMemoryPackages.AddUnique(RealPackage);
					}
					else
					{
						PackagesToReload.AddUnique(RealPackage);
					}
				}

				// How should we handle locally dirty packages?
				if (DirtyPackages.Num() > 0)
				{
					EAppReturnType::Type ReloadDirtyPackagesResult = EAppReturnType::No;

					// Ask the user whether dirty packages should be reloaded.
					if (InteractionMode == EReloadPackagesInteractionMode::Interactive)
					{
						FTextBuilder ReloadDirtyPackagesMsgBuilder;
						ReloadDirtyPackagesMsgBuilder.AppendLine(NSLOCTEXT("UnrealEd", "ShouldReloadDirtyPackagesHeader", "The following packages have been modified:"));
						{
							ReloadDirtyPackagesMsgBuilder.Indent();
							for (UPackage* DirtyPackage : DirtyPackages)
							{
								ReloadDirtyPackagesMsgBuilder.AppendLine(DirtyPackage->GetFName());
							}
							ReloadDirtyPackagesMsgBuilder.Unindent();
						}
						ReloadDirtyPackagesMsgBuilder.AppendLine(NSLOCTEXT("UnrealEd", "ShouldReloadDirtyPackagesFooter", "Would you like to reload these packages? This will revert any changes you have made."));

						ReloadDirtyPackagesResult = FMessageDialog::Open(EAppMsgType::YesNo, ReloadDirtyPackagesMsgBuilder.ToText());
					}
					else if (InteractionMode == EReloadPackagesInteractionMode::AssumePositive)
					{
						ReloadDirtyPackagesResult = EAppReturnType::Yes;
					}

					if (ReloadDirtyPackagesResult == EAppReturnType::Yes)
					{
						for (UPackage* DirtyPackage : DirtyPackages)
						{
							DirtyPackage->SetDirtyFlag(false);
							PackagesToReload.AddUnique(DirtyPackage);
						}
						DirtyPackages.Reset();
					}
				}
			}

			// Inform the user that dirty packages won't be reloaded.
			if (DirtyPackages.Num() > 0)
			{
				if (!ErrorMessageBuilder.IsEmpty())
				{
					ErrorMessageBuilder.AppendLine();
				}

				ErrorMessageBuilder.AppendLine(NSLOCTEXT("UnrealEd", "Error_ReloadDirtyPackagesHeader", "The following packages have been modified and cannot be reloaded:"));
				{
					ErrorMessageBuilder.Indent();
					for (UPackage* DirtyPackage : DirtyPackages)
					{
						ErrorMessageBuilder.AppendLine(DirtyPackage->GetFName());
					}
					ErrorMessageBuilder.Unindent();
				}
				ErrorMessageBuilder.AppendLine(NSLOCTEXT("UnrealEd", "Error_ReloadDirtyPackagesFooter", "Saving these packages will allow them to be reloaded."));
			}

			// Inform the user that in-memory packages won't be reloaded.
			if (InMemoryPackages.Num() > 0)
			{
				if (!ErrorMessageBuilder.IsEmpty())
				{
					ErrorMessageBuilder.AppendLine();
				}

				ErrorMessageBuilder.AppendLine(NSLOCTEXT("UnrealEd", "Error_ReloadInMemoryPackagesHeader", "The following packages are in-memory only and cannot be reloaded:"));
				{
					ErrorMessageBuilder.Indent();
					for (UPackage* InMemoryPackage : InMemoryPackages)
					{
						ErrorMessageBuilder.AppendLine(InMemoryPackage->GetFName());
					}
					ErrorMessageBuilder.Unindent();
				}
			}
		}

		// Get the current world.
		TWeakObjectPtr<UWorld> CurrentWorld;
		if (GIsEditor)
		{
			if (UWorld* EditorWorld = GEditor->GetEditorWorldContext().World())
			{
				CurrentWorld = EditorWorld;
			}
		}
		else if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
		{
			if (UWorld* GameWorld = GameEngine->GetGameWorld())
			{
				CurrentWorld = GameWorld;
			}
		}

		// Check to see if we need to reload the current world.
		FName WorldNameToReload;
		TMap<FName, const UMapBuildDataRegistry*> LevelsToMapBuildData;
		TArray<ULevelStreaming*> RemovedStreamingLevels;
		if (UWorld* CurrentWorldPtr = CurrentWorld.Get())
		{
			// Is the current world being reloaded? If so, we just reset the current world and load it again at the end rather than let it go through ReloadPackage 
			// (which doesn't work for the current world due to some assumptions about worlds, and their lifetimes).
			// We also need to skip the build data package as that will also be destroyed by the transition.
			if (PackagesToReload.Contains(CurrentWorldPtr->GetOutermost()))
			{
				// Cache this so we can reload the world later
				WorldNameToReload = *CurrentWorldPtr->GetPathName();

				// Remove the world package from the reload list
				PackagesToReload.Remove(CurrentWorldPtr->GetOutermost());

				// Remove the level build data package from the reload list as creating a new map will unload build data for the current world
				for (int32 LevelIndex = 0; LevelIndex < CurrentWorldPtr->GetNumLevels(); ++LevelIndex)
				{
					ULevel* Level = CurrentWorldPtr->GetLevel(LevelIndex);
					if (Level->MapBuildData)
					{
						PackagesToReload.Remove(Level->MapBuildData->GetOutermost());
					}
				}

				// Remove any streaming levels from the reload list as creating a new map will unload streaming levels for the current world
				for (ULevelStreaming* StreamingLevel : CurrentWorldPtr->GetStreamingLevels())
				{
					if (StreamingLevel->IsLevelLoaded())
					{
						UPackage* StreamingLevelPackage = StreamingLevel->GetLoadedLevel()->GetOutermost();
						PackagesToReload.Remove(StreamingLevelPackage);
					}
				}

				// Unload the current world
				if (GIsEditor)
				{
					GEditor->CreateNewMapForEditing();
				}
				else if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
				{
					// Outside of the editor we need to keep the packages alive to stop the world transition from GC'ing them
					TGCObjectsScopeGuard<UPackage> KeepPackagesAlive(PackagesToReload);

					FString LoadMapError;
					GameEngine->LoadMap(GameEngine->GetWorldContextFromWorldChecked(CurrentWorldPtr), FURL(TEXT("/Engine/Maps/Templates/Template_Default")), nullptr, LoadMapError);
				}
			}
			// Cache the current map build data for the levels of the current world so we can see if they change due to a reload (we can skip this if reloading the current world).
			else
			{
				const TArray<ULevel*>& Levels = CurrentWorldPtr->GetLevels();
				for (int32 i = Levels.Num() - 1; i >= 0; --i)
				{
					ULevel* Level = Levels[i];
					if (PackagesToReload.Contains(Level->GetOutermost()))
					{
						for (ULevelStreaming* StreamingLevel : CurrentWorldPtr->GetStreamingLevels())
						{
							if (StreamingLevel->GetLoadedLevel() == Level)
							{
								CurrentWorldPtr->RemoveFromWorld(Level);
								StreamingLevel->RemoveLevelFromCollectionForReload();
								RemovedStreamingLevels.Add(StreamingLevel);
								break;
							}
						}
					}
					else
					{
						LevelsToMapBuildData.Add(Level->GetFName(), Level->MapBuildData);
					}
				}
			}
		}

		if (PackagesToReload.Num() > 0)
		{
			const FScopedBusyCursor BusyCursor;

			// We need to sort the packages to reload so that dependencies are reloaded before the assets that depend on them
			::SortPackagesForReload(PackagesToReload);

			// Remove potential references to to-be deleted objects from the global selection set.
			if (GIsEditor)
			{
				GEditor->GetSelectedObjects()->DeselectAll();
			}
			// Detach all components while loading a package.
			// This is necessary for the cases where the load replaces existing objects which may be referenced by the attached components.
			FGlobalComponentReregisterContext ReregisterContext;

			bool bScriptPackageWasReloaded = false;
			TArray<FReloadPackageData> PackagesToReloadData;
			PackagesToReloadData.Reserve(PackagesToReload.Num());
			for (UPackage* PackageToReload : PackagesToReload)
			{
				bScriptPackageWasReloaded |= PackageToReload->HasAnyPackageFlags(PKG_ContainsScript);
				PackagesToReloadData.Emplace(PackageToReload, LOAD_None);
			}

			TArray<UPackage*> ReloadedPackages;
			::ReloadPackages(PackagesToReloadData, ReloadedPackages, 500);

			TArray<UPackage*> FailedPackages;
			for (int32 PackageIndex = 0; PackageIndex < PackagesToReload.Num(); ++PackageIndex)
			{
				UPackage* ExistingPackage = PackagesToReload[PackageIndex];
				UPackage* ReloadedPackage = ReloadedPackages[PackageIndex];

				if (ReloadedPackage)
				{
					bScriptPackageWasReloaded |= ReloadedPackage->HasAnyPackageFlags(PKG_ContainsScript);
					bResult = true;
				}
				else
				{
					FailedPackages.Add(ExistingPackage);
				}
			}

			// Inform the user of any packages that failed to reload.
			if (FailedPackages.Num() > 0)
			{
				if (!ErrorMessageBuilder.IsEmpty())
				{
					ErrorMessageBuilder.AppendLine();
				}

				ErrorMessageBuilder.AppendLine(NSLOCTEXT("UnrealEd", "Error_ReloadFailedPackagesHeader", "The following packages failed to reload:"));
				{
					ErrorMessageBuilder.Indent();
					for (UPackage* FailedPackage : FailedPackages)
					{
						ErrorMessageBuilder.AppendLine(FailedPackage->GetFName());
					}
					ErrorMessageBuilder.Unindent();
				}
			}

			// Update the actor browser if a script package was reloaded.
			if (GIsEditor && bScriptPackageWasReloaded)
			{
				GEditor->BroadcastClassPackageLoadedOrUnloaded();
			}
		}

		// Load the previous world (if needed).
		if (!WorldNameToReload.IsNone())
		{
			if (GIsEditor)
			{
				TArray<FName> WorldNamesToReload;
				WorldNamesToReload.Add(WorldNameToReload);
				FAssetEditorManager::Get().OpenEditorsForAssets(WorldNamesToReload);
			}
			else if (UGameEngine* GameEngine = Cast<UGameEngine>(GEngine))
			{
				FString LoadMapError;
				GameEngine->LoadMap(GameEngine->GetWorldContextFromWorldChecked(GameEngine->GetGameWorld()), FURL(*WorldNameToReload.ToString()), nullptr, LoadMapError);
			}
		}
		// Update the rendering resources for the levels of the current world if their map build data has changed (we skip this if reloading the current world).
		else
		{
			if (LevelsToMapBuildData.Num() > 0)
			{
				UWorld* CurrentWorldPtr = CurrentWorld.Get();
				check(CurrentWorldPtr);

				for (int32 LevelIndex = 0; LevelIndex < CurrentWorldPtr->GetNumLevels(); ++LevelIndex)
				{
					ULevel* Level = CurrentWorldPtr->GetLevel(LevelIndex);
					const UMapBuildDataRegistry* OldMapBuildData = LevelsToMapBuildData.FindRef(Level->GetFName());

					if (OldMapBuildData && OldMapBuildData != Level->MapBuildData)
					{
						Level->ReleaseRenderingResources();
						Level->InitializeRenderingResources();
					}
				}
			}

			if (RemovedStreamingLevels.Num() > 0)
			{
				UWorld* CurrentWorldPtr = CurrentWorld.Get();
				check(CurrentWorldPtr);

				for (ULevelStreaming* StreamingLevel : RemovedStreamingLevels)
				{
					ULevel* NewLevel = StreamingLevel->GetLoadedLevel();
					CurrentWorldPtr->AddToWorld(NewLevel, StreamingLevel->LevelTransform, false);
					StreamingLevel->AddLevelToCollectionAfterReload();
				}
			}
		}

		OutErrorMessage = ErrorMessageBuilder.ToText();

		return bResult;
	}


	void UPackageTools::HandlePackageReloaded(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent)
	{
		static TSet<UBlueprint*> BlueprintsToRecompileThisBatch;

		if (InPackageReloadPhase == EPackageReloadPhase::PrePackageFixup)
		{
			GEngine->NotifyToolsOfObjectReplacement(InPackageReloadedEvent->GetRepointedObjects());

			// Notify any Blueprint assets that are about to be unloaded.
			ForEachObjectWithOuter(InPackageReloadedEvent->GetOldPackage(), [&](UObject* InObject)
			{
				if (InObject->IsAsset())
				{
					// Notify about any BP assets that are about to be unloaded
					if (UBlueprint* BP = Cast<UBlueprint>(InObject))
					{
						BP->ClearEditorReferences();
					}
				}
			}, false, RF_Transient, EInternalObjectFlags::PendingKill);
		}

		if (InPackageReloadPhase == EPackageReloadPhase::OnPackageFixup)
		{
			TMap<UClass*, UClass*> OldClassToNewClass;
			for (const auto& RepointedObjectPair : InPackageReloadedEvent->GetRepointedObjects())
			{
				UObject* OldObject = RepointedObjectPair.Key;
				UObject* NewObject = RepointedObjectPair.Value;

				if(OldObject && NewObject)
				{
					UClass* OldObjectAsClass = Cast<UClass>(OldObject);
					if(OldObjectAsClass)
					{
						UClass* NewObjectAsClass = Cast<UClass>(NewObject);
						if(ensureMsgf(NewObjectAsClass, TEXT("Class object replaced with non-class object: %s %s"), *(OldObject->GetName()), *(NewObject->GetName())))
						{
							OldClassToNewClass.Add(OldObjectAsClass, NewObjectAsClass);
						}
					}
				}
			}

			FBlueprintCompilationManager::ReparentHierarchies(OldClassToNewClass);

			for (const auto& RepointedObjectPair : InPackageReloadedEvent->GetRepointedObjects())
			{
				UObject* OldObject = RepointedObjectPair.Key;
				UObject* NewObject = RepointedObjectPair.Value;

				if (OldObject->IsAsset())
				{
					if (const UBlueprint* OldBlueprint = Cast<UBlueprint>(OldObject))
					{
						if (NewObject && CastChecked<UBlueprint>(NewObject)->GeneratedClass)
						{
							// Don't change the class on instances that are being thrown away by the reload code. If we update
							// the class and recompile the old class ::ReplaceInstancesOfClass will experience some crosstalk 
							// with the compiler (both trying to create objects of the same class in the same location):
							TArray<UObject*> OldInstances;
							GetObjectsOfClass(OldBlueprint->GeneratedClass, OldInstances, false);
							OldInstances.RemoveAllSwap(
								[](UObject* Obj){ return !Obj->HasAnyFlags(RF_NewerVersionExists); }
							);

							TSet<UObject*> InstancesToLeaveAlone(OldInstances);
							FReplaceInstancesOfClassParameters ReplaceInstancesParameters(OldBlueprint->GeneratedClass, CastChecked<UBlueprint>(NewObject)->GeneratedClass);
							ReplaceInstancesParameters.InstancesThatShouldUseOldClass = &InstancesToLeaveAlone;
							FBlueprintCompileReinstancer::ReplaceInstancesOfClassEx(ReplaceInstancesParameters);
						}
						else
						{
							// we failed to load the UBlueprint and/or it's GeneratedClass. Show a notification indicating that maps may need to be reloaded:
							FNotificationInfo Warning(
								FText::Format(
									NSLOCTEXT("UnrealEd", "Warning_FailedToLoadParentClass", "Failed to load ParentClass for {0}"),
									FText::FromName(OldObject->GetFName())
								)
							);
							Warning.ExpireDuration = 3.0f;
							FSlateNotificationManager::Get().AddNotification(Warning);
						}
					}
				}
			}
		}

		if (InPackageReloadPhase == EPackageReloadPhase::PostPackageFixup)
		{
			for (TWeakObjectPtr<UObject> ObjectReferencer : InPackageReloadedEvent->GetObjectReferencers())
			{
				UObject* ObjectReferencerPtr = ObjectReferencer.Get();
				if (!ObjectReferencerPtr)
				{
					continue;
				}

				FPropertyChangedEvent PropertyEvent(nullptr, EPropertyChangeType::Redirected);
				ObjectReferencerPtr->PostEditChangeProperty(PropertyEvent);

				// We need to recompile any Blueprints that had properties changed to make sure their generated class is up-to-date and has no lingering references to the old objects
				UBlueprint* BlueprintToRecompile = nullptr;
				if (UBlueprint* BlueprintReferencer = Cast<UBlueprint>(ObjectReferencerPtr))
				{
					BlueprintToRecompile = BlueprintReferencer;
				}
				else if (UClass* ClassReferencer = Cast<UClass>(ObjectReferencerPtr))
				{
					BlueprintToRecompile = Cast<UBlueprint>(ClassReferencer->ClassGeneratedBy);
				}
				else
				{
					BlueprintToRecompile = ObjectReferencerPtr->GetTypedOuter<UBlueprint>();
				}

				if (BlueprintToRecompile)
				{
					BlueprintsToRecompileThisBatch.Add(BlueprintToRecompile);
				}
			}
		}

		if (InPackageReloadPhase == EPackageReloadPhase::PreBatch)
		{
			// If this fires then ReloadPackages has probably bee called recursively :(
			check(BlueprintsToRecompileThisBatch.Num() == 0);

			// Flush all pending render commands, as reloading the package may invalidate render resources.
			FlushRenderingCommands();
		}

		if (InPackageReloadPhase == EPackageReloadPhase::PostBatchPreGC)
		{
			if (GEditor)
			{
				// Make sure we don't have any lingering transaction buffer references.
				GEditor->ResetTransaction(NSLOCTEXT("UnrealEd", "ReloadedPackage", "Reloaded Package"));
			}

			// Recompile any BPs that had their references updated
			if (BlueprintsToRecompileThisBatch.Num() > 0)
			{
				FScopedSlowTask CompilingBlueprintsSlowTask(BlueprintsToRecompileThisBatch.Num(), NSLOCTEXT("UnrealEd", "CompilingBlueprints", "Compiling Blueprints"));

				for (UBlueprint* BlueprintToRecompile : BlueprintsToRecompileThisBatch)
				{
					CompilingBlueprintsSlowTask.EnterProgressFrame(1.0f);

					FKismetEditorUtilities::CompileBlueprint(BlueprintToRecompile, EBlueprintCompileOptions::SkipGarbageCollection);
				}
			}
			BlueprintsToRecompileThisBatch.Reset();
		}

		if (InPackageReloadPhase == EPackageReloadPhase::PostBatchPostGC)
		{
			// Tick some things that aren't processed while we're reloading packages and can result in excessive memory usage if not periodically updated.
			if (GShaderCompilingManager)
			{
				GShaderCompilingManager->ProcessAsyncResults(true, false);
			}
			if (GDistanceFieldAsyncQueue)
			{
				GDistanceFieldAsyncQueue->ProcessAsyncTasks();
			}
		}
	}


	/**
	 * Wrapper method for multiple objects at once.
	 *
	 * @param	TopLevelPackages		the packages to be export
	 * @param	LastExportPath			the path that the user last exported assets to
	 * @param	FilteredClasses			if specified, set of classes that should be the only types exported if not exporting to single file
	 * @param	bUseProvidedExportPath	If true, use LastExportPath as the user's export path w/o prompting for a directory, where applicable
	 *
	 * @return	the path that the user chose for the export.
	 */
	FString UPackageTools::DoBulkExport(const TArray<UPackage*>& TopLevelPackages, FString LastExportPath, const TSet<UClass*>* FilteredClasses /* = NULL */, bool bUseProvidedExportPath/* = false*/ )
	{
		// Disallow export if any packages are cooked.
		if (HandleFullyLoadingPackages( TopLevelPackages, NSLOCTEXT("UnrealEd", "BulkExportE", "Bulk Export...") ) )
		{
			TArray<UObject*> ObjectsInPackages;
			GetObjectsInPackages(&TopLevelPackages, ObjectsInPackages);

			// See if any filtering has been requested. Objects can be filtered by class and/or localization filter.
			TArray<UObject*> FilteredObjects;
			if ( FilteredClasses )
			{
				// Present the user with a warning that only the filtered types are being exported
				FSuppressableWarningDialog::FSetupInfo Info( NSLOCTEXT("UnrealEd", "BulkExport_FilteredWarning", "Asset types are currently filtered within the Content Browser. Only objects of the filtered types will be exported."),
					LOCTEXT("BulkExport_FilteredWarning_Title", "Asset Filter in Effect"), "BulkExportFilterWarning" );
				Info.ConfirmText = NSLOCTEXT("ModalDialogs", "BulkExport_FilteredWarningConfirm", "Close");

				FSuppressableWarningDialog PromptAboutFiltering( Info );
				PromptAboutFiltering.ShowModal();
				
				for ( TArray<UObject*>::TConstIterator ObjIter(ObjectsInPackages); ObjIter; ++ObjIter )
				{
					UObject* CurObj = *ObjIter;

					// Only add the object if it passes all of the specified filters
					if ( CurObj && FilteredClasses->Contains( CurObj->GetClass() ) )
					{
						FilteredObjects.Add( CurObj );
					}
				}
			}

			// If a filtered set was provided, export the filtered objects array; otherwise, export all objects in the packages
			TArray<UObject*>& ObjectsToExport = FilteredClasses ? FilteredObjects : ObjectsInPackages;

			// Prompt the user about how many objects will be exported before proceeding.
			const bool bProceed = EAppReturnType::Yes == FMessageDialog::Open( EAppMsgType::YesNo, EAppReturnType::Yes, FText::Format(
				NSLOCTEXT("UnrealEd", "Prompt_AboutToBulkExportNItems_F", "About to bulk export {0} items.  Proceed?"), FText::AsNumber(ObjectsToExport.Num()) ) );
			if ( bProceed )
			{
				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");

				AssetToolsModule.Get().ExportAssets(ObjectsToExport, LastExportPath);
			}
		}

		return LastExportPath;
	}

	void UPackageTools::CheckOutRootPackages( const TArray<UPackage*>& Packages )
	{
		if (ISourceControlModule::Get().IsEnabled())
		{
			ISourceControlProvider& SourceControlProvider = ISourceControlModule::Get().GetProvider();

			// Update to the latest source control state.
			SourceControlProvider.Execute(ISourceControlOperation::Create<FUpdateStatus>(), Packages);

			TArray<FString> TouchedPackageNames;
			bool bCheckedSomethingOut = false;
			for( int32 PackageIndex = 0 ; PackageIndex < Packages.Num() ; ++PackageIndex )
			{
				UPackage* Package = Packages[PackageIndex];
				FSourceControlStatePtr SourceControlState = SourceControlProvider.GetState(Package, EStateCacheUsage::Use);
				if( SourceControlState.IsValid() && SourceControlState->CanCheckout() )
				{
					// The package is still available, so do the check out.
					bCheckedSomethingOut = true;
					TouchedPackageNames.Add(Package->GetName());
				}
				else
				{
					// The status on the package has changed to something inaccessible, so we have to disallow the check out.
					// Don't warn if the file isn't in the depot.
					if (SourceControlState.IsValid() && SourceControlState->IsSourceControlled())
					{			
						FMessageDialog::Open( EAppMsgType::Ok, NSLOCTEXT("UnrealEd", "Error_PackageStatusChanged", "Package can't be checked out - status has changed!") );
					}
				}
			}

			// Synchronize source control state if something was checked out.
			SourceControlProvider.Execute(ISourceControlOperation::Create<FCheckOut>(), SourceControlHelpers::PackageFilenames(TouchedPackageNames));
		}
	}

	/**
	 * Checks if the passed in path is in an external directory. I.E Ones not found automatically in the content directory
	 *
	 * @param	PackagePath	Path of the package to check, relative or absolute
	 * @return	true if PackagePath points to an external location
	 */
	bool UPackageTools::IsPackagePathExternal( const FString& PackagePath )
	{
		bool bIsExternal = true;
		TArray< FString > Paths;
		GConfig->GetArray( TEXT("Core.System"), TEXT("Paths"), Paths, GEngineIni );
	
		FString PackageFilename = FPaths::ConvertRelativePathToFull(PackagePath);

		// absolute path of the package that was passed in, without the actual name of the package
		FString PackageFullPath = FPaths::GetPath(PackageFilename);

		for(int32 pathIdx = 0; pathIdx < Paths.Num(); ++pathIdx)
		{ 
			FString AbsolutePathName = FPaths::ConvertRelativePathToFull(Paths[ pathIdx ]);

			// check if the package path is within the list of paths the engine searches.
			if( PackageFullPath.Contains( AbsolutePathName ) )
			{
				bIsExternal = false;
				break;
			}
		}

		return bIsExternal;
	}

	/**
	 * Checks if the passed in package's filename is in an external directory. I.E Ones not found automatically in the content directory
	 *
	 * @param	Package	The package to check
	 * @return	true if the package points to an external filename
	 */
	bool UPackageTools::IsPackageExternal(const UPackage& Package)
	{
		FString FileString;
		FPackageName::DoesPackageExist(Package.GetName(), NULL, &FileString);

		return IsPackagePathExternal( FileString );
	}

	bool UPackageTools::SavePackagesForObjects(const TArray<UObject*>& ObjectsToSave)
	{
		// Retrieve all dirty packages for the objects 
		TArray<UPackage*> PackagesToSave;
		for (UObject* Object : ObjectsToSave)
		{
			if (Object->GetOutermost()->IsDirty())
			{
				PackagesToSave.AddUnique(Object->GetOutermost());
			}
		}

		const bool bCheckDirty = false;
		const bool bPromptToSave = false;
		const FEditorFileUtils::EPromptReturnCode Return = FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, bCheckDirty, bPromptToSave);
		
		return (PackagesToSave.Num() > 0) && Return == FEditorFileUtils::EPromptReturnCode::PR_Success;
	}

	bool UPackageTools::IsSingleAssetPackage(const FString& PackageName)
	{
		FString PackageFileName;
		if ( FPackageName::DoesPackageExist(PackageName, NULL, &PackageFileName) )
		{
			return FPaths::GetExtension(PackageFileName, /*bIncludeDot=*/true) == FPackageName::GetAssetPackageExtension();
		}

		// If it wasn't found in the package file cache, this package does not yet
		// exist so it is assumed to be saved as a UAsset file.
		return true;
	}

	FString UPackageTools::SanitizePackageName (const FString& InPackageName)
	{
		FString SanitizedName;
		FString InvalidChars = INVALID_LONGPACKAGE_CHARACTERS;

		// See if the name contains invalid characters.
		FString Char;
		for( int32 CharIdx = 0; CharIdx < InPackageName.Len(); ++CharIdx )
		{
			Char = InPackageName.Mid(CharIdx, 1);

			if ( InvalidChars.Contains(*Char) )
			{
				SanitizedName += TEXT("_");
			}
			else
			{
				SanitizedName += Char;
			}
		}

		// Remove double-slashes
		SanitizedName.ReplaceInline(TEXT("//"), TEXT("/"));

		return SanitizedName;
	}

#undef LOCTEXT_NAMESPACE

// EOF
