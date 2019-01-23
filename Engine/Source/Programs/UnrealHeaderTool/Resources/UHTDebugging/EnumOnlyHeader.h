// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EnumOnlyHeader.generated.h"

UENUM()
enum EOldEnum
{
	One,
	Two,
	Three
};

UENUM()
namespace ENamespacedEnum
{
	enum Type
	{
		Four,
		Five,
		Six
	};
}

UENUM()
enum class ECppEnum : uint8
{
	Seven,
	Eight,
	Nine
};

UENUM()
enum struct ECppEnumStruct : uint8
{
	Ten,
	Eleven,
	Twelve
};

UENUM()
enum alignas(8) EAlignedOldEnum
{
	Thirteen,
	Fourteen,
	Fifteen
};

UENUM()
namespace EAlignedNamespacedEnum
{
	enum alignas(8) Type
	{
		Sixteen,
		Seventeen,
		Eighteen
	};
}

UENUM()
enum class alignas(8) EAlignedCppEnum : uint8
{
	Nineteen,
	Twenty,
	TwentyOne
};

UENUM()
enum struct alignas(8) EAlignedCppEnumStruct : uint8
{
	TwentyTwo,
	TwentyThree,
	TwentyFour
};
