// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LiveCodingSettings.generated.h"

UENUM()
enum class ELiveCodingStartupMode : uint8
{
	Automatic UMETA(DisplayName = "Start automatically and show console"),
	AutomaticButHidden UMETA(DisplayName = "Start automatically but hide console until summoned"),
	Manual UMETA(DisplayName = "Manual"),
};

UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Live Coding"))
class ULiveCodingSettings : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(config, EditAnywhere, Category=General, Meta=(DisplayName="Enable Live Coding"))
    bool bEnabled;

    UPROPERTY(config, EditAnywhere, Category=General, Meta=(ConfigRestartRequired=true, EditCondition="bEnabled"))
    ELiveCodingStartupMode Startup;

	UPROPERTY(config, EditAnywhere, Category=Modules, Meta=(ConfigRestartRequired=true, EditCondition="bEnabled"))
    bool bIncludeEngineModules;

    UPROPERTY(config, EditAnywhere, Category=Modules, Meta=(ConfigRestartRequired=true, EditCondition="bEnabled"))
    bool bIncludeEnginePluginModules;

    UPROPERTY(config, EditAnywhere, Category=Modules, Meta=(ConfigRestartRequired=true, EditCondition="bEnabled"))
    bool bIncludeProjectModules;

    UPROPERTY(config, EditAnywhere, Category=Modules, Meta=(ConfigRestartRequired=true, EditCondition="bEnabled"))
    bool bIncludeProjectPluginModules;

    UPROPERTY(config, EditAnywhere, Category=Modules, Meta=(ConfigRestartRequired=true, EditCondition="bEnabled"))
    TArray<FName> IncludeSpecificModules;

    UPROPERTY(config, EditAnywhere, Category=Modules, Meta=(ConfigRestartRequired=true, EditCondition="bEnabled"))
    TArray<FName> ExcludeSpecificModules;

	ULiveCodingSettings(const FObjectInitializer& Initializer);
};
