// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "EditorUtilityContext.generated.h"

class AActor;
class UEditorPerProjectUserSettings;

// Expose editor utility functions to Blutilities 
UCLASS(config = EditorPerProjectUserSettings)
class UEditorUtilityContext : public UObject
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(config)
	TArray<FSoftObjectPath> LoadedUIs;
};
