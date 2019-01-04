// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AsyncPackage.h: Unreal async loading definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

/** Helper class to set and restore serialized property on an archive */
class COREUOBJECT_API FSerializedPropertyScope
{
	FArchive& Ar;
	UProperty* Property;
	void PushProperty();
	void PopProperty();
public:
	FSerializedPropertyScope(FArchive& InAr, UProperty* InProperty, const UProperty* OnlyIfOldProperty = nullptr)
		: Ar(InAr)
		, Property(InProperty)
	{
		if (!OnlyIfOldProperty || Ar.GetSerializedProperty() == OnlyIfOldProperty)
		{
			PushProperty();
		}
		else
		{
			Property = nullptr;
		}
	}
	~FSerializedPropertyScope()
	{
		PopProperty();
	}
};
