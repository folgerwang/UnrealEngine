// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PropertyValue.h"

#include "PropertyValueVisibility.generated.h"


UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValueVisibility : public UPropertyValue
{
	GENERATED_UCLASS_BODY()

public:

	// Utility functions that set/read the recorded data
	virtual void SetVisibility(bool bVisible);
	virtual bool GetVisibility();

	virtual void ApplyDataToResolvedObject() override;
};