// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CoreTypes.h"
#include "Misc/Compression.h"
#include "Misc/ICompressionFormat.h"
#include "Misc/CommandLine.h"
#if HAS_OODLE_SDK
#include "oodle2.h"
#endif

#if HAS_OODLE_SDK
DEFINE_LOG_CATEGORY_STATIC(OodleCompression, Log, All);



struct FOodleCustomCompressor : ICompressionFormat
{
	bool bInitialized;
	OodleLZ_Compressor Compressor;
	OodleLZ_CompressionLevel CompressionLevel;
	OodleLZ_CompressOptions CompressionOptions;
	int SpaceSpeedTradeoffBytes;


	FOodleCustomCompressor(OodleLZ_Compressor InCompressor, OodleLZ_CompressionLevel InCompressionLevel, int InSpaceSpeedTradeoffBytes)
		: bInitialized(false)
		, SpaceSpeedTradeoffBytes(InSpaceSpeedTradeoffBytes)
	{
		Compressor = InCompressor;
		CompressionLevel = InCompressionLevel;
	}

	inline void ConditionalInitialize()
	{
		if (bInitialized)
		{
			return;
		}

		CompressionOptions = *OodleLZ_CompressOptions_GetDefault(Compressor, CompressionLevel);
		CompressionOptions.spaceSpeedTradeoffBytes = SpaceSpeedTradeoffBytes;

		bInitialized = true;
	}

	virtual FName GetCompressionFormatName()
	{
		return TEXT("Oodle");
	}

	virtual bool Compress(void* CompressedBuffer, int32& CompressedSize, const void* UncompressedBuffer, int32 UncompressedSize, int32 CompressionData)
	{
		ConditionalInitialize();

		int32 Result = (int32)OodleLZ_Compress(Compressor, UncompressedBuffer, UncompressedSize, CompressedBuffer, CompressionLevel, &CompressionOptions);
		if (Result > 0)
		{
			if (Result > GetCompressedBufferSize(UncompressedSize, CompressionData))
			{
				FPlatformMisc::LowLevelOutputDebugStringf(TEXT("%d < %d"), Result, GetCompressedBufferSize(UncompressedSize, CompressionData));
				// we cannot safely go over the BufferSize needed!
				return false;
			}
			CompressedSize = Result;
			return true;
		}
		return false;
	}

	virtual bool Uncompress(void* UncompressedBuffer, int32& UncompressedSize, const void* CompressedBuffer, int32 CompressedSize, int32 CompressionData)
	{
		ConditionalInitialize();

		int32 Result = (int32)OodleLZ_Decompress(CompressedBuffer, CompressedSize, UncompressedBuffer, UncompressedSize, OodleLZ_FuzzSafe_No);
		if (Result > 0)
		{
			UncompressedSize = Result;
			return true;
		}
		return false;
	}

	virtual int32 GetCompressedBufferSize(int32 UncompressedSize, int32 CompressionData)
	{
		ConditionalInitialize();

		int32 Needed = (int32)OodleLZ_GetCompressedBufferSizeNeeded(UncompressedSize);
		return Needed;
	}
};

#endif

#include "Misc/ICompressionFormat.h"

class FOodleCompressionFormatModuleInterface : public IModuleInterface
{
	virtual void StartupModule() override
	{
#if HAS_OODLE_SDK
		FString Method = TEXT("Mermaid");
		FString Level = TEXT("Normal");
		int32 SpaceSpeedTradeoff = 256;

		// let commandline override
		FParse::Value(FCommandLine::Get(), TEXT("OodleMethod="), Method);
		FParse::Value(FCommandLine::Get(), TEXT("OodleLevel="), Level);
		FParse::Value(FCommandLine::Get(), TEXT("OodleSpaceSpeedTradeoff="), SpaceSpeedTradeoff);

		// convert values to enums
		TMap<FString, OodleLZ_Compressor> MethodMap = { 
			{ TEXT("Mermaid"), OodleLZ_Compressor_Mermaid },
			{ TEXT("Kraken"), OodleLZ_Compressor_Kraken },
			{ TEXT("Selkie"), OodleLZ_Compressor_Selkie },
			{ TEXT("LZNA"), OodleLZ_Compressor_LZNA },
			{ TEXT("BitKnit"), OodleLZ_Compressor_BitKnit },
			{ TEXT("LZB16"), OodleLZ_Compressor_LZB16 },
		};
		TMap<FString, OodleLZ_CompressionLevel> LevelMap = { 
			{ TEXT("None"), OodleLZ_CompressionLevel_None },
			{ TEXT("RLE"), OodleLZ_CompressionLevel_RLE },
			{ TEXT("VeryFast"), OodleLZ_CompressionLevel_VeryFast },
			{ TEXT("Fast"), OodleLZ_CompressionLevel_Fast },
			{ TEXT("Normal"), OodleLZ_CompressionLevel_Normal },
			{ TEXT("Optimal1"), OodleLZ_CompressionLevel_Optimal1 },
			{ TEXT("Optimal2"), OodleLZ_CompressionLevel_Optimal2 },
			{ TEXT("Optimal3"), OodleLZ_CompressionLevel_Optimal3 },
		};

		OodleLZ_Compressor UsedCompressor = MethodMap.FindRef(Method);
		OodleLZ_CompressionLevel UsedLevel = LevelMap.FindRef(Level);

		UE_LOG(OodleCompression, Display, TEXT("Oodle Compressing with %s, level %s, SpaceSpeed tradeoff %d"), **MethodMap.FindKey(UsedCompressor), **LevelMap.FindKey(UsedLevel), SpaceSpeedTradeoff, *GEngineIni);

		CompressionFormat = new FOodleCustomCompressor(MethodMap.FindRef(Method), LevelMap.FindRef(Level), SpaceSpeedTradeoff);

		IModularFeatures::Get().RegisterModularFeature(COMPRESSION_FORMAT_FEATURE_NAME, CompressionFormat);
#endif
	}

	virtual void ShutdownModule() override
	{
#if HAS_OODLE_SDK
		IModularFeatures::Get().UnregisterModularFeature(COMPRESSION_FORMAT_FEATURE_NAME, CompressionFormat);
		delete CompressionFormat;
#endif
	}

	ICompressionFormat* CompressionFormat = nullptr;
};

IMPLEMENT_MODULE(FOodleCompressionFormatModuleInterface, OodleCompressionFormat);