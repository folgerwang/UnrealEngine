// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetData.h"

#include "ShotgunEngine.generated.h"

class AActor;

USTRUCT(Blueprintable)
struct FShotgunMenuItem
{
	GENERATED_BODY()

public:
	// Command name for internal use
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Python)
	FString Name;

	// Text to display in the menu
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Python)
	FString Title;

	// Description text for the tooltip
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Python)
	FString Description;

	// Menu item type to help interpret the command
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Python)
	FString Type;
};


/**
 * Wrapper for the Python Shotgun Engine
 * The functions are implemented in Python by a class that derives from this one
 */
UCLASS(Blueprintable)
class UShotgunEngine : public UObject
{
	GENERATED_BODY()

public:
	// Get the instance of the Python Shotgun Engine
	UFUNCTION(BlueprintCallable, Category = Python)
	static UShotgunEngine* GetInstance();

	// Callback for when the Python Shotgun Engine has finished initialization
	UFUNCTION(BlueprintCallable, Category = Python)
	void OnEngineInitialized() const;

	// Get the available Shotgun commands from the Python Shotgun Engine
	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	TArray<FShotgunMenuItem> GetShotgunMenuItems() const;

	// Execute a Shotgun command by name in the Python Shotgun Engine
	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	void ExecuteCommand(const FString& CommandName) const;

	// Shut down the Python Shotgun Engine
	UFUNCTION(BlueprintImplementableEvent, Category = Python)
	void Shutdown() const;

	// Set the selected objects that will be used to determine the Shotgun Engine context and execute Shotgun commands
	void SetSelection(const TArray<FAssetData>* InSelectedAssets, const TArray<AActor*>* InSelectedActors);

	// Get the assets that are referenced by the given Actor
	UFUNCTION(BlueprintCallable, Category = Python)
	TArray<UObject*> GetReferencedAssets(const AActor* Actor) const;

	// Get the root path for the Shotgun work area
	UFUNCTION(BlueprintCallable, Category = Python)
	static FString GetShotgunWorkDir();

	// Selected assets to be used for Shotgun commands
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Python)
	TArray<FAssetData> SelectedAssets;

	// Selected actors to be used for Shotgun commands
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Python)
	TArray<AActor*> SelectedActors;
};
