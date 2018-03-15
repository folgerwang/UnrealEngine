// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Channels/MovieSceneBoolChannel.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Curves/IntegralCurve.h"
#include "FrameRate.h"
#include "MovieSceneFwd.h"
#include "MovieSceneFrameMigration.h"

uint32 FMovieSceneBoolChannel::GetChannelID()
{
	static uint32 ID = FMovieSceneChannelEntry::RegisterNewID();
	return ID;
}

bool FMovieSceneBoolChannel::SerializeFromMismatchedTag(const FPropertyTag& Tag, FArchive& Ar)
{
	static const FName IntegralCurveName("IntegralCurve");
	if (Tag.Type == NAME_StructProperty && Tag.StructName == IntegralCurveName)
	{
		FIntegralCurve IntegralCurve;
		FIntegralCurve::StaticStruct()->SerializeItem(Ar, &IntegralCurve, nullptr);

		if (IntegralCurve.GetDefaultValue() != MAX_int32)
		{
			bHasDefaultValue = true;
			DefaultValue = IntegralCurve.GetDefaultValue() != 0;
		}

		Times.Reserve(IntegralCurve.GetNumKeys());
		Values.Reserve(IntegralCurve.GetNumKeys());

		FFrameRate LegacyFrameRate = GetLegacyConversionFrameRate();

		int32 Index = 0;
		for (auto It = IntegralCurve.GetKeyIterator(); It; ++It)
		{
			FFrameNumber KeyTime = UpgradeLegacyMovieSceneTime(nullptr, LegacyFrameRate, It->Time);

			bool Val = It->Value != 0;
			ConvertInsertAndSort<bool>(Index++, KeyTime, Val, Times, Values);
		}
		return true;
	}

	return false;
}

bool FMovieSceneBoolChannel::Evaluate(FFrameTime InTime, bool& OutValue) const
{
	if (Times.Num())
	{
		const int32 Index = FMath::Max(0, Algo::UpperBound(Times, InTime.FrameNumber)-1);
		OutValue = Values[Index];
		return true;
	}
	else if (bHasDefaultValue)
	{
		OutValue = DefaultValue;
		return true;
	}

	return false;
}