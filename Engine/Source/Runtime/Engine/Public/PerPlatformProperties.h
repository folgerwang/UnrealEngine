// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
PerPlatformProperties.h: Property types that can be overridden on a per-platform basis at cook time
=============================================================================*/

#pragma once

#include "Serialization/Archive.h"
#include "RHIDefinitions.h"
#include "Containers/Map.h"
#include "Algo/Find.h"
#include "PerPlatformProperties.generated.h"

/** TPerPlatformProperty - template parent class for per-platform properties 
 *  Implements Serialize function to replace value at cook time, and 
 *  backwards-compatible loading code for properties converted from simple types.
 */
template<typename _StructType, typename _ValueType, EName _BasePropertyName>
struct ENGINE_API TPerPlatformProperty
{
	typedef _ValueType ValueType;

#if WITH_EDITOR
	/* Return the value */
	_ValueType GetValueForPlatformIdentifiers(FName PlatformGroupName, FName VanillaPlatformName = NAME_None) const
	{
		const _StructType* This = StaticCast<const _StructType*>(this);
		
		const _ValueType* ValuePtr = [This, VanillaPlatformName, PlatformGroupName]() -> const _ValueType*
		{
			const _ValueType* Ptr = nullptr;
			if (VanillaPlatformName != NAME_None)
			{
				TArray<FName> Keys;
				This->PerPlatform.GetKeys(Keys);
				const FName* MatchedName = Keys.FindByPredicate([VanillaPlatformName](FName& Name)
				{
					return VanillaPlatformName.ToString().Contains(Name.ToString());
				});
				Ptr = MatchedName ? This->PerPlatform.Find(*MatchedName) : nullptr;
			}			
			if (Ptr == nullptr && PlatformGroupName != NAME_None)
			{				
				Ptr = This->PerPlatform.Find(PlatformGroupName);
			}
			return Ptr;			
		}();

		return ValuePtr != nullptr ? *ValuePtr : This->Default;
	}

	UE_DEPRECATED(4.22, "GetValueForPlatformGroup renamed GetValueForPlatformIdentifiers")
	_ValueType GetValueForPlatformGroup(FName PlatformGroupName) const
	{
		return GetValueForPlatformIdentifiers(PlatformGroupName);
	}

#endif

	_ValueType GetValueForFeatureLevel(ERHIFeatureLevel::Type FeatureLevel) const
	{
#if WITH_EDITORONLY_DATA
		/* Temporary, we should replace this with something better using editor feature level preview system */
		FName PlatformGroupName;
		switch (FeatureLevel)
		{
			case ERHIFeatureLevel::ES2:
			case ERHIFeatureLevel::ES3_1:
			{
				static FName NAME_Mobile("Mobile");
				PlatformGroupName = NAME_Mobile;
				break;
			}
			default:
				PlatformGroupName = NAME_None;
				break;
		}
		return GetValueForPlatformIdentifiers(PlatformGroupName);
#else
		const _StructType* This = StaticCast<const _StructType*>(this);
		return This->Default;
#endif
	}

	/* Load old properties that have been converted to FPerPlatformX */
	bool SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar)
	{
		if (Tag.Type == _BasePropertyName)
		{
			_StructType* This = StaticCast<_StructType*>(this);
			_ValueType OldValue;
			Ar << OldValue;
			*This = _StructType(OldValue);
			return true;
		}
		return false;
	}

	/* Serialization */
	bool Serialize(FArchive& Ar)
	{
		Ar << *this;
		return true;
	}

	/* Serialization */
	bool Serialize(FStructuredArchive::FSlot Slot)
	{
		Slot << *this;
		return true;
	}
};

template<typename _StructType, typename _ValueType, EName _BasePropertyName>
ENGINE_API FArchive& operator<<(FArchive& Ar, TPerPlatformProperty<_StructType, _ValueType, _BasePropertyName>& P);
template<typename _StructType, typename _ValueType, EName _BasePropertyName>
ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<_StructType, _ValueType, _BasePropertyName>& P);

/** FPerPlatformInt - int32 property with per-platform overrides */
USTRUCT()
struct ENGINE_API FPerPlatformInt
#if CPP
:	public TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>
#endif
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = PerPlatform)
	int32 Default;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = PerPlatform)
	TMap<FName, int32> PerPlatform;
#endif

	FPerPlatformInt()
	:	Default(0)
	{
	}

	FPerPlatformInt(int32 InDefaultValue)
	:	Default(InDefaultValue)
	{
	}
};
extern template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>&);
extern template ENGINE_API void operator<<(FStructuredArchive::FSlot Slot, TPerPlatformProperty<FPerPlatformInt, int32, NAME_IntProperty>&);

template<>
struct TStructOpsTypeTraits<FPerPlatformInt>
	: public TStructOpsTypeTraitsBase2<FPerPlatformInt>
{
	enum
	{
		WithSerializeFromMismatchedTag = true,
		WithSerializer = true
	};
};

/** FPerPlatformFloat - float property with per-platform overrides */
USTRUCT()
struct ENGINE_API FPerPlatformFloat
#if CPP
:	public TPerPlatformProperty<FPerPlatformFloat, float, NAME_FloatProperty>
#endif
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = PerPlatform)
	float Default;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = PerPlatform)
	TMap<FName, float> PerPlatform;
#endif

	FPerPlatformFloat()
	:	Default(0.f)
	{
	}

	FPerPlatformFloat(float InDefaultValue)
	:	Default(InDefaultValue)
	{
	}
};
extern template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformFloat, float, NAME_FloatProperty>&);

template<>
struct TStructOpsTypeTraits<FPerPlatformFloat>
:	public TStructOpsTypeTraitsBase2<FPerPlatformFloat>
{
	enum
	{
		WithSerializeFromMismatchedTag = true,
		WithSerializer = true
	};
};

/** FPerPlatformBool - bool property with per-platform overrides */
USTRUCT()
struct ENGINE_API FPerPlatformBool
#if CPP
:	public TPerPlatformProperty<FPerPlatformBool, bool, NAME_BoolProperty>
#endif
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, Category = PerPlatform)
	bool Default;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = PerPlatform)
	TMap<FName, bool> PerPlatform;
#endif

	FPerPlatformBool()
	:	Default(false)
	{
	}

	FPerPlatformBool(bool InDefaultValue)
	:	Default(InDefaultValue)
	{
	}
};
extern template ENGINE_API FArchive& operator<<(FArchive&, TPerPlatformProperty<FPerPlatformBool, bool, NAME_BoolProperty>&);

template<>
struct TStructOpsTypeTraits<FPerPlatformBool>
	: public TStructOpsTypeTraitsBase2<FPerPlatformBool>
{
	enum
	{
		WithSerializeFromMismatchedTag = true,
		WithSerializer = true
	};
};