// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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

	virtual void ApplyDataToResolvedObject() override;
};