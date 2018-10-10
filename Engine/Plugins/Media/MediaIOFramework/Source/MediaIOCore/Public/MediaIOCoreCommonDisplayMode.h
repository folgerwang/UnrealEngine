// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/ArrayView.h"
#include "Misc/FrameRate.h"

struct FMediaIOCommonDisplayModeInfo
{
	int32 Width;
	int32 Height;
	FFrameRate FrameRate;
	int32 FieldPerFrame;
	FText Name;
};

struct FMediaIOCommonDisplayModeResolutionInfo
{
	int32 Width;
	int32 Height;
	FText Name;
};

struct MEDIAIOCORE_API FMediaIOCommonDisplayModes
{
	static TArrayView<const FMediaIOCommonDisplayModeInfo> GetAllModes();
	static TArrayView<const FMediaIOCommonDisplayModeResolutionInfo> GetAllResolutions();

	static const FMediaIOCommonDisplayModeResolutionInfo* Find(int32 InWidth, int32 InHeight);
	static const FMediaIOCommonDisplayModeInfo* Find(int32 InWidth, int32 InHeight, const FFrameRate& InFrameRate, int32 InFieldPerFrame);
	static bool Contains(int32 InWidth, int32 InHeight) { return Find(InWidth, InHeight) != nullptr; }
	static bool Contains(int32 InWidth, int32 InHeight, const FFrameRate& InFrameRate, int32 InFieldPerFrame) { return Find(InWidth, InHeight, InFrameRate, InFieldPerFrame) != nullptr; }

	static FText GetMediaIOCommonDisplayModeResolutionInfoName(int32 InWidth, int32 InHeight);
	static FText GetMediaIOCommonDisplayModeInfoName(int32 InWidth, int32 InHeight, const FFrameRate& InFrameRate, int32 InFieldPerFrame);
};