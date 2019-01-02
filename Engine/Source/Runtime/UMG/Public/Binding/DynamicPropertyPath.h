// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "PropertyPathHelpers.h"
#include "DynamicPropertyPath.generated.h"

/** */
USTRUCT()
struct UMG_API FDynamicPropertyPath : public FCachedPropertyPath
{
	GENERATED_USTRUCT_BODY()

public:

	/** */
	FDynamicPropertyPath();

	/** */
	FDynamicPropertyPath(const FString& Path);

	/** */
	FDynamicPropertyPath(const TArray<FString>& PropertyChain);

	/** Get the value represented by this property path */
	template<typename T>
	bool GetValue(UObject* InContainer, T& OutValue) const
	{
		UProperty* OutProperty;
		return GetValue<T>(InContainer, OutValue, OutProperty);
	}

	/** Get the value and the leaf property represented by this property path */
	template<typename T>
	bool GetValue(UObject* InContainer, T& OutValue, UProperty*& OutProperty) const
	{
		return PropertyPathHelpers::GetPropertyValue(InContainer, *this, OutValue, OutProperty);
	}
};
