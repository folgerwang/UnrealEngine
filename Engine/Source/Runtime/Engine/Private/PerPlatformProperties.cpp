// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

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
		const PlatformInfo::FPlatformInfo& PlatformInfo = Ar.CookingTarget()->GetPlatformInfo();
		ValueType Value = Property.GetValueForPlatformIdentifiers(PlatformInfo.PlatformGroupName, PlatformInfo.VanillaPlatformName);
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

/** Serializer to cook out the most appropriate platform override */
template<typename StructType, typename ValueType, EName BasePropertyName>
ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<StructType, ValueType, BasePropertyName>& Property)
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();
	FStructuredArchive::FRecord Record = Slot.EnterRecord();

	bool bCooked = false;
#if WITH_EDITOR
	if (UnderlyingArchive.IsCooking())
	{
		bCooked = true;
		Record << NAMED_FIELD(bCooked);
		// Save out platform override if it exists and Default otherwise
		ValueType Value = Property.GetValueForPlatformIdentifiers(UnderlyingArchive.CookingTarget()->GetPlatformInfo().PlatformGroupName);
		Record << NAMED_FIELD(Value);
	}
	else
#endif
	{
		StructType* This = StaticCast<StructType*>(&Property);
		Record << NAMED_FIELD(bCooked);
		Record << NAMED_ITEM("Value", This->Default);
#if WITH_EDITORONLY_DATA
		if (!bCooked)
		{
			Record << NAMED_ITEM("PerPlatform", This->PerPlatform);
		}
#endif
	}
}

template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>&);
template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformFloat, float, NAME_FloatProperty>&);
template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformBool, bool, NAME_BoolProperty>&);
template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>&);
template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FPerPlatformFloat, float, NAME_FloatProperty>&);
template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FPerPlatformBool, bool, NAME_BoolProperty>&);
