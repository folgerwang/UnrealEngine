// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputScaleBias.generated.h"

// Input scaling struct
USTRUCT(BlueprintType)
struct ENGINE_API FInputScaleBias
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	float Scale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Settings)
	float Bias;

public:
	FInputScaleBias()
		: Scale(1.0f)
		, Bias(0.0f)
	{
	}

	// Apply scale and bias to value
	float ApplyTo(float Value) const;
};

USTRUCT(BlueprintType)
struct ENGINE_API FInputScaleBiasClamp
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float Scale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float Bias;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bClampResult;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float ClampMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float ClampMax;

public:
	FInputScaleBiasClamp()
		: Scale(1.0f)
		, Bias(0.0f)
		, bClampResult(false)
		, ClampMin(0.f)
		, ClampMax(1.f)
	{
	}

	// Apply scale, bias, and clamp to value
	float ApplyTo(float Value) const;
};
