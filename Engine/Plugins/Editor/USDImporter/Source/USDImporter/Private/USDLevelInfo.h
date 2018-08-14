// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "USDLevelInfo.generated.h"

UCLASS(Blueprintable)
class AUSDLevelInfo : public AActor
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, Category = USD)
	FFilePath FilePath;

	UPROPERTY(EditAnywhere, Category = USD)
	FFilePath BaseFilePath;

	UPROPERTY(EditAnywhere, Category = USD)
	float FileScale;
};

