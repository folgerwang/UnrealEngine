// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "AjaMediaFinder.h"
#include "CommonFrameRates.h"
#include "Misc/FrameRate.h"


struct FMediaPermutationsSelectorBuilder
{
	static const FName NAME_DeviceIndex;
	static const FName NAME_SourceType;
	static const FName NAME_QuadType;
	static const FName NAME_Resolution;
	static const FName NAME_Standard;
	static const FName NAME_FrameRate;

	static bool IdenticalProperty(FName ColumnName, const FAjaMediaConfiguration& Left, const FAjaMediaConfiguration& Right);
	static bool Less(FName ColumnName, const FAjaMediaConfiguration& Left, const FAjaMediaConfiguration& Right);
	static FText GetLabel(FName ColumnName, const FAjaMediaConfiguration& Item);
	static FText GetTooltip(FName ColumnName, const FAjaMediaConfiguration& Item);
};
