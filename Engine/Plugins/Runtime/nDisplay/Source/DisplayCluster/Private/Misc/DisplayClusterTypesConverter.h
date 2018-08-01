// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DisplayClusterOperationMode.h"
#include "DisplayClusterStrings.h"


/**
 * Auxiliary class with different type conversion functions
 */
class FDisplayClusterTypesConverter
{
public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// TYPE --> STRING
	//////////////////////////////////////////////////////////////////////////////////////////////
	template <typename ConvertFrom>
	static FString ToString(const ConvertFrom& from);

	template <> static FString ToString<> (const FString& from)   { return from; }
	template <> static FString ToString<> (const bool& from)      { return (from ? DisplayClusterStrings::cfg::spec::ValTrue : DisplayClusterStrings::cfg::spec::ValFalse); }
	template <> static FString ToString<> (const int32& from)     { return FString::FromInt(from); }
	template <> static FString ToString<> (const float& from)     { return FString::SanitizeFloat(from); }
	template <> static FString ToString<> (const double& from)    { return FString::Printf(TEXT("%lf"), from); }
	template <> static FString ToString<> (const FVector& from)   { return from.ToString(); }
	template <> static FString ToString<> (const FVector2D& from) { return from.ToString(); }
	template <> static FString ToString<> (const FRotator& from)  { return from.ToString(); }

	template <> static FString ToString<> (const EDisplayClusterOperationMode& from)
	{
		switch (from)
		{
		case EDisplayClusterOperationMode::Cluster:
			return FString("cluster");
		case EDisplayClusterOperationMode::Standalone:
			return FString("standalone");
		case EDisplayClusterOperationMode::Editor:
			return FString("editor");
		case EDisplayClusterOperationMode::Disabled:
			return FString("disabled");
		default:
			return FString("unknown");
		}
	}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// STRING --> TYPE
	//////////////////////////////////////////////////////////////////////////////////////////////
	template <typename ConvertTo>
	static ConvertTo FromString(const FString& from);

	template <> static FString   FromString<> (const FString& from) { return from; }
	template <> static bool      FromString<> (const FString& from) { return (from == FString("1") || from == DisplayClusterStrings::cfg::spec::ValTrue); }
	template <> static int32     FromString<> (const FString& from) { return FCString::Atoi(*from); }
	template <> static float     FromString<> (const FString& from) { return FCString::Atof(*from); }
	template <> static double    FromString<> (const FString& from) { return FCString::Atod(*from); }
	template <> static FVector   FromString<> (const FString& from) { FVector vec;  vec.InitFromString(from); return vec; }
	template <> static FVector2D FromString<> (const FString& from) { FVector2D vec;  vec.InitFromString(from); return vec; }
	template <> static FRotator  FromString<> (const FString& from) { FRotator rot; rot.InitFromString(from); return rot; }
};
