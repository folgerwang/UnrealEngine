// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Sections/MovieSceneStringChannel.h"
#include "Curves/StringCurve.h"
#include "MovieSceneFwd.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "MovieSceneFrameMigration.h"

uint32 FMovieSceneStringChannel::GetChannelID()
{
	static uint32 ID = FMovieSceneChannelEntry::RegisterNewID();
	return ID;
}

const FString* FMovieSceneStringChannel::Evaluate(FFrameTime InTime) const
{
	if (Times.Num())
	{
		const int32 Index = FMath::Max(0, Algo::UpperBound(Times, InTime.FrameNumber)-1);
		return &Values[Index];
	}
	
	return bHasDefaultValue ? &DefaultValue : nullptr;
}

bool FMovieSceneStringChannel::SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar)
{
	static const FName StringCurveName("StringCurve");
	if (Tag.Type == NAME_StructProperty && Tag.StructName == StringCurveName)
	{
		FStringCurve StringCurve;
		FStringCurve::StaticStruct()->SerializeItem(Ar, &StringCurve, nullptr);

		FString NewDefault = StringCurve.GetDefaultValue();
		if (NewDefault.Len())
		{
			bHasDefaultValue = true;
			DefaultValue = MoveTemp(NewDefault);
		}

		FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();

		Times.Reserve(StringCurve.GetNumKeys());
		Values.Reserve(StringCurve.GetNumKeys());
		int32 Index = 0;
		for (const FStringCurveKey& Key : StringCurve.GetKeys())
		{
			FFrameNumber KeyTime = UpgradeLegacyMovieSceneTime(nullptr, LegacyFrameRate, Key.Time);

			FString Val(Key.Value);
			ConvertInsertAndSort<FString>(Index++, KeyTime, Val, Times, Values);
		}
		return true;
	}

	return false;
}
