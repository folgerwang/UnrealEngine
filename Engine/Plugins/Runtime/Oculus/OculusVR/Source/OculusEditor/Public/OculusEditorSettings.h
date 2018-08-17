// Fill out your copyright notice in the Description page of Project Settings.

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
};
