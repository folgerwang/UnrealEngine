// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"


/** Generic (struct) implementation */
template<typename T>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl(UProperty* Property)
{
	if ( UStructProperty* StructProperty = Cast<UStructProperty>(Property) )
	{
		return StructProperty->Struct == T::StaticStruct();
	}

	return false;
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<bool>(UProperty* Property)
{
	return Property->GetClass() == UBoolProperty::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<int8>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}

	return Property->GetClass() == UInt8Property::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<uint8>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}
	else if(UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property))
	{
		return !BoolProperty->IsNativeBool();
	}

	return Property->GetClass() == UByteProperty::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<int16>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}

	return Property->GetClass() == UInt16Property::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<uint16>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}
	else if(UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property))
	{
		return !BoolProperty->IsNativeBool();
	}

	return Property->GetClass() == UUInt16Property::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<int32>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}

	return Property->GetClass() == UIntProperty::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<uint32>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}
	else if(UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property))
	{
		return !BoolProperty->IsNativeBool();
	}

	return Property->GetClass() == UUInt32Property::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<int64>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}

	return Property->GetClass() == UInt64Property::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<uint64>(UProperty* Property)
{
	if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
	{
		Property = EnumProperty->GetUnderlyingProperty();
	}
	else if(UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property))
	{
		return !BoolProperty->IsNativeBool();
	}

	return Property->GetClass() == UUInt64Property::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<float>(UProperty* Property)
{
	return Property->GetClass() == UFloatProperty::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<double>(UProperty* Property)
{
	return Property->GetClass() == UDoubleProperty::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FText>(UProperty* Property)
{
	return Property->GetClass() == UTextProperty::StaticClass();
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FString>(UProperty* Property)
{
	return Property->GetClass() == UStrProperty::StaticClass();
}

template<typename T>
inline bool IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct(UProperty* Property)
{
	static const UScriptStruct* BuiltInStruct = TBaseStructure<T>::Get();
	
	if ( UStructProperty* StructProperty = Cast<UStructProperty>(Property) )
	{
		return StructProperty->Struct == BuiltInStruct;
	}

	return false;
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FColor>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FColor>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FLinearColor>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FLinearColor>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FVector2D>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FVector2D>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FVector>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FVector>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FRotator>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FRotator>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FQuat>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FQuat>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FTransform>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FTransform>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FBox2D>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FBox2D>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FGuid>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FGuid>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FFloatRangeBound>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FFloatRangeBound>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FFloatRange>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FFloatRange>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FInt32RangeBound>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FInt32RangeBound>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FInt32Range>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FInt32Range>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FFloatInterval>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FFloatInterval>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FInt32Interval>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FInt32Interval>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FSoftObjectPath>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FSoftObjectPath>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<FSoftClassPath>(UProperty* Property)
{
	return IsConcreteTypeCompatibleWithReflectedType_BuiltInStruct<FSoftClassPath>(Property);
}

template<>
inline bool IsConcreteTypeCompatibleWithReflectedType_Impl<UObject*>(UProperty* Property)
{
	if ( UObjectProperty* ObjectProperty = Cast<UObjectProperty>(Property) )
	{
		return true;
		//return ObjectProperty->PropertyClass->IsChildOf(T::StaticClass());
	}

	return false;
}

/** Standard implementation */
template<typename T> 
struct FConcreteTypeCompatibleWithReflectedTypeHelper
{
	static bool IsConcreteTypeCompatibleWithReflectedType(UProperty* Property) 
	{
		return IsConcreteTypeCompatibleWithReflectedType_Impl<T>(Property);
	}
};

/** Dynamic array partial specialization */
template<typename T>
struct FConcreteTypeCompatibleWithReflectedTypeHelper<TArray<T>>
{
	static bool IsConcreteTypeCompatibleWithReflectedType(UProperty* Property) 
	{
		if( UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property) )
		{
			return IsConcreteTypeCompatibleWithReflectedType_Impl<T>(ArrayProperty->Inner);
		}

		return false;
	}
};

/** Static array partial specialization */
template<typename T, int32 N>
struct FConcreteTypeCompatibleWithReflectedTypeHelper<T[N]>
{
	static bool IsConcreteTypeCompatibleWithReflectedType(UProperty* Property) 
	{
		return Property->ArrayDim == N && IsConcreteTypeCompatibleWithReflectedType_Impl<T>(Property);
	}
};

/** Weak object partial specialization */
template<typename T>
struct FConcreteTypeCompatibleWithReflectedTypeHelper<TWeakObjectPtr<T>>
{
	static bool IsConcreteTypeCompatibleWithReflectedType(UProperty* Property) 
	{
		return Property->GetClass() == UWeakObjectProperty::StaticClass();
	}
};

/** Weak object partial specialization */
template<typename T>
struct FConcreteTypeCompatibleWithReflectedTypeHelper<TLazyObjectPtr<T>>
{
	static bool IsConcreteTypeCompatibleWithReflectedType(UProperty* Property) 
	{
		return Property->GetClass() == ULazyObjectProperty::StaticClass();
	}
};

/** 
 * Check whether the concrete type T is compatible with the reflected type of a UProperty for 
 * the purposes of CopysingleValue()
 * Non-enum implementation.
 */
template<typename T>
static inline typename TEnableIf<!TIsEnum<T>::Value, bool>::Type IsConcreteTypeCompatibleWithReflectedType(UProperty* Property)
{
	return FConcreteTypeCompatibleWithReflectedTypeHelper<T>::IsConcreteTypeCompatibleWithReflectedType(Property);
}

/** Enum implementation. @see IsConcreteTypeCompatibleWithReflectedType */
template<typename T>
static inline typename TEnableIf<TIsEnum<T>::Value, bool>::Type IsConcreteTypeCompatibleWithReflectedType(UProperty* Property)
{
	return FConcreteTypeCompatibleWithReflectedTypeHelper<uint8>::IsConcreteTypeCompatibleWithReflectedType(Property);
}

template<typename T>
inline bool PropertySizesMatch_Impl(UProperty* InProperty)
{
	return InProperty->ElementSize == sizeof(T);
}

template<>
inline bool PropertySizesMatch_Impl<uint8>(UProperty* InProperty)
{
	if(UBoolProperty* BoolProperty = Cast<UBoolProperty>(InProperty))
	{
		return !BoolProperty->IsNativeBool();
	}

	return InProperty->ElementSize == sizeof(uint8);
}

template<>
inline bool PropertySizesMatch_Impl<uint16>(UProperty* InProperty)
{
	if(UBoolProperty* BoolProperty = Cast<UBoolProperty>(InProperty))
	{
		return !BoolProperty->IsNativeBool();
	}

	return InProperty->ElementSize == sizeof(uint16);
}

template<>
inline bool PropertySizesMatch_Impl<uint32>(UProperty* InProperty)
{
	if(UBoolProperty* BoolProperty = Cast<UBoolProperty>(InProperty))
	{
		return !BoolProperty->IsNativeBool();
	}

	return InProperty->ElementSize == sizeof(uint32);
}

template<>
inline bool PropertySizesMatch_Impl<uint64>(UProperty* InProperty)
{
	if(UBoolProperty* BoolProperty = Cast<UBoolProperty>(InProperty))
	{
		return !BoolProperty->IsNativeBool();
	}

	return InProperty->ElementSize == sizeof(uint64);
}

template<typename T>
struct FPropertySizesMatchHelper
{
	static bool PropertySizesMatch(UProperty* InProperty)
	{
		return PropertySizesMatch_Impl<T>(InProperty);
	}
};

template<typename T, int32 N>
struct FPropertySizesMatchHelper<T[N]>
{
	static bool PropertySizesMatch(UProperty* InProperty)
	{
		return PropertySizesMatch_Impl<T>(InProperty);
	}
};

template<typename T>
inline bool PropertySizesMatch(UProperty* InProperty)
{
	return FPropertySizesMatchHelper<T>::PropertySizesMatch(InProperty);
}
