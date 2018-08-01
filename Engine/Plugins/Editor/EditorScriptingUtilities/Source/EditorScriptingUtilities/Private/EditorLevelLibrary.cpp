// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "EditorLevelLibrary.h"

#include "EditorScriptingUtils.h"

#include "ActorEditorUtils.h"
#include "AssetRegistryModule.h"
#include "Components/MeshComponent.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "EngineUtils.h"
#include "Engine/Brush.h"
#include "Engine/Selection.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "IContentBrowserSingleton.h"
#include "IMeshMergeUtilities.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet2/ComponentEditorUtils.h"
#include "Layers/ILayers.h"
#include "LevelEditorViewport.h"
#include "Engine/MapBuildDataRegistry.h"
#include "MeshMergeModule.h"
#include "ScopedTransaction.h"
#include "UnrealEdGlobals.h"

#define LOCTEXT_NAMESPACE "EditorLevelLibrary"

/**
 *
 * Editor Scripting | Utilities
 *
 **/
namespace InternalEditorLevelLibrary
{
	template<class T>
	bool IsEditorLevelActor(T* Actor)
	{
		bool bResult = false;
		if (Actor && !Actor->IsPendingKill())
		{
			UWorld* World = Actor->GetWorld();
			if (World && World->WorldType == EWorldType::Editor)
			{
				bResult = true;
			}
		}
		return bResult;
	}

	UWorld* GetEditorWorld()
	{
		return GEditor ? GEditor->GetEditorWorldContext(false).World() : nullptr;
	}

	template<class T>
	TArray<T*> GetAllLoadedObjects()
	{
		TArray<T*> Result;

		if(!EditorScriptingUtils::CheckIfInEditorAndPIE())
		{
			return Result;
		}

		const EObjectFlags ExcludeFlags = RF_ClassDefaultObject;
		for (TObjectIterator<T> It(ExcludeFlags, true, EInternalObjectFlags::PendingKill); It; ++It)
		{
			T* Obj = *It;
			if (InternalEditorLevelLibrary::IsEditorLevelActor(Obj))
			{
				Result.Add(Obj);
			}
		}

		return Result;
	}
}

TArray<AActor*> UEditorLevelLibrary::GetAllLevelActors()
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);
	TArray<AActor*> Result;

	if (EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		//Default iterator only iterates over active levels.
		const EActorIteratorFlags Flags = EActorIteratorFlags::SkipPendingKill;
		for (TActorIterator<AActor> It(GetEditorWorld(), AActor::StaticClass(), Flags); It; ++It)
		{
			AActor* Actor = *It;
			if (Actor->IsEditable() &&
				Actor->IsListedInSceneOutliner() &&					// Only add actors that are allowed to be selected and drawn in editor
				!Actor->IsTemplate() &&								// Should never happen, but we never want CDOs
				!Actor->HasAnyFlags(RF_Transient) &&				// Don't add transient actors in non-play worlds
				!FActorEditorUtils::IsABuilderBrush(Actor) &&		// Don't add the builder brush
				!Actor->IsA(AWorldSettings::StaticClass()))			// Don't add the WorldSettings actor, even though it is technically editable
			{
				Result.Add(*It);
			}
		}
	}

	return Result;
}

TArray<UActorComponent*> UEditorLevelLibrary::GetAllLevelActorsComponents()
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return TArray<UActorComponent*>();
	}

	return InternalEditorLevelLibrary::GetAllLoadedObjects<UActorComponent>();
}

TArray<AActor*> UEditorLevelLibrary::GetSelectedLevelActors()
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TArray<AActor*> Result;
	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return Result;
	}

	for (FSelectionIterator Iter(*GEditor->GetSelectedActors()); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (InternalEditorLevelLibrary::IsEditorLevelActor(Actor))
		{
			Result.Add(Actor);
		}
	}

	return Result;
}

void UEditorLevelLibrary::SetSelectedLevelActors(const TArray<class AActor*>& ActorsToSelect)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TArray<AActor*> Result;
	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return;
	}

	if (GEdSelectionLock)
	{
		UE_LOG(LogEditorScripting, Warning, TEXT("SetSelectedLevelActors. The editor selection is currently locked."));
		return;
	}

	if (ActorsToSelect.Num() > 0)
	{
		GEditor->SelectNone(false, true, false);
		for (AActor* Actor : ActorsToSelect)
		{
			if (InternalEditorLevelLibrary::IsEditorLevelActor(Actor))
			{
				if (!GEditor->CanSelectActor(Actor, true))
				{
					UE_LOG(LogEditorScripting, Warning, TEXT("SetSelectedLevelActors. Can't select actor '%s'."), *Actor->GetName());
					continue;
				}
				GEditor->SelectActor(Actor, true, false);
			}
		}
		GEditor->NoteSelectionChange();
	}
	else
	{
		GEditor->SelectNone(true, true, false);
	}

	return;
}

namespace InternalEditorLevelLibrary
{
	AActor* SpawnActor(const TCHAR* MessageName, UObject* ObjToUse, FVector Location, FRotator Rotation)
	{
		if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
		{
			return nullptr;
		}

		if (!ObjToUse)
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. ObjToUse is not valid."), MessageName);
			return nullptr;
		}

		UWorld* World = InternalEditorLevelLibrary::GetEditorWorld();
		if (!World)
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. Can't spawn the actor because there is no world."), MessageName);
			return nullptr;
		}

		ULevel* DesiredLevel = World->GetCurrentLevel();
		if (!DesiredLevel)
		{
			UE_LOG(LogEditorScripting, Error, TEXT("%s. Can't spawn the actor because there is no Level."), MessageName);
			return nullptr;
		}

		GEditor->ClickLocation = Location;
		GEditor->ClickPlane = FPlane(Location, FVector::UpVector);

		const EObjectFlags NewObjectFlags = RF_Transactional;
		UActorFactory* FactoryToUse = nullptr;
		bool bSelectActors = true;
		TArray<AActor*> Actors = FLevelEditorViewportClient::TryPlacingActorFromObject(DesiredLevel, ObjToUse, bSelectActors, NewObjectFlags, FactoryToUse);

		if (Actors.Num() == 0 || Actors[0] == nullptr)
		{
			UE_LOG(LogEditorScripting, Warning, TEXT("%s. No actor was spawned."), MessageName);
			return nullptr;
		}

		for (AActor* Actor : Actors)
		{
			if (Actor)
			{
				Actor->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);
			}
		}

		return Actors[0];
	}
}

AActor* UEditorLevelLibrary::SpawnActorFromObject(UObject* ObjToUse, FVector Location, FRotator Rotation)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return nullptr;
	}

	if (!ObjToUse)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SpawnActorFromObject. ObjToUse is not valid."));
		return nullptr;
	}

	return InternalEditorLevelLibrary::SpawnActor(TEXT("SpawnActorFromObject"), ObjToUse, Location, Rotation);
}

AActor* UEditorLevelLibrary::SpawnActorFromClass(TSubclassOf<class AActor> ActorClass, FVector Location, FRotator Rotation)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return nullptr;
	}

	if (!ActorClass.Get())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SpawnActorFromClass. ActorClass is not valid."));
		return nullptr;
	}

	return InternalEditorLevelLibrary::SpawnActor(TEXT("SpawnActorFromClass"), ActorClass.Get(), Location, Rotation);
}

bool UEditorLevelLibrary::DestroyActor(class AActor* ToDestroyActor)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (!ToDestroyActor)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DestroyActor. ToDestroyActor is invalid."));
		return false;
	}

	if (!InternalEditorLevelLibrary::IsEditorLevelActor(ToDestroyActor))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DestroyActor. The Actor is not part of the world editor."));
		return false;
	}

	UWorld* World = InternalEditorLevelLibrary::GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("DestroyActor. Can't destroy the actor because there is no world."));
		return false;
	}

	//To avoid dangling gizmo after actor has been destroyed
	if (ToDestroyActor->IsSelected())
	{
		GEditor->SelectNone(true, true, false);
	}

	GEditor->Layers->DisassociateActorFromLayers(ToDestroyActor);
	return World->EditorDestroyActor(ToDestroyActor, true);
}

UWorld* UEditorLevelLibrary::GetEditorWorld()
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return nullptr;
	}

	return InternalEditorLevelLibrary::GetEditorWorld();
}

/**
 *
 * Editor Scripting | Level
 *
 **/

bool UEditorLevelLibrary::NewLevel(const FString& AssetPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	FString FailureReason;
	FString ObjectPath = EditorScriptingUtils::ConvertAnyPathToObjectPath(AssetPath, FailureReason);
	if (ObjectPath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("NewLevel. Failed to create the level. %s"), *FailureReason);
		return false;
	}

	if (!EditorScriptingUtils::IsAValidPathForCreateNewAsset(ObjectPath, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("NewLevel. Failed to validate the destination. %s"), *FailureReason);
		return false;
	}

	if (FPackageName::DoesPackageExist(ObjectPath, nullptr, nullptr))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("NewLevel. Failed to validate the destination '%s'. There's alreay an asset at the destination."), *ObjectPath);
		return false;
	}

	UWorld* World = GEditor->NewMap();
	if (!World)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("NewLevel. Failed to create the new level."));
		return false;
	}

	FString DestinationLongPackagePath = FPackageName::ObjectPathToPackageName(ObjectPath);
	if (!UEditorLoadingAndSavingUtils::SaveMap(World, DestinationLongPackagePath))
	{
		UE_LOG(LogEditorScripting, Warning, TEXT("NewLevel. Failed to save the new level."));
		return false;
	}

	return true;
}

bool UEditorLevelLibrary::NewLevelFromTemplate(const FString& AssetPath, const FString& TemplateAssetPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	FString FailureReason;
	FString ObjectPath = EditorScriptingUtils::ConvertAnyPathToObjectPath(AssetPath, FailureReason);
	if (ObjectPath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("NewLevelFromTemplate. Failed to create the level. %s"), *FailureReason);
		return false;
	}

	if (!EditorScriptingUtils::IsAValidPathForCreateNewAsset(ObjectPath, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("NewLevelFromTemplate. Failed to validate the destination. %s"), *FailureReason);
		return false;
	}

	// DuplicateAsset does it, but failed with a Modal
	if (FPackageName::DoesPackageExist(ObjectPath, nullptr, nullptr))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("NewLevelFromTemplate. Failed to validate the destination '%s'. There's alreay an asset at the destination."), *ObjectPath);
		return false;
	}

	FString TemplateObjectPath = EditorScriptingUtils::ConvertAnyPathToObjectPath(TemplateAssetPath, FailureReason);
	if (TemplateObjectPath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("NewLevelFromTemplate. Failed to create the level. %s"), *FailureReason);
		return false;
	}

	const bool bLoadAsTemplate = true;
	// Load the template map file - passes LoadAsTemplate==true making the
	// level load into an untitled package that won't save over the template
	if (!FEditorFileUtils::LoadMap(*TemplateObjectPath, bLoadAsTemplate))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("NewLevelFromTemplate. Failed to create the new level from template."));
		return false;
	}

	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("NewLevelFromTemplate. Failed to find the new created world."));
		return false;
	}

	FString DestinationLongPackagePath = FPackageName::ObjectPathToPackageName(ObjectPath);
	if (!UEditorLoadingAndSavingUtils::SaveMap(World, DestinationLongPackagePath))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("NewLevelFromTemplate. Failed to save the new level."));
		return false;
	}

	return true;
}

bool UEditorLevelLibrary::LoadLevel(const FString& AssetPath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	FString FailureReason;
	FString ObjectPath = EditorScriptingUtils::ConvertAnyPathToObjectPath(AssetPath, FailureReason);
	if (ObjectPath.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("LoadLevel. Failed to load level: %s"), *FailureReason);
		return false;
	}

	return UEditorLoadingAndSavingUtils::LoadMap(ObjectPath) != nullptr;
}

bool UEditorLevelLibrary::SaveCurrentLevel()
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	UWorld* World = InternalEditorLevelLibrary::GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SaveCurrentLevel. Can't save the current level because there is no world."));
		return false;
	}

	ULevel* Level = World->GetCurrentLevel();
	if (!Level)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SaveCurrentLevel. Can't save the level because there is no current level."));
		return false;
	}

	FString Filename = FEditorFileUtils::GetFilename(Level->OwningWorld);
	if (Filename.Len() == 0)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SaveCurrentLevel. Can't save the level because it doesn't have a filename. Use EditorLoadingAndSavingUtils."));
		return false;
	}

	TArray<UPackage*> MapPackages;
	MapPackages.Add(Level->GetOutermost());

	if (Level->MapBuildData)
	{
		MapPackages.AddUnique(Level->MapBuildData->GetOutermost());
	}

	// Checkout without a prompt
	TArray<UPackage*>* PackagesCheckedOut = nullptr;
	const bool bErrorIfAlreadyCheckedOut = false;
	FEditorFileUtils::CheckoutPackages(MapPackages, PackagesCheckedOut, bErrorIfAlreadyCheckedOut);

	return FEditorFileUtils::SaveLevel(Level);
}

bool UEditorLevelLibrary::SaveAllDirtyLevels()
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	UWorld* World = InternalEditorLevelLibrary::GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SaveAllDirtyLevels. Can't save the current level because there is no world."));
		return false;
	}

	TArray<UPackage*> DirtyMapPackages;
	TArray<ULevel*> DirtyLevels;
	for (ULevel* Level : World->GetLevels())
	{
		if (Level)
		{
			UPackage* OutermostPackage = Level->GetOutermost();
			if (OutermostPackage->IsDirty())
			{
				FString Filename = FEditorFileUtils::GetFilename(Level->OwningWorld);
				if (Filename.Len() == 0)
				{
					UE_LOG(LogEditorScripting, Warning, TEXT("SaveAllDirtyLevels. Can't save the level '%s' because it doesn't have a filename. Use EditorLoadingAndSavingUtils."), *OutermostPackage->GetName());
					continue;
				}

				DirtyLevels.Add(Level);
				DirtyMapPackages.Add(OutermostPackage);

				if (Level->MapBuildData)
				{
					UPackage* BuiltDataPackage = Level->MapBuildData->GetOutermost();
					if (BuiltDataPackage->IsDirty() && BuiltDataPackage != OutermostPackage)
					{
						DirtyMapPackages.Add(BuiltDataPackage);
					}
				}
			}
		}
	}

	bool bAllSaved = true;
	if (DirtyMapPackages.Num() > 0)
	{
		// Checkout without a prompt
		TArray<UPackage*>* PackagesCheckedOut = nullptr;
		const bool bErrorIfAlreadyCheckedOut = false;
		FEditorFileUtils::CheckoutPackages(DirtyMapPackages, PackagesCheckedOut, bErrorIfAlreadyCheckedOut);

		for (ULevel* Level : DirtyLevels)
		{
			bool bSaved = FEditorFileUtils::SaveLevel(Level);
			if (!bSaved)
			{
				UE_LOG(LogEditorScripting, Warning, TEXT("SaveAllDirtyLevels. Can't save the level '%s'."), *World->GetOutermost()->GetName());
				bAllSaved = false;
			}
		}
	}
	else
	{
		UE_LOG(LogEditorScripting, Log, TEXT("SaveAllDirtyLevels. There is no dirty level."));
	}

	return bAllSaved;
}

bool UEditorLevelLibrary::SetCurrentLevelByName(FName LevelName)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	if (LevelName == NAME_None)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("SetCurrentLevel. LevelName is invalid."));
		return false;
	}

	UWorld* World = InternalEditorLevelLibrary::GetEditorWorld();
	if (!World)
	{
		UE_LOG(LogEditorScripting, Warning, TEXT("SetCurrentLevel. Can't set the current level because there is no world."));
		return false;
	}

	bool bLevelFound = false;
	const TArray<ULevel*>& AllLevels = World->GetLevels();
	if (AllLevels.Num() > 0)
	{
		FString LevelNameStr = LevelName.ToString();
		for (ULevel* Level : AllLevels)
		{
			if (FPackageName::GetShortName(Level->GetOutermost()) == LevelNameStr)
			{
				// SetCurrentLevel return true only if the level is changed and it's not the same as the current.
				//For UEditorLevelLibrary, always return true.
				World->SetCurrentLevel(Level);
				bLevelFound = true;
				break;
			}
		}
	}

	return bLevelFound;
}

/**
 *
 * Editor Scripting | DataPrep
 *
 **/
namespace InternalEditorLevelLibrary
{
	template<typename ArrayType>
	int32 ReplaceMaterials(ArrayType& Array, UMaterialInterface* MaterialToBeReplaced, UMaterialInterface* NewMaterial)
	{
		//Would use FObjectEditorUtils::SetPropertyValue, but Material are a special case. They need a lock and we need to use the SetMaterial function
		UProperty* MaterialProperty = FindFieldChecked<UProperty>(UMeshComponent::StaticClass(), GET_MEMBER_NAME_CHECKED(UMeshComponent, OverrideMaterials));
		TArray<UObject*, TInlineAllocator<16>> ObjectsThatChanged;
		int32 NumberOfChanges = 0;

		for (UMeshComponent* Component : Array)
		{
			const bool bIsClassDefaultObject = Component->HasAnyFlags(RF_ClassDefaultObject);
			if (!bIsClassDefaultObject)
			{
				const int32 NumberOfMaterial = Component->GetNumMaterials();
				for (int32 Index = 0; Index < NumberOfMaterial; ++Index)
				{
					if (Component->GetMaterial(Index) == MaterialToBeReplaced)
					{
						FEditPropertyChain PropertyChain;
						PropertyChain.AddHead(MaterialProperty);
						static_cast<UObject*>(Component)->PreEditChange(PropertyChain);

						// Set the material
						Component->SetMaterial(Index, NewMaterial);
						++NumberOfChanges;

						ObjectsThatChanged.Add(Component);
					}
				}
			}
		}

		// Route post edit change after all components have had their values changed.  This is to avoid
		// construction scripts from re-running in the middle of setting values and wiping out components we need to modify
		for (UObject* ObjectData : ObjectsThatChanged)
		{
			FPropertyChangedEvent PropertyEvent(MaterialProperty);
			ObjectData->PostEditChangeProperty(PropertyEvent);
		}

		return NumberOfChanges;
	}
}

void UEditorLevelLibrary::ReplaceMeshComponentsMaterials(const TArray<UMeshComponent*>& MeshComponents, UMaterialInterface* MaterialToBeReplaced, UMaterialInterface* NewMaterial)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return;
	}

	FScopedTransaction ScopedTransaction(LOCTEXT("ReplaceMeshComponentsMaterials", "Replace components materials"));

	int32 ChangeCounter = InternalEditorLevelLibrary::ReplaceMaterials(MeshComponents, MaterialToBeReplaced, NewMaterial);

	if (ChangeCounter > 0)
	{
		// Redraw viewports to reflect the material changes
		GEditor->RedrawLevelEditingViewports();
	}

	UE_LOG(LogEditorScripting, Log, TEXT("ReplaceMeshComponentsMaterials. %d material(s) changed occurred."), ChangeCounter);
}

void UEditorLevelLibrary::ReplaceMeshComponentsMaterialsOnActors(const TArray<AActor*>& Actors, UMaterialInterface* MaterialToBeReplaced, UMaterialInterface* NewMaterial)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return;
	}

	FScopedTransaction ScopedTransaction(LOCTEXT("ReplaceComponentUsedMaterial", "Replace components materials"));

	int32 ChangeCounter = 0;
	TInlineComponentArray<UMeshComponent*> ComponentArray;

	for (AActor* Actor : Actors)
	{
		if (Actor && !Actor->IsPendingKill())
		{
			Actor->GetComponents(ComponentArray);
			ChangeCounter += InternalEditorLevelLibrary::ReplaceMaterials(ComponentArray, MaterialToBeReplaced, NewMaterial);
		}
	}

	if (ChangeCounter > 0)
	{
		// Redraw viewports to reflect the material changes
		GEditor->RedrawLevelEditingViewports();
	}

	UE_LOG(LogEditorScripting, Log, TEXT("ReplaceMeshComponentsMaterialsOnActors. %d material(s) changed occurred."), ChangeCounter);
}

namespace InternalEditorLevelLibrary
{
	template<typename ArrayType>
	int32 ReplaceMeshes(const ArrayType& Array, UStaticMesh* MeshToBeReplaced, UStaticMesh* NewMesh)
	{
		//Would use FObjectEditorUtils::SetPropertyValue, but meshes are a special case. They need a lock and we need to use the SetMesh function
		UProperty* StaticMeshProperty = FindFieldChecked<UProperty>(UStaticMeshComponent::StaticClass(), "StaticMesh");
		TArray<UObject*, TInlineAllocator<16>> ObjectsThatChanged;
		int32 NumberOfChanges = 0;

		for (UStaticMeshComponent* Component : Array)
		{
			const bool bIsClassDefaultObject = Component->HasAnyFlags(RF_ClassDefaultObject);
			if (!bIsClassDefaultObject)
			{
				if (Component->GetStaticMesh() == MeshToBeReplaced)
				{
					FEditPropertyChain PropertyChain;
					PropertyChain.AddHead(StaticMeshProperty);
					static_cast<UObject*>(Component)->PreEditChange(PropertyChain);

					// Set the mesh
					Component->SetStaticMesh(NewMesh);
					++NumberOfChanges;

					ObjectsThatChanged.Add(Component);
				}
			}
		}

		// Route post edit change after all components have had their values changed.  This is to avoid
		// construction scripts from re-running in the middle of setting values and wiping out components we need to modify
		for (UObject* ObjectData : ObjectsThatChanged)
		{
			FPropertyChangedEvent PropertyEvent(StaticMeshProperty);
			ObjectData->PostEditChangeProperty(PropertyEvent);
		}

		return NumberOfChanges;
	}
}

void UEditorLevelLibrary::ReplaceMeshComponentsMeshes(const TArray<UStaticMeshComponent*>& MeshComponents, UStaticMesh* MeshToBeReplaced, UStaticMesh* NewMesh)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return;
	}

	FScopedTransaction ScopedTransaction(LOCTEXT("ReplaceMeshComponentsMaterials", "Replace components meshes"));

	int32 ChangeCounter = InternalEditorLevelLibrary::ReplaceMeshes(MeshComponents, MeshToBeReplaced, NewMesh);

	if (ChangeCounter > 0)
	{
		// Redraw viewports to reflect the material changes
		GEditor->RedrawLevelEditingViewports();
	}

	UE_LOG(LogEditorScripting, Log, TEXT("ReplaceMeshComponentsMeshes. %d mesh(es) changed occurred."), ChangeCounter);
}

void UEditorLevelLibrary::ReplaceMeshComponentsMeshesOnActors(const TArray<AActor*>& Actors, UStaticMesh* MeshToBeReplaced, UStaticMesh* NewMesh)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return;
	}

	FScopedTransaction ScopedTransaction(LOCTEXT("ReplaceMeshComponentsMaterials", "Replace components meshes"));

	int32 ChangeCounter = 0;
	TInlineComponentArray<UStaticMeshComponent*> ComponentArray;

	for (AActor* Actor : Actors)
	{
		if (Actor && !Actor->IsPendingKill())
		{
			Actor->GetComponents(ComponentArray);
			ChangeCounter += InternalEditorLevelLibrary::ReplaceMeshes(ComponentArray, MeshToBeReplaced, NewMesh);
		}
	}

	if (ChangeCounter > 0)
	{
		// Redraw viewports to reflect the material changes
		GEditor->RedrawLevelEditingViewports();
	}

	UE_LOG(LogEditorScripting, Log, TEXT("ReplaceMeshComponentsMeshesOnActors. %d mesh(es) changed occurred."), ChangeCounter);
}

TArray<class AActor*> UEditorLevelLibrary::ConvertActors(const TArray<class AActor*>& Actors, TSubclassOf<class AActor> ActorClass, const FString& StaticMeshPackagePath)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	TArray<class AActor*> Result;
	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return Result;
	}

	if (ActorClass.Get() == nullptr)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("ConvertActorWith. The ActorClass is not valid."));
		return Result;
	}

	FString PackagePath = StaticMeshPackagePath;
	if (!PackagePath.IsEmpty())
	{
		FString FailureReason;
		PackagePath = EditorScriptingUtils::ConvertAnyPathToLongPackagePath(PackagePath, FailureReason);
		if (PackagePath.IsEmpty())
		{
			UE_LOG(LogEditorScripting, Error, TEXT("ConvertActorWith. %s"), *FailureReason);
			return Result;
		}
	}

	TArray<class AActor*> ActorToConvert;
	ActorToConvert.Reserve(Actors.Num());
	for (AActor* Actor : Actors)
	{
		if (Actor == nullptr || Actor->IsPendingKill())
		{
			continue;
		}

		UWorld* ActorWorld = Actor->GetWorld();
		if (ActorWorld == nullptr)
		{
			UE_LOG(LogEditorScripting, Warning, TEXT("ConvertActorWith. %s is not in a world. The actor will be skipped."), *Actor->GetActorLabel());
			continue;
		}
		if (ActorWorld->WorldType != EWorldType::Editor)
		{
			UE_LOG(LogEditorScripting, Warning, TEXT("ConvertActorWith. %s is not in an editor world. The actor will be skipped."), *Actor->GetActorLabel());
			continue;
		}

		ULevel* CurrentLevel = Actor->GetLevel();
		if (CurrentLevel == nullptr)
		{
			UE_LOG(LogEditorScripting, Warning, TEXT("ConvertActorWith. %s must be in a valid level. The actor will be skipped."), *Actor->GetActorLabel());
			continue;
		}

		if (Cast<ABrush>(Actor) && PackagePath.Len() == 0)
		{
			UE_LOG(LogEditorScripting, Warning, TEXT("ConvertActorWith. %s is a Brush and not package path was provided. The actor will be skipped."), *Actor->GetActorLabel());
			continue;
		}

		ActorToConvert.Add(Actor);
	}

	if (ActorToConvert.Num() != 0)
	{
		const bool bUseSpecialCases = false; // Don't use special cases, they are a bit too exhaustive and create dialog
		GEditor->DoConvertActors(ActorToConvert, ActorClass.Get(), TSet<FString>(), bUseSpecialCases, StaticMeshPackagePath);
		Result.Reserve(GEditor->GetSelectedActorCount());
		for(auto Itt = GEditor->GetSelectedActorIterator(); Itt; ++Itt)
		{
			Result.Add(CastChecked<AActor>(*Itt));
		}
	}

	UE_LOG(LogEditorScripting, Log, TEXT("ConvertActorWith. %d convertions occurred."), Result.Num());
	return Result;
}

namespace InternalEditorLevelLibrary
{
	template<class TPrimitiveComponent>
	bool FindValidActorAndComponents(TArray<AStaticMeshActor*> ActorsToTest, TArray<AStaticMeshActor*>& OutValidActor, TArray<TPrimitiveComponent*>& OutPrimitiveComponent, FVector& OutAverageLocation, FString& OutFailureReason)
	{
		for (int32 Index = ActorsToTest.Num() - 1; Index >= 0; --Index)
		{
			if (ActorsToTest[Index] == nullptr || ActorsToTest[Index]->IsPendingKill())
			{
				ActorsToTest.RemoveAtSwap(Index);
			}
		}
		if (ActorsToTest.Num() < 2)
		{
			OutFailureReason = TEXT("A merge operation requires at least 2 Actors.");
			return false;
		}

		// All actors need to come from the same World
		UWorld* CurrentWorld = ActorsToTest[0]->GetWorld();
		if (CurrentWorld == nullptr)
		{
			OutFailureReason = TEXT("The actors were not in a valid world.");
			return false;
		}
		if (CurrentWorld->WorldType != EWorldType::Editor)
		{
			OutFailureReason = TEXT("The actors were not in an editor world.");
			return false;
		}

		ULevel* CurrentLevel = ActorsToTest[0]->GetLevel();
		if (CurrentLevel == nullptr)
		{
			OutFailureReason = TEXT("The actors were not in a valid level.");
			return false;
		}

		FVector PivotLocation = FVector::ZeroVector;

		OutPrimitiveComponent.Reset(ActorsToTest.Num());
		OutValidActor.Reset(ActorsToTest.Num());
		{
			bool bShowedDifferentLevelMessage = false;
			for (AStaticMeshActor* MeshActor : ActorsToTest)
			{
				if (MeshActor->GetWorld() != CurrentWorld)
				{
					OutFailureReason = TEXT("Some actors were not from the same world.");
					return false;
				}

				if (!bShowedDifferentLevelMessage && MeshActor->GetLevel() != CurrentLevel)
				{
					UE_LOG(LogEditorScripting, Log, TEXT("Not all actors are from the same level. The Actor will be created in the first level found."));
					bShowedDifferentLevelMessage = true;
				}

				PivotLocation += MeshActor->GetActorLocation();

				TInlineComponentArray<UStaticMeshComponent*> ComponentArray;
				MeshActor->GetComponents<UStaticMeshComponent>(ComponentArray);

				bool bActorIsValid = false;
				for (UStaticMeshComponent* MeshCmp : ComponentArray)
				{
					if (MeshCmp->GetStaticMesh() && MeshCmp->GetStaticMesh()->RenderData.IsValid())
					{
						bActorIsValid = true;
						OutPrimitiveComponent.Add(MeshCmp);
					}
				}

				//Actor needs at least one StaticMeshComponent to be considered valid
				if (bActorIsValid)
				{
					OutValidActor.Add(MeshActor);
				}
			}
		}

		if (OutValidActor.Num() < 2)
		{
			OutFailureReason = TEXT("A merge operation requires at least 2 valid Actors.");
			return false;
		}

		OutAverageLocation = PivotLocation / OutValidActor.Num();

		return true;
	}

	FName GenerateValidOwnerBasedComponentNameForNewOwner(UStaticMeshComponent* OriginalComponent, AActor* NewOwner)
	{
		check(OriginalComponent);
		check(OriginalComponent->GetOwner());
		check(NewOwner);

		//Find first valid name on new owner by incrementing internal index
		FName NewName = OriginalComponent->GetOwner()->GetFName();
		const int32 InitialNumber = NewName.GetNumber();
		while (FindObjectFast<UObject>(NewOwner, NewName) != nullptr)
		{
			uint32 NextNumber = NewName.GetNumber();
			if (NextNumber >= 0xfffffe)
			{
				NewName = NAME_None;
				break;
			}
			++NextNumber;
			NewName.SetNumber(NextNumber);
		}

		return NewName;
	}
}

AActor* UEditorLevelLibrary::JoinStaticMeshActors(const TArray<AStaticMeshActor*>& ActorsToMerge, const FEditorScriptingJoinStaticMeshActorsOptions& JoinOptions)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return nullptr;
	}

	TArray<AStaticMeshActor*> AllActors;
	TArray<UStaticMeshComponent*> AllComponents;
	FVector PivotLocation;
	FString FailureReason;
	if (!InternalEditorLevelLibrary::FindValidActorAndComponents(ActorsToMerge, AllActors, AllComponents, PivotLocation, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("JoinStaticMeshSctors failed. %s"), *FailureReason);
		return nullptr;
	}

	// Create the new Actor
	FActorSpawnParameters Params;
	Params.OverrideLevel = AllActors[0]->GetLevel();
	AActor* NewActor = AllActors[0]->GetWorld()->SpawnActor<AActor>(PivotLocation, FRotator::ZeroRotator, Params);
	if (!NewActor)
	{
		UE_LOG(LogEditorScripting, Error, TEXT("JoinStaticMeshSctors failed. Internal error while creating the join actor."));
		return nullptr;
	}

	if (!JoinOptions.NewActorLabel.IsEmpty())
	{
		NewActor->SetActorLabel(JoinOptions.NewActorLabel);
	}

	// Duplicate and attach all components to the new actors
	USceneComponent* NewRootComponent = NewObject<USceneComponent>(NewActor, TEXT("Root"));
	NewActor->SetRootComponent(NewRootComponent);
	NewRootComponent->SetMobility(EComponentMobility::Static);
	for (UStaticMeshComponent* ActorCmp : AllComponents)
	{
		FName NewName = NAME_None;
		if (JoinOptions.bRenameComponentsFromSource)
		{
			NewName = InternalEditorLevelLibrary::GenerateValidOwnerBasedComponentNameForNewOwner(ActorCmp, NewActor);
		}

		UStaticMeshComponent* NewComponent = DuplicateObject<UStaticMeshComponent>(ActorCmp, NewActor, NewName);
		NewActor->AddInstanceComponent(NewComponent);
		FTransform CmpTransform = ActorCmp->GetComponentToWorld();
		NewComponent->SetComponentToWorld(CmpTransform);
		NewComponent->AttachToComponent(NewRootComponent, FAttachmentTransformRules::KeepWorldTransform);
		NewComponent->RegisterComponent();
	}

	if (JoinOptions.bDestroySourceActors)
	{
		UWorld* World = AllActors[0]->GetWorld();
		for (AActor* Actor : AllActors)
		{
			GEditor->Layers->DisassociateActorFromLayers(Actor);
			World->EditorDestroyActor(Actor, true);
		}
	}

	//Select newly created actor
	GEditor->SelectNone(false, true, false);
	GEditor->SelectActor(NewActor, true, false);
	GEditor->NoteSelectionChange();

	UE_LOG(LogEditorScripting, Log, TEXT("JoinStaticMeshActors joined %d actors toghether in actor '%s'."), AllComponents.Num(), *NewActor->GetActorLabel());
	return NewActor;
}

bool UEditorLevelLibrary::MergeStaticMeshActors(const TArray<AStaticMeshActor*>& ActorsToMerge, const FEditorScriptingMergeStaticMeshActorsOptions& MergeOptions, AStaticMeshActor*& OutMergedActor)
{
	TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, true);

	OutMergedActor = nullptr;

	if (!EditorScriptingUtils::CheckIfInEditorAndPIE())
	{
		return false;
	}

	FString FailureReason;
	FString PackageName = EditorScriptingUtils::ConvertAnyPathToLongPackagePath(MergeOptions.BasePackageName, FailureReason);
	if (PackageName.IsEmpty())
	{
		UE_LOG(LogEditorScripting, Error, TEXT("MergeStaticMeshActors. Failed to convert the BasePackageName. %s"), *FailureReason);
		return false;
	}

	TArray<AStaticMeshActor*> AllActors;
	TArray<UPrimitiveComponent*> AllComponents;
	FVector PivotLocation;
	if (!InternalEditorLevelLibrary::FindValidActorAndComponents(ActorsToMerge, AllActors, AllComponents, PivotLocation, FailureReason))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("MergeStaticMeshActors failed. %s"), *FailureReason);
		return false;
	}

	//
	// See MeshMergingTool.cpp
	//
	const IMeshMergeUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();


	FVector MergedActorLocation;
	TArray<UObject*> CreatedAssets;
	const float ScreenAreaSize = TNumericLimits<float>::Max();
	MeshUtilities.MergeComponentsToStaticMesh(AllComponents, AllActors[0]->GetWorld(), MergeOptions.MeshMergingSettings, nullptr, nullptr, MergeOptions.BasePackageName, CreatedAssets, MergedActorLocation, ScreenAreaSize, true);

	UStaticMesh* MergedMesh = nullptr;
	if (!CreatedAssets.FindItemByClass(&MergedMesh))
	{
		UE_LOG(LogEditorScripting, Error, TEXT("MergeStaticMeshActors failed. No mesh was created."));
		return false;
	}

	FAssetRegistryModule& AssetRegistry = FModuleManager::Get().LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	for (UObject* Obj : CreatedAssets)
	{
		AssetRegistry.AssetCreated(Obj);
	}

	//Also notify the content browser that the new assets exists
	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	ContentBrowserModule.Get().SyncBrowserToAssets(CreatedAssets, true);

	// Place new mesh in the world
	if (MergeOptions.bSpawnMergedActor)
	{
		FActorSpawnParameters Params;
		Params.OverrideLevel = AllActors[0]->GetLevel();
		OutMergedActor = AllActors[0]->GetWorld()->SpawnActor<AStaticMeshActor>(MergedActorLocation, FRotator::ZeroRotator, Params);
		if (!OutMergedActor)
		{
			UE_LOG(LogEditorScripting, Error, TEXT("MergeStaticMeshActors failed. Internal error while creating the merged actor."));
			return false;
		}

		OutMergedActor->GetStaticMeshComponent()->SetStaticMesh(MergedMesh);
		OutMergedActor->SetActorLabel(MergeOptions.NewActorLabel);
		AllActors[0]->GetWorld()->UpdateCullDistanceVolumes(OutMergedActor, OutMergedActor->GetStaticMeshComponent());
	}

	// Remove source actors
	if (MergeOptions.bDestroySourceActors)
	{
		UWorld* World = AllActors[0]->GetWorld();
		for (AActor* Actor : AllActors)
		{
			GEditor->Layers->DisassociateActorFromLayers(Actor);
			World->EditorDestroyActor(Actor, true);
		}
	}

	//Select newly created actor
	GEditor->SelectNone(false, true, false);
	GEditor->SelectActor(OutMergedActor, true, false);
	GEditor->NoteSelectionChange();

	return true;
}

#undef LOCTEXT_NAMESPACE
