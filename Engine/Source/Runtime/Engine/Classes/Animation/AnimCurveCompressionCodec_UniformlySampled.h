// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
* Stores the raw rich curves as FCompressedRichCurve internally with optional key reduction and key time quantization.
*/

#include "CoreMinimal.h"
#include "Animation/AnimCurveCompressionCodec.h"
#include "AnimCurveCompressionCodec_UniformlySampled.generated.h"

UCLASS(meta = (DisplayName = "Uniformly Sampled"))
class ENGINE_API UAnimCurveCompressionCodec_UniformlySampled : public UAnimCurveCompressionCodec
{
	GENERATED_UCLASS_BODY()

#if WITH_EDITORONLY_DATA
	/** Whether to use the animation sequence sample rate or an explicit value */
	UPROPERTY(Category = Compression, EditAnywhere)
	bool UseAnimSequenceSampleRate;

	/** Sample rate to use when uniformly sampling */
	UPROPERTY(Category = Compression, EditAnywhere, meta = (ClampMin = "0", EditCondition = "!UseAnimSequenceSampleRate"))
	float SampleRate;
#endif

	//////////////////////////////////////////////////////////////////////////

#if WITH_EDITORONLY_DATA
	// UAnimCurveCompressionCodec overrides
	virtual bool Compress(const UAnimSequence& AnimSeq, FAnimCurveCompressionResult& OutResult) override;
	virtual void PopulateDDCKey(FArchive& Ar) override;
#endif

	virtual void DecompressCurves(const UAnimSequence& AnimSeq, FBlendedCurve& Curves, float CurrentTime) const override;
	virtual float DecompressCurve(const UAnimSequence& AnimSeq, SmartName::UID_Type CurveUID, float CurrentTime) const override;
};
