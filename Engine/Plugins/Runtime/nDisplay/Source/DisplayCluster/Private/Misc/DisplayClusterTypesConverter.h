// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Timecode.h"
#include "Misc/FrameRate.h"
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

	// We can't just use FTimecode ToString as that loses information.
	template <> static FString ToString<> (const FTimecode& from)	{ return FString::Printf(TEXT("%d;%d;%d;%d;%d"), from.bDropFrameFormat ? 1 : 0, from.Hours, from.Minutes, from.Seconds, from.Frames); }
	template <> static FString ToString<> (const FFrameRate& from) 	{ return FString::Printf(TEXT("%d;%d"), from.Numerator, from.Denominator); }

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
	template <> static FTimecode FromString<> (const FString& from)
	{
		FTimecode timecode;

		TArray<FString> parts;
		parts.Reserve(5);
		const int32 found = from.ParseIntoArray(parts, TEXT(";"));

		// We are expecting 5 "parts" - DropFrame, Hours, Minutes, Seconds, Frames.
		if (found == 5)
		{
			timecode.bDropFrameFormat = FromString<bool>(parts[0]);
			timecode.Hours = FromString<int32>(parts[1]);
			timecode.Minutes = FromString<int32>(parts[2]);
			timecode.Seconds = FromString<int32>(parts[3]);
			timecode.Frames = FromString<int32>(parts[4]);
		}

		return timecode;
	}
	template <> static FFrameRate FromString<> (const FString& from)
	{
		FFrameRate frameRate;

		TArray<FString> parts;
		parts.Reserve(2);
		const int32 found = from.ParseIntoArray(parts, TEXT(";"));

		// We are expecting 2 "parts" - Numerator, Denominator.
		if (found == 2)
		{
			frameRate.Numerator = FromString<int32>(parts[0]);
			frameRate.Denominator = FromString<int32>(parts[1]);
		}

		return frameRate;
	}
};
