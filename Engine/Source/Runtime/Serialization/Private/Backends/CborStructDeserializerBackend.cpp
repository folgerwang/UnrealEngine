// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Backends/CborStructDeserializerBackend.h"
#include "Backends/StructDeserializerBackendUtilities.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/TextProperty.h"

const FString& FCborStructDeserializerBackend::GetCurrentPropertyName() const
{
	return LastMapKey;
}

FString FCborStructDeserializerBackend::GetDebugString() const
{
	FArchive* Ar = const_cast<FArchive*>(CborReader.GetArchive());
	return FString::Printf(TEXT("Offset: %u"), Ar ? Ar->Tell() : 0);
}

const FString& FCborStructDeserializerBackend::GetLastErrorMessage() const
{
	// interface function that is actually entirely unused...
	static FString Dummy;
	return Dummy;
}

bool FCborStructDeserializerBackend::GetNextToken(EStructDeserializerBackendTokens& OutToken)
{
	LastMapKey.Reset();

	if (!CborReader.ReadNext(LastContext))
	{
		OutToken = LastContext.IsError() ? EStructDeserializerBackendTokens::Error : EStructDeserializerBackendTokens::None;
		return false;
	}

	if (LastContext.IsBreak())
	{
		ECborCode ContainerEndType = LastContext.AsBreak();
		// We do not support indefinite string container type
		check(ContainerEndType == ECborCode::Array || ContainerEndType == ECborCode::Map);
		OutToken = ContainerEndType == ECborCode::Array ? EStructDeserializerBackendTokens::ArrayEnd : EStructDeserializerBackendTokens::StructureEnd;
		return true;
	}

	// if after reading the last context, the parent context is a map with an odd length, we just read a key
	if (CborReader.GetContext().MajorType() == ECborCode::Map && (CborReader.GetContext().AsLength() & 1))
	{
		// Should be a string
		check(LastContext.MajorType() == ECborCode::TextString);
		LastMapKey = LastContext.AsString();

		// Read next and carry on
		if (!CborReader.ReadNext(LastContext))
		{
			OutToken = LastContext.IsError() ? EStructDeserializerBackendTokens::Error : EStructDeserializerBackendTokens::None;
			return false;
		}
	}

	switch (LastContext.MajorType())
	{
	case ECborCode::Array:
		OutToken = EStructDeserializerBackendTokens::ArrayStart;
		break;
	case ECborCode::Map:
		OutToken = EStructDeserializerBackendTokens::StructureStart;
		break;
	case ECborCode::Int:
		// fall through
	case ECborCode::Uint:
		// fall through
	case ECborCode::TextString:
		// fall through
	case ECborCode::Prim:
		OutToken = EStructDeserializerBackendTokens::Property;
		break;
	default:
		// Other types are unsupported
		check(false);
	}

	return true;
}

bool FCborStructDeserializerBackend::ReadProperty(UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex)
{
	switch (LastContext.MajorType())
	{
	// Unsigned Integers
	case ECborCode::Uint:
	{
		if (UByteProperty* ByteProperty = Cast<UByteProperty>(Property))
		{
			return StructDeserializerBackendUtilities::SetPropertyValue(ByteProperty, Outer, Data, ArrayIndex, (uint8)LastContext.AsUInt());
		}

		if (UUInt16Property* UInt16Property = Cast<UUInt16Property>(Property))
		{
			return StructDeserializerBackendUtilities::SetPropertyValue(UInt16Property, Outer, Data, ArrayIndex, (uint16)LastContext.AsUInt());
		}

		if (UUInt32Property* UInt32Property = Cast<UUInt32Property>(Property))
		{
			return StructDeserializerBackendUtilities::SetPropertyValue(UInt32Property, Outer, Data, ArrayIndex, (uint32)LastContext.AsUInt());
		}

		if (UUInt64Property* UInt64Property = Cast<UUInt64Property>(Property))
		{
			return StructDeserializerBackendUtilities::SetPropertyValue(UInt64Property, Outer, Data, ArrayIndex, (uint64)LastContext.AsUInt());
		}
	}
	// Fall through - cbor can encode positive signed integers as unsigned
	// Signed Integers
	case ECborCode::Int:
	{
		if (UInt8Property* Int8Property = Cast<UInt8Property>(Property))
		{
			return StructDeserializerBackendUtilities::SetPropertyValue(Int8Property, Outer, Data, ArrayIndex, (int8)LastContext.AsInt());
		}

		if (UInt16Property* Int16Property = Cast<UInt16Property>(Property))
		{
			return StructDeserializerBackendUtilities::SetPropertyValue(Int16Property, Outer, Data, ArrayIndex, (int16)LastContext.AsInt());
		}

		if (UIntProperty* IntProperty = Cast<UIntProperty>(Property))
		{
			return StructDeserializerBackendUtilities::SetPropertyValue(IntProperty, Outer, Data, ArrayIndex, (int32)LastContext.AsInt());
		}

		if (UInt64Property* Int64Property = Cast<UInt64Property>(Property))
		{
			return StructDeserializerBackendUtilities::SetPropertyValue(Int64Property, Outer, Data, ArrayIndex, (int64)LastContext.AsInt());
		}


		UE_LOG(LogSerialization, Verbose, TEXT("Integer field %s with value '%d' is not supported in UProperty type %s (%s)"), *Property->GetFName().ToString(), LastContext.AsUInt(), *Property->GetClass()->GetName(), *GetDebugString());

		return false;
	}
	break;

	// Strings, Names & Enumerations
	case ECborCode::TextString:
	{
		FString StringValue = LastContext.AsString();

		if (UStrProperty* StrProperty = Cast<UStrProperty>(Property))
		{
			return StructDeserializerBackendUtilities::SetPropertyValue(StrProperty, Outer, Data, ArrayIndex, StringValue);
		}

		if (UNameProperty* NameProperty = Cast<UNameProperty>(Property))
		{
			return StructDeserializerBackendUtilities::SetPropertyValue(NameProperty, Outer, Data, ArrayIndex, FName(*StringValue));
		}

		if (UTextProperty* TextProperty = Cast<UTextProperty>(Property))
		{
			return StructDeserializerBackendUtilities::SetPropertyValue(TextProperty, Outer, Data, ArrayIndex, FText::FromString(StringValue));
		}

		if (UByteProperty* ByteProperty = Cast<UByteProperty>(Property))
		{
			if (!ByteProperty->Enum)
			{
				return false;
			}

			int32 Value = ByteProperty->Enum->GetValueByName(*StringValue);
			if (Value == INDEX_NONE)
			{
				return false;
			}

			return StructDeserializerBackendUtilities::SetPropertyValue(ByteProperty, Outer, Data, ArrayIndex, (uint8)Value);
		}

		if (UEnumProperty* EnumProperty = Cast<UEnumProperty>(Property))
		{
			int64 Value = EnumProperty->GetEnum()->GetValueByName(*StringValue);
			if (Value == INDEX_NONE)
			{
				return false;
			}

			if (void* ElementPtr = StructDeserializerBackendUtilities::GetPropertyValuePtr(EnumProperty, Outer, Data, ArrayIndex))
			{
				EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ElementPtr, Value);
				return true;
			}

			return false;
		}

		if (UClassProperty* ClassProperty = Cast<UClassProperty>(Property))
		{
			return StructDeserializerBackendUtilities::SetPropertyValue(ClassProperty, Outer, Data, ArrayIndex, LoadObject<UClass>(NULL, *StringValue, NULL, LOAD_NoWarn));
		}

		UE_LOG(LogSerialization, Verbose, TEXT("String field %s with value '%s' is not supported in UProperty type %s (%s)"), *Property->GetFName().ToString(), *StringValue, *Property->GetClass()->GetName(), *GetDebugString());

		return false;
	}
	break;

	// Prim
	case ECborCode::Prim:
	{
		switch (LastContext.AdditionalValue())
		{
			// Boolean
		case ECborCode::True:
			// fall through
		case ECborCode::False:
			if (UBoolProperty* BoolProperty = Cast<UBoolProperty>(Property))
			{
				return StructDeserializerBackendUtilities::SetPropertyValue(BoolProperty, Outer, Data, ArrayIndex, LastContext.AsBool());
			}
			UE_LOG(LogSerialization, Verbose, TEXT("Boolean field %s with value '%s' is not supported in UProperty type %s (%s)"), *Property->GetFName().ToString(), LastContext.AsBool() ? *(GTrue.ToString()) : *(GFalse.ToString()), *Property->GetClass()->GetName(), *GetDebugString());
			return false;
			// Null
		case ECborCode::Null:
			return StructDeserializerBackendUtilities::ClearPropertyValue(Property, Outer, Data, ArrayIndex);
			// Float
		case ECborCode::Value_4Bytes:
			if (UFloatProperty* FloatProperty = Cast<UFloatProperty>(Property))
			{
				return StructDeserializerBackendUtilities::SetPropertyValue(FloatProperty, Outer, Data, ArrayIndex, LastContext.AsFloat());
			}
			UE_LOG(LogSerialization, Verbose, TEXT("Float field %s with value '%f' is not supported in UProperty type %s (%s)"), *Property->GetFName().ToString(), LastContext.AsFloat(), *Property->GetClass()->GetName(), *GetDebugString());
			return false;
			// Double
		case ECborCode::Value_8Bytes:
			if (UDoubleProperty* DoubleProperty = Cast<UDoubleProperty>(Property))
			{
				return StructDeserializerBackendUtilities::SetPropertyValue(DoubleProperty, Outer, Data, ArrayIndex, LastContext.AsDouble());
			}
			UE_LOG(LogSerialization, Verbose, TEXT("Double field %s with value '%f' is not supported in UProperty type %s (%s)"), *Property->GetFName().ToString(), LastContext.AsDouble(), *Property->GetClass()->GetName(), *GetDebugString());
			return false;
		default:
			UE_LOG(LogSerialization, Verbose, TEXT("Unsupported primitive type for %s in UProperty type %s (%s)"), *Property->GetFName().ToString(), LastContext.AsDouble(), *Property->GetClass()->GetName(), *GetDebugString());
			return false;
		}
	}
	}

	return true;

}

void FCborStructDeserializerBackend::SkipArray()
{
	CborReader.SkipContainer(ECborCode::Array);
}

void FCborStructDeserializerBackend::SkipStructure()
{
	CborReader.SkipContainer(ECborCode::Map);
}
