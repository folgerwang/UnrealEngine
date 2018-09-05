// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/FrameRate.h"

#include "LiveLinkSourceSettings.generated.h"

UENUM()
enum class ELiveLinkSourceMode : uint8
{
	Default,				//! The source will be run in default mode.
							//! This mode will not attempt any type of interpolation, time synchronization,
							//! or other processing.

	Interpolated,			//! The source will be run in interpolated mode.
							//! This mode will use FLiveLinkInterpolationSettings and is most useful
							//! when smooth animation is desired.

	TimeSynchronized,		//! The source will be run in time synchronized mode.
							//! This mode will use FLiveLinkTimeSynchronizationSettings and is most useful
							//! when sources need to be synchronized with multiple other external inputs
							//! (such as video or other time synchronized sources).
							//! Don't use if the engine isn't setup with a Timecode provider.
};

USTRUCT()
struct FLiveLinkTimeSynchronizationSettings
{
	GENERATED_BODY()

	FLiveLinkTimeSynchronizationSettings() : FrameRate(60, 1) {}

	// The frame rate of the source.
	// This should be the frame rate the source is "stamped" at, not necessarily the frame rate the source is sending.
	// The source should supply this whenever possible.
	UPROPERTY(EditAnywhere, Category = Settings)
	FFrameRate FrameRate;
};
 
PRAGMA_DISABLE_DEPRECATION_WARNINGS

USTRUCT()
struct FLiveLinkInterpolationSettings
{
	GENERATED_BODY()

	FLiveLinkInterpolationSettings() : InterpolationOffset(0.5f) {}

	// Unused
	DEPRECATED(4.21, "Please use ULiveLinkSourceSettings::Mode to specify how the source will behave.")
	UPROPERTY()
	bool bUseInterpolation;

	// When interpolating: how far back from current time should we read the buffer (in seconds)
	UPROPERTY(EditAnywhere, Category = Settings)
	float InterpolationOffset;
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS

// Base class for live link source settings (can be replaced by sources themselves) 
UCLASS()
class LIVELINKINTERFACE_API ULiveLinkSourceSettings : public UObject
{
public:
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Mode")
	ELiveLinkSourceMode Mode = ELiveLinkSourceMode::Default;

	// Only used when Mode is set to Interpolated.
	UPROPERTY(EditAnywhere, Category = "Interpolation Settings")
	FLiveLinkInterpolationSettings InterpolationSettings;

	// Only used when Mode is set to TimeSynchronized.
	UPROPERTY(EditAnywhere, Category = "Time Synchronization Settings")
	FLiveLinkTimeSynchronizationSettings TimeSynchronizationSettings;

	virtual void Serialize(FArchive& Ar) override;
};
