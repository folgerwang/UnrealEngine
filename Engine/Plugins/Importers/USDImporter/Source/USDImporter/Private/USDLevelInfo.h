// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "USDLevelInfo.generated.h"

UCLASS(Blueprintable)
class AUSDLevelInfo : public AActor
{
	GENERATED_UCLASS_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = USD, meta = (CallInEditor = "true"))
	void SaveUSD();

	UPROPERTY(EditAnywhere, Category = USD)
	FFilePath FilePath;

	UPROPERTY(EditAnywhere, Category = USD)
	TArray<FFilePath> SubLayerPaths;

	UPROPERTY(EditAnywhere, Category = USD)
	float FileScale;
};

