// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MaterialEditor/DEditorParameterValue.h"

#include "DEditorScalarParameterValue.generated.h"

USTRUCT()
struct FScalarParameterAtlasData
{
	GENERATED_BODY()
public:
	UPROPERTY(Transient)
	bool bIsUsedAsAtlasPosition;

	UPROPERTY(Transient)
	TSoftObjectPtr<class UCurveLinearColor> Curve;

	UPROPERTY(Transient)
	TSoftObjectPtr<class UCurveLinearColorAtlas> Atlas;
};

UCLASS(hidecategories=Object, collapsecategories, editinlinenew)
class UNREALED_API UDEditorScalarParameterValue : public UDEditorParameterValue
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, Category=DEditorScalarParameterValue)
	float ParameterValue;

	float SliderMin;
	float SliderMax;

	UPROPERTY(Transient)
	FScalarParameterAtlasData AtlasData;

};

