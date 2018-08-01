// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Containers/UnrealString.h"
#include "GeometryCollectionBoneNode.h"
#include "Math/Color.h"
#include "Math/IntVector.h"
#include "Math/Quat.h"
#include "Math/Transform.h"
#include "Math/Vector.h"
#include "Math/Vector2D.h"


// ---------------------------------------------------------
//
// General purpose EManagedArrayType definition. 
// This defines things like 
//     EManagedArrayType::FVectorType
// see ManagedArrayTypeValues.inl for specific types.
//
#define MANAGED_ARRAY_TYPE(a,A) F##A##Type,
enum class EManagedArrayType : uint8
{
	FNoneType,
#include "ManagedArrayTypeValues.inl"
};
#undef MANAGED_ARRAY_TYPE

// ---------------------------------------------------------
//  ManagedArrayType<T>
//    Templated function to return a EManagedArrayType.
//
template<class T> inline EManagedArrayType ManagedArrayType();
#define MANAGED_ARRAY_TYPE(a,A) template<> inline EManagedArrayType ManagedArrayType<a>() { return EManagedArrayType::F##A##Type; }
#include "ManagedArrayTypeValues.inl"
#undef MANAGED_ARRAY_TYPE


// ---------------------------------------------------------
//  ManagedArrayType<T>
//     Returns a new EManagedArray shared pointer based on 
//     passed type.
//
inline TSharedPtr<FManagedArrayBase> NewManagedTypedArray(EManagedArrayType ArrayType)
{
	switch (ArrayType)
	{
#define MANAGED_ARRAY_TYPE(a,A)	case EManagedArrayType::F##A##Type:\
		return TSharedPtr< TManagedArray<a> >(new TManagedArray<a>());
#include "ManagedArrayTypeValues.inl"
#undef MANAGED_ARRAY_TYPE
	}
	check(false);
	return TSharedPtr< FManagedArrayBase >();
}
