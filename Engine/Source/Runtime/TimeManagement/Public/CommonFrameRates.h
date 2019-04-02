// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/FrameRate.h"
#include "Containers/ArrayView.h"

enum class ECommonFrameRate : uint8
{
	FPS_12,
	FPS_15,
	FPS_24,
	FPS_25,
	FPS_30,
	FPS_48,
	FPS_50,
	FPS_60,
	FPS_100,
	FPS_120,
	FPS_240,
	NTSC_24,
	NTSC_30,
	NTSC_60,

	Private_Num
};

struct FCommonFrameRateInfo
{
	FFrameRate FrameRate;
	FText DisplayName;
	FText Description;
};

struct TIMEMANAGEMENT_API FCommonFrameRates
{
	typedef __underlying_type(ECommonFrameRate) NumericType;

	FORCEINLINE static FFrameRate FPS_12()  { return AllFrameRates[(NumericType)ECommonFrameRate::FPS_12].FrameRate;  }
	FORCEINLINE static FFrameRate FPS_15()  { return AllFrameRates[(NumericType)ECommonFrameRate::FPS_15].FrameRate;  }
	FORCEINLINE static FFrameRate FPS_24()  { return AllFrameRates[(NumericType)ECommonFrameRate::FPS_24].FrameRate;  }
	FORCEINLINE static FFrameRate FPS_25()  { return AllFrameRates[(NumericType)ECommonFrameRate::FPS_25].FrameRate;  }
	FORCEINLINE static FFrameRate FPS_30()  { return AllFrameRates[(NumericType)ECommonFrameRate::FPS_30].FrameRate;  }
	FORCEINLINE static FFrameRate FPS_48()  { return AllFrameRates[(NumericType)ECommonFrameRate::FPS_48].FrameRate;  }
	FORCEINLINE static FFrameRate FPS_50()  { return AllFrameRates[(NumericType)ECommonFrameRate::FPS_50].FrameRate;  }
	FORCEINLINE static FFrameRate FPS_60()  { return AllFrameRates[(NumericType)ECommonFrameRate::FPS_60].FrameRate;  }
	FORCEINLINE static FFrameRate FPS_100() { return AllFrameRates[(NumericType)ECommonFrameRate::FPS_100].FrameRate; }
	FORCEINLINE static FFrameRate FPS_120() { return AllFrameRates[(NumericType)ECommonFrameRate::FPS_120].FrameRate; }
	FORCEINLINE static FFrameRate FPS_240() { return AllFrameRates[(NumericType)ECommonFrameRate::FPS_240].FrameRate; }

	FORCEINLINE static FFrameRate NTSC_24() { return AllFrameRates[(NumericType)ECommonFrameRate::NTSC_24].FrameRate; }
	FORCEINLINE static FFrameRate NTSC_30() { return AllFrameRates[(NumericType)ECommonFrameRate::NTSC_30].FrameRate; }
	FORCEINLINE static FFrameRate NTSC_60() { return AllFrameRates[(NumericType)ECommonFrameRate::NTSC_60].FrameRate; }

	static TArrayView<const FCommonFrameRateInfo> GetAll();

	static bool Contains(FFrameRate FrameRateToCheck)
	{
		return Find(FrameRateToCheck) != nullptr;
	}

	static const FCommonFrameRateInfo* Find(FFrameRate InFrameRate);

private:
	static const FCommonFrameRateInfo AllFrameRates[(int32)ECommonFrameRate::Private_Num];
};