// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameRate.h"
#include "UObject/UnrealType.h"
#include "UObject/EnumProperty.h"
#include "MovieSceneSectionSerialization.h"

enum class ESerializedPropertyType : uint8 {

	BoolType,
	ByteType,
	EnumType,
	FloatType,
	VectorType,
	ColorType,
	IntegerType,
	StringType
};

struct FPropertyFileHeader
{

	static const int32 cVersion = 1;

	FPropertyFileHeader() : Version(cVersion)
	{
	}


	FPropertyFileHeader(const FFrameRate &InFrameRate, const FName& InSerializedType, const FGuid& InGuid)
		: Version(cVersion)
		, SerializedType(InSerializedType)
		, Guid(InGuid)
		, TickResolution(InFrameRate)

	{
	}

	friend FArchive& operator<<(FArchive& Ar, FPropertyFileHeader& Header)
	{
		Ar << Header.Version;
		Ar << Header.SerializedType;
		Ar << Header.Guid;
		Ar << Header.TickResolution.Numerator;
		Ar << Header.TickResolution.Denominator;
		Ar << Header.PropertyName;
		Ar << Header.PropertyType;
		Ar << Header.TrackDisplayName;

		return Ar;
	}

	void SetProperty(const UProperty* Property, const FName& InPropertyName)
	{
		if (Property != nullptr)
		{
			PropertyName = InPropertyName;
			TrackDisplayName = *Property->GetDisplayNameText().ToString();
			if (Property->IsA<UBoolProperty>())
			{
				PropertyType = (ESerializedPropertyType::BoolType);
			}
			else if (Property->IsA<UByteProperty>())
			{
				PropertyType = (ESerializedPropertyType::ByteType);
			}
			else if (Property->IsA<UEnumProperty>())
			{
				PropertyType = (ESerializedPropertyType::EnumType);
			}
			else if (Property->IsA<UFloatProperty>())
			{
				PropertyType = (ESerializedPropertyType::FloatType);
			}
			else if (const UStructProperty* StructProperty = Cast<const UStructProperty>(Property))
			{
				if (StructProperty->Struct->GetFName() == NAME_Vector)
				{
					PropertyType = (ESerializedPropertyType::VectorType);
				}
				else if (StructProperty->Struct->GetFName() == NAME_Color)
				{
					PropertyType = (ESerializedPropertyType::ColorType);
				}
			}
			else if (Property->IsA<UIntProperty>())
			{
				PropertyType = (ESerializedPropertyType::IntegerType);
			}
			else if (Property->IsA<UStrProperty>())
			{
				PropertyType = (ESerializedPropertyType::StringType);
			}
		}
	}


	//DATA
	int32 Version;
	FName SerializedType;
	FGuid Guid;
	FFrameRate TickResolution;
	FName PropertyName;
	ESerializedPropertyType PropertyType;
	FString TrackDisplayName;

};


template <typename PropertyType>
struct FSerializedProperty
{
	FSerializedProperty() = default;

	friend FArchive& operator<<(FArchive& Ar, FSerializedProperty& Property)
	{
		Ar << Property.Time;
		Ar << Property.Value;
		return Ar;
	}

	FFrameNumber Time;
	PropertyType Value;

};

using FPropertySerializedBool = FSerializedProperty<bool>;
using FPropertySerializedBoolFrame = TMovieSceneSerializedFrame<FSerializedProperty<bool>>;

using FPropertySerializedByte = FSerializedProperty<uint8>;
using FPropertySerializedByteFrame = TMovieSceneSerializedFrame<FSerializedProperty<uint8>>;

using FPropertySerializedEnum = FSerializedProperty<int64>;
using FPropertySerializedEnumFrame = TMovieSceneSerializedFrame<FSerializedProperty<int64>>;
using FPropertySerializerEnum = TMovieSceneSerializer<FPropertyFileHeader, FSerializedProperty<int64>>;

using FPropertySerializedFloat = FSerializedProperty<float>;
using FPropertySerializedFloatFrame = TMovieSceneSerializedFrame<FSerializedProperty<float>>;

using FPropertySerializedVector = FSerializedProperty<FVector>;
using FPropertySerializedVectorFrame = TMovieSceneSerializedFrame<FSerializedProperty<FVector>>;

using FPropertySerializedColor = FSerializedProperty<FColor>;
using FPropertySerializedColorFrame = TMovieSceneSerializedFrame<FSerializedProperty<FColor>>;

using FPropertySerializedInteger = FSerializedProperty<int32>;
using FPropertySerializedIntegerFrame = TMovieSceneSerializedFrame<FSerializedProperty<int32>>;

using FPropertySerializedString = FSerializedProperty<FString>;
using FPropertySerializedStringFrame = TMovieSceneSerializedFrame<FSerializedProperty<FString>>;


