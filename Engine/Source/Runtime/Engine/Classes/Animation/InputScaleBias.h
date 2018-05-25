// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "AlphaBlend.h"
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

	FText GetFriendlyName(FText InFriendlyName) const;
};

USTRUCT(BlueprintType)
struct ENGINE_API FInputRange
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float Min;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float Max;

public:
	FInputRange()
		: Min(0.0f)
		, Max(1.0f)
	{
	}

	FVector2D ToVector2D() const { return FVector2D(Min, Max); } 
};

USTRUCT(BlueprintType)
struct ENGINE_API FInputScaleBiasClamp
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bMapRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "bMapRange"))
	FInputRange InRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "bMapRange"))
	FInputRange OutRange;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float Scale;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float Bias;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bClampResult;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "bClampResult"))
	float ClampMin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "bClampResult"))
	float ClampMax;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	bool bInterpResult;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "bInterpResult"))
	float InterpSpeedIncreasing;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings, meta = (EditCondition = "bInterpResult"))
	float InterpSpeedDecreasing;

	UPROPERTY(Transient)
	mutable float InterpolatedResult;

	UPROPERTY(Transient)
	mutable bool bInitialized;

public:
	FInputScaleBiasClamp()
		: Scale(1.0f)
		, Bias(0.0f)
		, bClampResult(false)
		, ClampMin(0.f)
		, ClampMax(1.f)
		, bInterpResult(false)
		, InterpSpeedIncreasing(10.f)
		, InterpSpeedDecreasing(10.f)
		, InterpolatedResult(0.f)
		, bInitialized(false)
	{
	}

	// Apply scale, bias, and clamp to value
	float ApplyTo(float Value, float InDeltaTime) const;
	void Reinitialize() { bInitialized = false; }
	FText GetFriendlyName(FText InFriendlyName) const;
};

// AnimNodes using an Alpha can choose how it is driven.
UENUM()
enum class EAnimAlphaInputType : uint8
{
	Float	UMETA(DisplayName = "Float Value"),
	Bool	UMETA(DisplayName = "Bool Value"),
	Curve	UMETA(DisplayName = "Anim Curve Value")
};

USTRUCT(BlueprintType)
struct ENGINE_API FInputAlphaBoolBlend
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float BlendInTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	float BlendOutTime;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	EAlphaBlendOption BlendOption;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Settings)
	UCurveFloat* CustomCurve;

	UPROPERTY(Transient)
	FAlphaBlend AlphaBlend;

	UPROPERTY(Transient)
	bool bInitialized;

	FInputAlphaBoolBlend()
		: BlendInTime(0.0f)
		, BlendOutTime(0.0f)
		, BlendOption(EAlphaBlendOption::Linear)
		, CustomCurve(nullptr)
		, bInitialized(false)
	{}

	float ApplyTo(bool bEnabled, float InDeltaTime);
	void Reinitialize() { bInitialized = false; }
};