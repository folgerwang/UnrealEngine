// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PropertyValue.h"

#include "PropertyValueColor.generated.h"

// Keeps an FLinearColor interface by using the property setter/getter functions,
// even though the property itself is of FColor type
UCLASS(BlueprintType)
class VARIANTMANAGERCONTENT_API UPropertyValueColor : public UPropertyValue
{
	GENERATED_UCLASS_BODY()

public:

	// UPropertyValue interface
	virtual void RecordDataFromResolvedObject() override;
	virtual UScriptStruct* GetStructPropertyStruct() const override;
	virtual int32 GetValueSizeInBytes() const override;
	//~ UPropertyValue interface
};