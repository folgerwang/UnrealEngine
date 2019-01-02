// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Binding/DynamicPropertyPath.h"
#include "PropertyPathHelpers.h"

FDynamicPropertyPath::FDynamicPropertyPath()
{
}

FDynamicPropertyPath::FDynamicPropertyPath(const FString& Path)
	: FCachedPropertyPath(Path)
{
}

FDynamicPropertyPath::FDynamicPropertyPath(const TArray<FString>& PropertyChain)
	: FCachedPropertyPath(PropertyChain)
{
}
