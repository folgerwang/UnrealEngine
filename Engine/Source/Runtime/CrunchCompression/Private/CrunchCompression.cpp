// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.


#include "CrunchCompression.h"
#include "Modules/ModuleManager.h"
#include "PixelFormat.h"
#include "HAL/IConsoleManager.h"

#ifndef CRUNCH_SUPPORT
#define CRUNCH_SUPPORT 0
#endif

const bool GAdaptiveBlockSizes = true;
static TAutoConsoleVariable<int32> CVarCrunchQuality(
	TEXT("crn.quality"),
	128,
	TEXT("Set the quality of the crunch texture compression. [0, 255], default: 128"));

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4018) // '<': signed/unsigned mismatch
#endif

//Crunch contains functions that are called 'check' and so they conflict with the UE check macro.
#undef check

#if CRUNCH_SUPPORT

#if WITH_EDITOR
	#include "crnlib.h"
#endif
#include "crn_decomp.h"

#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

class FCrunchCompressionModule : public IModuleInterface
{
};
IMPLEMENT_MODULE(FCrunchCompressionModule, CrunchCompression);

#if WITH_EDITOR
static FName NameDXT1(TEXT("DXT1"));
static FName NameDXT5(TEXT("DXT5"));

#if CRUNCH_SUPPORT
static crn_format GetCrnFormat(const FName& Format)
{
	if (Format == NameDXT1)			return cCRNFmtDXT1;
	else if (Format == NameDXT5)	return cCRNFmtDXT5;
	else							return cCRNFmtInvalid;
}
#endif

static uint8 GetFormat(const FName& Format)
{
	if (Format == NameDXT1)			return PF_DXT1;
	else if (Format == NameDXT5)	return PF_DXT5;
	else							return PF_Unknown;
}

bool CrunchCompression::IsValidFormat(const FName& Format)
{
#if CRUNCH_SUPPORT
	return GetCrnFormat(Format) != cCRNFmtInvalid;
#endif
	return false;
}

TArray<uint32> ImageAsPackedRGBA(const FImage& image)
{
	TArray<uint32> PackedRGBA;

	const uint32 NumPixels = image.SizeX * image.SizeY;
	PackedRGBA.AddUninitialized(NumPixels);
	const FColor* pixels = image.AsBGRA8();
	for (uint32 i = 0; i < NumPixels; ++i)
	{
		PackedRGBA[i] = (*pixels).ToPackedABGR();
		pixels++;
	}
	
	return PackedRGBA;
}


bool CrunchCompression::Encode(const TArray<FImage>& UncompressedSrc, const FName& OutputFormat, TArray<uint8>& OutCodecPayload, TArray<uint8>& OutCompressedData, TArray< TPair<uint32, uint32>>& OutTileInfos)
{
	return false;
#if CRUNCH_SUPPORT
	crn_comp_params comp_params;
	comp_params.clear();

	comp_params.m_width = UncompressedSrc[0].SizeX;
	comp_params.m_height = UncompressedSrc[0].SizeY;	
	comp_params.m_levels = UncompressedSrc.Num();
	comp_params.set_flag(cCRNCompFlagPerceptual, UncompressedSrc[0].IsGammaCorrected() == false);
	comp_params.set_flag(cCRNCompFlagHierarchical, GAdaptiveBlockSizes);
	comp_params.set_flag(cCrnCompFlagUniformMips, true);
	comp_params.m_format = GetCrnFormat(OutputFormat);
	const int32 quality = FMath::Clamp(CVarCrunchQuality.GetValueOnAnyThread(), (int32)cCRNMinQualityLevel, (int32)cCRNMaxQualityLevel);
	comp_params.m_quality_level = quality;
	comp_params.m_num_helper_threads = 0;
	comp_params.m_pProgress_func = nullptr;

	TArray<TArray<uint32>> ConvertedImages;
	ConvertedImages.AddDefaulted(UncompressedSrc.Num());
	for (int SubImageIdx = 0; SubImageIdx < UncompressedSrc.Num(); ++SubImageIdx)
	{
		ConvertedImages[SubImageIdx] = ImageAsPackedRGBA(UncompressedSrc[SubImageIdx]);
		comp_params.m_pImages[0][SubImageIdx] = ConvertedImages[SubImageIdx].GetData();
	}
	

	crn_uint32 ActualQualityLevel;
	crn_uint32 OutputSize;
	float BitRate = 0;
	void* RawOutput = crn_compress(comp_params, OutputSize, &ActualQualityLevel, &BitRate);
	if (!RawOutput)
	{
		return false;
	}

	auto Cleanup = [RawOutput]() {
		crn_free_block(RawOutput);
	};
	
	crnd::crn_texture_info TexInfo;
	if (!crnd::crnd_get_texture_info(RawOutput, OutputSize, &TexInfo))
	{
		Cleanup();
		return false;
	}

	const uint32 headerSize = crnd::crnd_get_segmented_file_size(RawOutput, OutputSize);
	OutCodecPayload.AddUninitialized(headerSize);
	if (!crnd::crnd_create_segmented_file(RawOutput, OutputSize, OutCodecPayload.GetData(), headerSize))
	{
		Cleanup();
		return false;
	}
	
	const void* PixelData = crnd::crnd_get_level_data(RawOutput, OutputSize, 0, nullptr);
	if (!PixelData)
	{
		Cleanup();
		return false;
	}
	crn_uint32 PixelDataSize = OutputSize - headerSize;
	OutCompressedData.AddUninitialized(PixelDataSize);
	FMemory::Memcpy(OutCompressedData.GetData(), PixelData, PixelDataSize);

	OutTileInfos.AddUninitialized(UncompressedSrc.Num());
	for (int SubImageIdx = 0; SubImageIdx < UncompressedSrc.Num(); ++SubImageIdx)
	{
		crnd::uint32 DataSize = 0;
		uint32 offset = crnd::crnd_get_segmented_level_offset(RawOutput, OutputSize, SubImageIdx, &DataSize);
		OutTileInfos[SubImageIdx] = TPair<uint32, uint32>(offset, DataSize);
	}

	Cleanup();

	return true;
#endif
}
#endif

struct CrunckContext
{
#if CRUNCH_SUPPORT
	crnd::crnd_unpack_context CrnContext = nullptr;
#endif
	uint8* Header = nullptr;
	size_t HeaderSize = 0;
};

void* CrunchCompression::InitializeDecoderContext(uint8* HeaderData, size_t HeaderDataSize)
{
#if CRUNCH_SUPPORT
	CrunckContext* context = new CrunckContext();
	context->CrnContext = crnd::crnd_unpack_begin(HeaderData, HeaderDataSize);
	ensure(context->CrnContext);
	context->Header = HeaderData;
	context->HeaderSize = HeaderDataSize;
	return context;
#endif
	return nullptr;
}

bool CrunchCompression::Decode(void* Context, uint8* CompressedPixelData, uint32 Slice, uint8* OutUncompressedData, size_t DataSize, size_t UncompressedDataPitch)
{
#if CRUNCH_SUPPORT
	CrunckContext* context = (CrunckContext*)Context;
	bool success = crnd::crnd_unpack_level_segmented(context->CrnContext, CompressedPixelData, Slice, (void**)&OutUncompressedData, DataSize, UncompressedDataPitch, 0);
	return success;
#endif
	return false;
}

void CrunchCompression::DestroyDecoderContext(void* Context)
{
#if CRUNCH_SUPPORT
	CrunckContext* context = (CrunckContext*)Context;
	crnd::crnd_unpack_end(context->CrnContext);
	delete context;
#endif
}

