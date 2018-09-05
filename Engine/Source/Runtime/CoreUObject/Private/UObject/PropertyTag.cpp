// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UObject/PropertyTag.h"
#include "UObject/DebugSerializationFlags.h"
#include "Serialization/SerializedPropertyScope.h"
#include "Serialization/ArchiveUObjectFromStructuredArchive.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "UObject/BlueprintsObjectVersion.h"

/*-----------------------------------------------------------------------------
FPropertyTag
-----------------------------------------------------------------------------*/

// Constructors.
FPropertyTag::FPropertyTag()
	: Prop      (nullptr)
	, Type      (NAME_None)
	, BoolVal   (0)
	, Name      (NAME_None)
	, StructName(NAME_None)
	, EnumName  (NAME_None)
	, InnerType (NAME_None)
	, ValueType	(NAME_None)
	, Size      (0)
	, ArrayIndex(INDEX_NONE)
	, SizeOffset(INDEX_NONE)
	, HasPropertyGuid(0)
{
}

FPropertyTag::FPropertyTag( FArchive& InSaveAr, UProperty* Property, int32 InIndex, uint8* Value, uint8* Defaults )
	: Prop      (Property)
	, Type      (Property->GetID())
	, BoolVal   (0)
	, Name      (Property->GetFName())
	, StructName(NAME_None)
	, EnumName	(NAME_None)
	, InnerType	(NAME_None)
	, ValueType	(NAME_None)
	, Size		(0)
	, ArrayIndex(InIndex)
	, SizeOffset(INDEX_NONE)
	, HasPropertyGuid(0)
{
	if (Property)
	{
		// Handle structs.
		if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
		{
			StructName = StructProperty->Struct->GetFName();
			StructGuid = StructProperty->Struct->GetCustomGuid();
		}
		else if (UEnumProperty* EnumProp = Cast<UEnumProperty>(Property))
		{
			if (UEnum* Enum = EnumProp->GetEnum())
			{
				EnumName = Enum->GetFName();
			}
		}
		else if (UByteProperty* ByteProp = Cast<UByteProperty>(Property))
		{
			if (ByteProp->Enum != nullptr)
			{
				EnumName = ByteProp->Enum->GetFName();
			}
		}
		else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(Property))
		{
			InnerType = ArrayProp->Inner->GetID();
		}
		else if (USetProperty* SetProp = Cast<USetProperty>(Property))
		{
			InnerType = SetProp->ElementProp->GetID();
		}
		else if (UMapProperty* MapProp = Cast<UMapProperty>(Property))
		{
			InnerType = MapProp->KeyProp->GetID();
			ValueType = MapProp->ValueProp->GetID();
		}
		else if (UBoolProperty* Bool = Cast<UBoolProperty>(Property))
		{
			BoolVal = Bool->GetPropertyValue(Value);
		}
	}
}

// Set optional property guid
void FPropertyTag::SetPropertyGuid(const FGuid& InPropertyGuid)
{
	if (InPropertyGuid.IsValid())
	{
		PropertyGuid = InPropertyGuid;
		HasPropertyGuid = true;
	}
}

// Serializer.
FArchive& operator<<(FArchive& Ar, FPropertyTag& Tag)
{
	FStructuredArchiveFromArchive(Ar).GetSlot() << Tag;
	return Ar;
}

void operator<<(FStructuredArchive::FSlot Slot, FPropertyTag& Tag)
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();
	FStructuredArchive::FRecord Record = Slot.EnterRecord();
	int32 Version = UnderlyingArchive.UE4Ver();

	checkf(!UnderlyingArchive.IsSaving() || Tag.Prop, TEXT("FPropertyTag must be constructed with a valid property when used for saving data!"));

	// Name.
	Record << NAMED_ITEM("Name", Tag.Name);
	if ((Tag.Name == NAME_None) || !Tag.Name.IsValid())
	{
		return;
	}

	Record << NAMED_ITEM("Type", Tag.Type);
	if (UnderlyingArchive.IsSaving())
	{
		// remember the offset of the Size variable - UStruct::SerializeTaggedProperties will update it after the
		// property has been serialized.
		Tag.SizeOffset = UnderlyingArchive.Tell();
	}
	{
		FArchive::FScopeSetDebugSerializationFlags S(UnderlyingArchive, DSF_IgnoreDiff);
		Record << NAMED_ITEM("Size", Tag.Size) << NAMED_ITEM("ArrayIndex", Tag.ArrayIndex);
	}
	// only need to serialize this for structs
	if (Tag.Type == NAME_StructProperty)
	{
		Record << NAMED_ITEM("StructName", Tag.StructName);
		if (Version >= VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG)
		{
			Record << NAMED_ITEM("StructGuid", Tag.StructGuid);
		}
	}
	// only need to serialize this for bools
	else if (Tag.Type == NAME_BoolProperty && !UnderlyingArchive.IsTextFormat())
	{
		if (UnderlyingArchive.IsSaving())
		{
			FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Tag.Prop);
			Record << NAMED_ITEM("BoolVal", Tag.BoolVal);
		}
		else
		{
			Record << NAMED_ITEM("BoolVal", Tag.BoolVal);
		}
	}
	// only need to serialize this for bytes/enums
	else if (Tag.Type == NAME_ByteProperty || Tag.Type == NAME_EnumProperty)
	{
		Record << NAMED_ITEM("EnumName", Tag.EnumName);
	}
	// only need to serialize this for arrays
	else if (Tag.Type == NAME_ArrayProperty)
	{
		if (Version >= VAR_UE4_ARRAY_PROPERTY_INNER_TAGS)
		{
			Record << NAMED_ITEM("InnerType", Tag.InnerType);
		}
	}

	if (Version >= VER_UE4_PROPERTY_TAG_SET_MAP_SUPPORT)
	{
		if (Tag.Type == NAME_SetProperty)
		{
			Record << NAMED_ITEM("InnerType", Tag.InnerType);
		}
		else if (Tag.Type == NAME_MapProperty)
		{
			Record << NAMED_ITEM("InnerType", Tag.InnerType);
			Record << NAMED_ITEM("ValueType", Tag.ValueType);
		}
	}

	// Property tags to handle renamed blueprint properties effectively.
	if (Version >= VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG)
	{
		Record << NAMED_ITEM("HasPropertyGuid", Tag.HasPropertyGuid);
		if (Tag.HasPropertyGuid)
		{
			Record << NAMED_ITEM("PropertyGuid", Tag.PropertyGuid);
		}
	}
}

// Property serializer.
void FPropertyTag::SerializeTaggedProperty(FArchive& Ar, UProperty* Property, uint8* Value, uint8* Defaults) const
{
	SerializeTaggedProperty(FStructuredArchiveFromArchive(Ar).GetSlot(), Property, Value, Defaults);
}

void FPropertyTag::SerializeTaggedProperty(FStructuredArchive::FSlot Slot, UProperty* Property, uint8* Value, uint8* Defaults) const
{
	FArchive& UnderlyingArchive = Slot.GetUnderlyingArchive();

	if (!UnderlyingArchive.IsTextFormat() && Property->GetClass() == UBoolProperty::StaticClass())
	{
		UBoolProperty* Bool = (UBoolProperty*)Property;
		if (UnderlyingArchive.IsLoading())
		{
			Bool->SetPropertyValue(Value, BoolVal != 0);
		}

		Slot.EnterStream();	// Effectively discard
	}
	else
	{
#if WITH_EDITOR
		static const FName NAME_SerializeTaggedProperty = FName(TEXT("SerializeTaggedProperty"));
		FArchive::FScopeAddDebugData P(UnderlyingArchive, NAME_SerializeTaggedProperty);
		FArchive::FScopeAddDebugData A(UnderlyingArchive, Property->GetFName());
#endif
		FSerializedPropertyScope SerializedProperty(UnderlyingArchive, Property);

		Property->SerializeItem(Slot, Value, Defaults);
	}
}
