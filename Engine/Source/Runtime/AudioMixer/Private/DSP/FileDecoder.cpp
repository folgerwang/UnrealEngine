// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DSP/FileDecoder.h"
#include "OpusAudioInfo.h"
#include "VorbisAudioInfo.h"
#include "HAL/PlatformFilemanager.h"

FAudioFileReader::FAudioFileReader(const FString& InPath)
{
	QualityInfo = { 0 };

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	FileHandle.Reset(PlatformFile.OpenRead(*InPath));
	if (FileHandle.IsValid())
	{
		int64 FileSize = FileHandle->Size();
		CompressedFile.Reset();
		CompressedFile.AddUninitialized(FileSize);
		FileHandle->Read(CompressedFile.GetData(), FileSize);
		
		Decompressor.Reset(GetNewDecompressorForFile(InPath));

		if (Decompressor.IsValid())
		{
			Decompressor->ReadCompressedInfo(CompressedFile.GetData(), FileSize, &QualityInfo);
		}
		else
		{
			QualityInfo.NumChannels = 0;
			UE_LOG(LogTemp, Error, TEXT("Invalid file extension!"));
		}
	}
	else
	{
		QualityInfo.NumChannels = 0;
		UE_LOG(LogTemp, Error, TEXT("Invalid file %s!"), *InPath);
	}
}

void FAudioFileReader::GetFileInfo(FSoundQualityInfo& OutInfo)
{
	OutInfo = QualityInfo;
}

bool FAudioFileReader::PopAudio(float* OutAudio, int32 NumSamples)
{
	check(FileHandle.IsValid());
	check(Decompressor.IsValid());

	DecompressionBuffer.Reset();
	DecompressionBuffer.AddUninitialized(NumSamples);

	bool bIsFinished = Decompressor->ReadCompressedData((uint8*) DecompressionBuffer.GetData(), false, NumSamples * sizeof(Audio::DefaultUSoundWaveSampleType));

	// Convert to float:
	for (int32 Index = 0; Index < NumSamples; Index++)
	{
		OutAudio[Index] = ((float)DecompressionBuffer[Index]) / 32768.0f;
	}

	return bIsFinished;
}

ICompressedAudioInfo* FAudioFileReader::GetNewDecompressorForFile(const FString& InPath)
{
	FString Extension = GetExtensionForFile(InPath);

#if !PLATFORM_TVOS && !PLATFORM_HTML5
	static const FString OpusExtension = TEXT("opus");
	static const FString OggExtension = TEXT("ogg");

	if (Extension.Equals(OpusExtension))
	{
		return new FOpusAudioInfo();
	}
	else if (Extension.Equals(OggExtension))
	{
		return new FVorbisAudioInfo();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid file extension %s."), *Extension);
		return nullptr;
	}
#else
	UE_LOG(LogTemp, Error, TEXT("FAudioFileReader is not supported on this platform."), *Extension);
	return nullptr;
#endif // !PLATFORM_TVOS && !PLATFORM_HTML5
}

FString FAudioFileReader::GetExtensionForFile(const FString& InPath)
{
	int32 Index = INDEX_NONE;
	if (InPath.FindLastChar(TCHAR('.'), Index))
	{
		return InPath.RightChop(Index + 1);
	}
	else
	{
		return FString();
	}
}
