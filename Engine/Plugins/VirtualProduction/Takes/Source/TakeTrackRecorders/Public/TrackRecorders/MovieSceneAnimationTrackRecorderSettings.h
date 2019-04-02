// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/FrameRate.h"
#include "MovieSceneTrackRecorderSettings.h"
#include "Curves/RichCurve.h"
#include "MovieSceneAnimationTrackRecorderSettings.generated.h"

UCLASS(Abstract, BlueprintType, config=EditorSettings, DisplayName="Animation Recorder Defaults")
class TAKETRACKRECORDERS_API UMovieSceneAnimationTrackRecorderEditorSettings : public UMovieSceneTrackRecorderSettings
{
	GENERATED_BODY()
public:
	UMovieSceneAnimationTrackRecorderEditorSettings(const FObjectInitializer& ObjInit)
		: Super(ObjInit)
	, AnimationTrackName(NSLOCTEXT("UMovieSceneAnimationTrackRecorderSettings", "DefaultAnimationTrackName", "RecordedAnimation"))
	, AnimationSubDirectory(TEXT("Animation"))
	, bRemoveRootAnimation(true)
	{
	}

	/** Name of the recorded animation track. */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Animation Recorder Settings")
	FText AnimationTrackName;

	/** The name of the subdirectory animations will be placed in. Leave this empty to place into the same directory as the sequence base path. */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Animation Recorder Settings")
	FString AnimationSubDirectory;
	
	/** Interpolation mode for the recorded keys. */
	UPROPERTY(EditAnywhere, Category = "Animation Recorder Settings", DisplayName = "Interpolation Mode")
	TEnumAsByte<ERichCurveInterpMode> InterpMode;

	/** Tangent mode for the recorded keys. */
	UPROPERTY(EditAnywhere, Category = "Animation Recorder Settings")
	TEnumAsByte<ERichCurveTangentMode> TangentMode;

	/** The following parameter is dynamically set based upon whether or not the animation was spawned dynamically via a blueprint or not, if so set to false, otherwise true */
	UPROPERTY()
	bool bRemoveRootAnimation;


};

UCLASS(BlueprintType, config = EditorSettings, DisplayName = "Animation Recorder Settings")
class TAKETRACKRECORDERS_API UMovieSceneAnimationTrackRecorderSettings : public UMovieSceneAnimationTrackRecorderEditorSettings
{
	GENERATED_BODY()
public:
	UMovieSceneAnimationTrackRecorderSettings(const FObjectInitializer& ObjInit)
		: Super(ObjInit)
	{
	}
};