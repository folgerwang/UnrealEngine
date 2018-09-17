// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Widgets/MediaPermutationsSelectorBuilder.h"


#define LOCTEXT_NAMESPACE "AjaMediaPermutationsSelectorBuilder"

const FName FMediaPermutationsSelectorBuilder::NAME_DeviceIndex = "DeviceIndex";
const FName FMediaPermutationsSelectorBuilder::NAME_SourceType = "SourceType";
const FName FMediaPermutationsSelectorBuilder::NAME_QuadType = "QuadType";
const FName FMediaPermutationsSelectorBuilder::NAME_Resolution = "Resolution";
const FName FMediaPermutationsSelectorBuilder::NAME_Standard = "Standard";
const FName FMediaPermutationsSelectorBuilder::NAME_FrameRate = "FrameRate";

bool FMediaPermutationsSelectorBuilder::IdenticalProperty(FName ColumnName, const FAjaMediaConfiguration& Left, const FAjaMediaConfiguration& Right)
{
	if (ColumnName == NAME_DeviceIndex) return Left.MediaPort.DeviceIndex == Right.MediaPort.DeviceIndex;
	if (ColumnName == NAME_SourceType) return Left.MediaPort.LinkType == Right.MediaPort.LinkType && Left.MediaPort.PortIndex == Right.MediaPort.PortIndex;
	if (ColumnName == NAME_QuadType) return Left.MediaPort.LinkType == EAjaLinkType::QuadLink ? Left.MediaPort.QuadLinkType == Right.MediaPort.QuadLinkType : true;
	if (ColumnName == NAME_Resolution) return Left.MediaMode.TargetSize == Right.MediaMode.TargetSize;
	if (ColumnName == NAME_Standard) return Left.MediaMode.bIsProgressiveStandard == Right.MediaMode.bIsProgressiveStandard && Left.MediaMode.bIsPsfStandard == Right.MediaMode.bIsPsfStandard;
	if (ColumnName == NAME_FrameRate) return Left.MediaMode.FrameRate == Right.MediaMode.FrameRate;
	check(false);
	return false;
}

bool FMediaPermutationsSelectorBuilder::Less(FName ColumnName, const FAjaMediaConfiguration& Left, const FAjaMediaConfiguration& Right)
{
	if (ColumnName == NAME_DeviceIndex)
	{
		return Left.MediaPort.DeviceIndex < Right.MediaPort.DeviceIndex;
	}
	if (ColumnName == NAME_SourceType)
	{
		if (Left.MediaPort.LinkType == Right.MediaPort.LinkType)
		{
			return Left.MediaPort.PortIndex < Right.MediaPort.PortIndex;
		}
		return (int32)Left.MediaPort.LinkType < (int32)Right.MediaPort.LinkType;
	}

	if (ColumnName == NAME_Resolution)
	{
		if (Left.MediaMode.TargetSize.X == Right.MediaMode.TargetSize.X)
		{
			return Left.MediaMode.TargetSize.Y < Right.MediaMode.TargetSize.Y;
		}
		return Left.MediaMode.TargetSize.X < Right.MediaMode.TargetSize.X;
	}

	if (ColumnName == NAME_QuadType)
	{
		if (Left.MediaPort.LinkType == EAjaLinkType::QuadLink)
		{
			return (int32)Left.MediaPort.QuadLinkType < (int32)Right.MediaPort.QuadLinkType;
		}
		return true;
	}

	if (ColumnName == NAME_Standard)
	{
		if (Left.MediaMode.bIsProgressiveStandard == Right.MediaMode.bIsProgressiveStandard)
		{
			return Left.MediaMode.bIsPsfStandard;
		}
		return Left.MediaMode.bIsProgressiveStandard;
	}

	if (ColumnName == NAME_FrameRate)
	{
		return Left.MediaMode.FrameRate.AsDecimal() < Right.MediaMode.FrameRate.AsDecimal();
	}

	check(false);
	return false;
}

FText FMediaPermutationsSelectorBuilder::GetLabel(FName ColumnName, const FAjaMediaConfiguration& Item)
{
	if (ColumnName == NAME_DeviceIndex) return FText::FromName(Item.MediaPort.DeviceName);
	if (ColumnName == NAME_SourceType) return FAjaMediaFinder::LinkTypeToPrettyText(Item.MediaPort.LinkType, Item.MediaPort.PortIndex, false);
	if (ColumnName == NAME_QuadType) return FAjaMediaFinder::QuadLinkTypeToPrettyText(Item.MediaPort.QuadLinkType);
	if (ColumnName == NAME_Resolution) return FAjaMediaFinder::ResolutionToPrettyText(Item.MediaMode.TargetSize);
	if (ColumnName == NAME_Standard) return Item.MediaMode.bIsProgressiveStandard ? LOCTEXT("Progressive", "Progressive") : (Item.MediaMode.bIsPsfStandard ? LOCTEXT("psf", "psf") : LOCTEXT("Interlaced", "Interlaced"));
	if (ColumnName == NAME_FrameRate) return Item.MediaMode.FrameRate.ToPrettyText();
	check(false);
	return FText::GetEmpty();
}

FText FMediaPermutationsSelectorBuilder::GetTooltip(FName ColumnName, const FAjaMediaConfiguration& Item)
{
	if (ColumnName == NAME_DeviceIndex) return FText::FromString(FString::Printf(TEXT("%s as index: %d"), *Item.MediaPort.DeviceName.ToString(), Item.MediaPort.DeviceIndex));
	if (ColumnName == NAME_SourceType) return FText::GetEmpty();
	if (ColumnName == NAME_QuadType) return FText::GetEmpty();
	if (ColumnName == NAME_Resolution) return FText::FromString(FString::Printf(TEXT("%dx%d"), Item.MediaMode.TargetSize.X, Item.MediaMode.TargetSize.Y));
	if (ColumnName == NAME_Standard) return FText::GetEmpty();
	if (ColumnName == NAME_FrameRate)
	{
		if (const FCommonFrameRateInfo* Found = FCommonFrameRates::Find(Item.MediaMode.FrameRate))
		{
			return Found->Description;
		}
		return Item.MediaMode.FrameRate.ToPrettyText();
	}
	check(false);
	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
