// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "LiveCodingSettings.generated.h"

UENUM()
enum class ELiveCodingStartupMode : uint8
{
	Automatic,
	Manual,
};

UCLASS(config=EditorPerProjectUserSettings, meta=(DisplayName="Live Coding"))
class ULiveCodingSettings : public UObject
{
    GENERATED_BODY()

public:
    UPROPERTY(config, EditAnywhere, Category=Startup, Meta=(ConfigRestartRequired=true))
    ELiveCodingStartupMode StartupMode;
	
    UPROPERTY(config, EditAnywhere, Category=Startup, Meta=(ConfigRestartRequired=true))
    bool bShowConsole;

    UPROPERTY(config, EditAnywhere, Category=Modules, Meta=(ConfigRestartRequired=true))
    bool bIncludeEngineModules;

    UPROPERTY(config, EditAnywhere, Category=Modules, Meta=(ConfigRestartRequired=true))
    bool bIncludeEnginePluginModules;

    UPROPERTY(config, EditAnywhere, Category=Modules, Meta=(ConfigRestartRequired=true))
    bool bIncludeProjectModules;

    UPROPERTY(config, EditAnywhere, Category=Modules, Meta=(ConfigRestartRequired=true))
    bool bIncludeProjectPluginModules;

    UPROPERTY(config, EditAnywhere, Category=Modules, Meta=(ConfigRestartRequired=true))
    TArray<FName> IncludeSpecificModules;

    UPROPERTY(config, EditAnywhere, Category=Modules, Meta=(ConfigRestartRequired=true))
    TArray<FName> ExcludeSpecificModules;

	ULiveCodingSettings(const FObjectInitializer& Initializer);
};
