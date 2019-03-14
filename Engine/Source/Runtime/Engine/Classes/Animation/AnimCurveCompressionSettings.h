// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimCurveCompressionSettings.generated.h"

class UAnimCurveCompressionCodec;
class UAnimSequence;

/*
 * This object is used to wrap a curve compression codec. It allows a clean integration in the editor by avoiding the need
 * to create asset types and factory wrappers for every codec.
 */
UCLASS(hidecategories = Object)
class ENGINE_API UAnimCurveCompressionSettings : public UObject
{
	GENERATED_UCLASS_BODY()

	/** An animation curve compression codec. */
	UPROPERTY(Category = Compression, Instanced, EditAnywhere, NoClear)
	UAnimCurveCompressionCodec* Codec;

	//////////////////////////////////////////////////////////////////////////

	/** Allow us to convert DDC serialized path back into codec object */
	UAnimCurveCompressionCodec* GetCodec(const FString& Path);

#if WITH_EDITORONLY_DATA
	/** Returns whether or not we can use these settings to compress. */
	bool AreSettingsValid() const;

	/*
	 * Compresses the animation curves inside the supplied sequence.
	 * Note that this will modify the animation sequence by populating the compressed bytes
	 * and the codec used but it is left unchanged if compression fails.
	 */
	bool Compress(UAnimSequence& AnimSeq) const;

	/** Generates a DDC key that takes into account the current settings and selected codec. */
	FString MakeDDCKey() const;
#endif
};
