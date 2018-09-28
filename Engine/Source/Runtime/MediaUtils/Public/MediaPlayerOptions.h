// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"

#include "MediaPlayerOptions.generated.h"

USTRUCT(BlueprintType)
struct FMediaPlayerTrackOptions
{
	GENERATED_BODY()

	FMediaPlayerTrackOptions() :
		Audio(0),
		Caption(-1),
		Metadata(-1),
		Script(-1),
		Subtitle(-1),
		Text(-1),
		Video(0)
	{
	}

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Audio;

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Caption;

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Metadata;

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Script;

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Subtitle;

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Text;

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	int32 Video;
};

USTRUCT(BlueprintType)
struct FMediaPlayerOptions
{
	GENERATED_BODY()

	FMediaPlayerOptions()
	{
	}

	UPROPERTY(BlueprintReadWrite, Category = "Tracks")
	FMediaPlayerTrackOptions Tracks;
};
