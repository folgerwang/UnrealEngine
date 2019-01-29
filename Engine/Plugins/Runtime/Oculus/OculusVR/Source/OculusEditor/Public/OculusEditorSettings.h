// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "OculusEditorSettings.generated.h"

UENUM()
enum class EOculusPlatform : uint8
{
	PC UMETA(DisplayName="PC"),
	Mobile UMETA(DisplayName="Mobile"),
	Length UMETA(DisplayName="Invalid")
};

/**
 * 
 */
UCLASS(config=Editor)
class OCULUSEDITOR_API UOculusEditorSettings : public UObject
{
	GENERATED_BODY()

public:
	UOculusEditorSettings();

	UPROPERTY(config, EditAnywhere, Category = Oculus)
	TMap<FName, bool> PerfToolIgnoreList;
	
	UPROPERTY(config, EditAnywhere, Category = Oculus)
	EOculusPlatform PerfToolTargetPlatform;

	UPROPERTY(globalconfig, EditAnywhere, Category = Oculus)
	bool bAddMenuOption;
};
