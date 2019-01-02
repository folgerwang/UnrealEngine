// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "MovieSceneSection.h"
#include "MovieSceneKeyStruct.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "MovieSceneColorSection.generated.h"

struct FPropertyChangedEvent;

/**
 * Proxy structure for color section key data.
 */
USTRUCT()
struct FMovieSceneColorKeyStruct
	: public FMovieSceneKeyStruct
{
	GENERATED_BODY()

	/** The key's color value. */
	UPROPERTY(EditAnywhere, Category=Key, meta=(InlineColorPicker))
	FLinearColor Color;

	/** The key's time. */
	UPROPERTY(EditAnywhere, Category=Key)
	FFrameNumber Time;

	FMovieSceneKeyStructHelper KeyStructInterop;

	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent) override;
};
template<> struct TStructOpsTypeTraits<FMovieSceneColorKeyStruct> : public TStructOpsTypeTraitsBase2<FMovieSceneColorKeyStruct> { enum { WithCopy = false }; };


/**
 * A single floating point section
 */
UCLASS(MinimalAPI)
class UMovieSceneColorSection 
	: public UMovieSceneSection
{
	GENERATED_UCLASS_BODY()

public:

	/** Access the underlying generation function for the red component of this section */
	const FMovieSceneFloatChannel& GetRedChannel() const { return RedCurve; }

	/** Access the underlying generation function for the green component of this section */
	const FMovieSceneFloatChannel& GetGreenChannel() const { return GreenCurve; }

	/** Access the underlying generation function for the blue component of this section */
	const FMovieSceneFloatChannel& GetBlueChannel() const { return BlueCurve; }

	/** Access the underlying generation function for the alpha component of this section */
	const FMovieSceneFloatChannel& GetAlphaChannel() const { return AlphaCurve; }

protected:

	//~ UMovieSceneSection interface
	virtual TSharedPtr<FStructOnScope> GetKeyStruct(TArrayView<const FKeyHandle> KeyHandles) override;

private:

	/** Red curve data */
	UPROPERTY()
	FMovieSceneFloatChannel RedCurve;

	/** Green curve data */
	UPROPERTY()
	FMovieSceneFloatChannel GreenCurve;

	/** Blue curve data */
	UPROPERTY()
	FMovieSceneFloatChannel BlueCurve;

	/** Alpha curve data */
	UPROPERTY()
	FMovieSceneFloatChannel AlphaCurve;
};
