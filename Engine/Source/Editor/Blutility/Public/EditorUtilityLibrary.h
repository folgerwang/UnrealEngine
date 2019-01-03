// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EditorUtilityLibrary.generated.h"

class AActor;
class UEditorPerProjectUserSettings;

// Expose editor utility functions to Blutilities 
UCLASS()
class BLUTILITY_API UEditorUtilityLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Development|Editor")
	static TArray<AActor*> GetSelectionSet();

	UFUNCTION(BlueprintCallable, Category = "Development|Editor")
	static void GetSelectionBounds(FVector& Origin, FVector& BoxExtent, float& SphereRadius);

	// Gets the set of currently selected assets
	UFUNCTION(BlueprintCallable, Category = "Development|Editor")
	static TArray<UObject*> GetSelectedAssets();

	// Renames an asset (cannot move folders)
	UFUNCTION(BlueprintCallable, Category = "Development|Editor")
	static void RenameAsset(UObject* Asset, const FString& NewName);

	/**
	* Attempts to find the actor specified by PathToActor in the current editor world
	* @param	PathToActor	The path to the actor (e.g. PersistentLevel.PlayerStart)
	* @return	A reference to the actor, or none if it wasn't found
	*/
	UFUNCTION(BlueprintPure, Category = "Development|Editor")
	AActor* GetActorReference(FString PathToActor);

};
