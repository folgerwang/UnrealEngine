// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PropertyValue.h"

#include "PropertyValueTransform.generated.h"


UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValueTransform : public UPropertyValue
{
	GENERATED_UCLASS_BODY()

public:
	FVector GetLocation();
	FQuat GetRotation();
	FVector GetScale3D();

	void SetLocation(FVector NewValue);
	void SetRotation(FQuat NewValue);
	void SetScale3D(FVector NewValue);

	virtual void ApplyDataToResolvedObject() override;
};