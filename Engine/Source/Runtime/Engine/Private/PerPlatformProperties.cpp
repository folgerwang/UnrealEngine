// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "PerPlatformProperties.h"
#include "Serialization/Archive.h"

#if WITH_EDITOR
#include "Interfaces/ITargetPlatform.h"
#include "PlatformInfo.h"
#endif

/** Serializer to cook out the most appropriate platform override */
template<typename StructType, typename ValueType, EName BasePropertyName>
ENGINE_API FArchive& operator<<(FArchive& Ar, TPerPlatformProperty<StructType, ValueType, BasePropertyName>& Property)
{
	bool bCooked = false;
#if WITH_EDITOR
	if (Ar.IsCooking())
	{
		bCooked = true;
		Ar << bCooked;
		// Save out platform override if it exists and Default otherwise
		ValueType Value = Property.GetValueForPlatformGroup(Ar.CookingTarget()->GetPlatformInfo().PlatformGroupName);
		Ar << Value;
	}
	else
#endif
	{
		StructType* This = StaticCast<StructType*>(&Property);
		Ar << bCooked;
		Ar << This->Default;
#if WITH_EDITORONLY_DATA
		if (!bCooked)
		{
			Ar << This->PerPlatform;
		}
#endif
	}
	return Ar;
}

template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>&);
template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformFloat, float, NAME_FloatProperty>&);
