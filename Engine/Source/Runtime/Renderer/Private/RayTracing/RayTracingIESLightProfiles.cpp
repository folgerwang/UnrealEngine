// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "RayTracingIESLightProfiles.h"
#include "SceneRendering.h"

#if RHI_RAYTRACING

void FIESLightProfileResource::BuildIESLightProfilesTexture(const TArray<UTextureLightProfile*, SceneRenderingAllocator>& NewIESProfilesArray)
{
	// Rebuild 2D texture that contains one IES light profile per row

	check(IsInRenderingThread());

	bool NeedsRebuild = false;
	if (NewIESProfilesArray.Num() != IESTextureData.Num())
	{
		NeedsRebuild = true;
		IESTextureData.SetNum(NewIESProfilesArray.Num(), true);
	}
	else
	{
		for (int32 i = 0; i < IESTextureData.Num(); ++i)
		{
			if (IESTextureData[i] != NewIESProfilesArray[i])
			{
				NeedsRebuild = true;
				break;
			}
		}
	}

	uint32 NewArraySize = NewIESProfilesArray.Num();

	if (!NeedsRebuild || NewArraySize == 0)
	{
		return;
	}

	uint32 NumFloatsPerRow = AllowedIESProfileWidth() * 4;

	IESProfilesBulkData.SetNum(NewArraySize * NumFloatsPerRow);

	for (uint32 ProfileIndex = 0; ProfileIndex < NewArraySize; ++ProfileIndex)
	{
		IESTextureData[ProfileIndex] = NewIESProfilesArray[ProfileIndex];
		const UTextureLightProfile* LightProfileTexture = IESTextureData[ProfileIndex];
		const uint32 Offset = NumFloatsPerRow * ProfileIndex;
		FFloat16* DataPtr = &IESProfilesBulkData[Offset];

		if (IsIESTextureFormatValid(LightProfileTexture))
		{
			LightProfileTexture->PlatformData->Mips[0].BulkData.GetCopy((void**)&DataPtr, false);
		}
	}

	if (!TextureRHI || TextureRHI->GetSizeY() != NewArraySize)
	{
		FRHIResourceCreateInfo CreateInfo;
		const uint32 TexCreateFlags = TexCreate_Dynamic | TexCreate_NoTiling | TexCreate_ShaderResource;
		TextureRHI = RHICreateTexture2D(256, NewArraySize, PF_FloatRGBA, 1, 1, TexCreateFlags, CreateInfo);
	}

	uint32 DestStride;
	FFloat16* TargetPtr = (FFloat16*)RHILockTexture2D(TextureRHI, 0, RLM_WriteOnly, DestStride, false);
	{
		check(DestStride == sizeof(FFloat16) * NumFloatsPerRow);

		for (uint32 RowIndex = 0; RowIndex < NewArraySize; ++RowIndex)
		{
			FFloat16* Row = TargetPtr + RowIndex * DestStride / sizeof(FFloat16);
			const FFloat16* SourcePtr = IESProfilesBulkData.GetData() + NumFloatsPerRow * RowIndex;
			FPlatformMemory::Memcpy(Row, SourcePtr, sizeof(FFloat16) * NumFloatsPerRow);
		}
	}
	RHIUnlockTexture2D(TextureRHI, 0, false);
}

#endif
