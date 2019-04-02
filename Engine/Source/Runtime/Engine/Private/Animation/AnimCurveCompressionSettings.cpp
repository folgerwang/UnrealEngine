// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimCurveCompressionSettings.h"
#include "Animation/AnimCurveCompressionCodec_CompressedRichCurve.h"
#include "Animation/AnimSequence.h"
#include "Serialization/MemoryWriter.h"

UAnimCurveCompressionSettings::UAnimCurveCompressionSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Codec = CreateDefaultSubobject<UAnimCurveCompressionCodec_CompressedRichCurve>(TEXT("CurveCompressionCodec"));
	Codec->SetFlags(RF_Transactional);
}

UAnimCurveCompressionCodec* UAnimCurveCompressionSettings::GetCodec(const FString& Path)
{
	return Codec->GetCodec(Path);
}

#if WITH_EDITORONLY_DATA
bool UAnimCurveCompressionSettings::AreSettingsValid() const
{
	return Codec != nullptr && Codec->IsCodecValid();
}

bool UAnimCurveCompressionSettings::Compress(UAnimSequence& AnimSeq) const
{
	if (Codec == nullptr || !AreSettingsValid())
	{
		return false;
	}

	FAnimCurveCompressionResult CompressionResult;
	bool Success = Codec->Compress(AnimSeq, CompressionResult);
	if (Success)
	{
		AnimSeq.CompressedCurveByteStream = CompressionResult.CompressedBytes;
		AnimSeq.CurveCompressionCodec = CompressionResult.Codec;
	}

	return Success;
}

FString UAnimCurveCompressionSettings::MakeDDCKey() const
{
	if (Codec == nullptr)
	{
		return TEXT("<Missing Codec>");
	}

	TArray<uint8> TempBytes;
	TempBytes.Reserve(64);

	// Serialize the compression settings into a temporary array. The archive
	// is flagged as persistent so that machines of different endianness produce
	// identical binary results.
	FMemoryWriter Ar(TempBytes, /*bIsPersistent=*/ true);

	Codec->PopulateDDCKey(Ar);

	FString Key;
	Key.Reserve(TempBytes.Num() + 1);
	for (int32 ByteIndex = 0; ByteIndex < TempBytes.Num(); ++ByteIndex)
	{
		ByteToHex(TempBytes[ByteIndex], Key);
	}

	return Key;
}
#endif
