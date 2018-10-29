// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UCurveLinearColorAtlas.cpp
=============================================================================*/

#include "Curves/CurveLinearColorAtlas.h"
#include "Curves/CurveLinearColor.h"
#include "Components/MeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"


UCurveLinearColorAtlas::UCurveLinearColorAtlas(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITOR
	TextureSize = 256;
	GradientPixelSize = 1;
	bHasAnyDirtyTextures = false;
	bShowDebugColorsForNullGradients = false;
	SizeXY = { (float)TextureSize, (float)GradientPixelSize };
	MipGenSettings = TMGS_NoMipmaps;
#endif
	Filter = TextureFilter::TF_Bilinear;
	SRGB = false;
	AddressX = TA_Clamp;
	AddressY = TA_Clamp;
	CompressionSettings = TC_HDR;
}
#if WITH_EDITOR
void UCurveLinearColorAtlas::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	// Determine whether any property that requires recompression of the texture, or notification to Materials has changed.
	bool bRequiresNotifyMaterials = false;

	if (PropertyChangedEvent.Property != nullptr)
	{
		const FName PropertyName(PropertyChangedEvent.Property->GetFName());
		// if Resizing
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UCurveLinearColorAtlas, TextureSize))
		{
			if ((uint32)GradientCurves.Num() > TextureSize)
			{
				int32 OldCurveCount = GradientCurves.Num();
				GradientCurves.RemoveAt(TextureSize, OldCurveCount - TextureSize);
			}

			Source.Init(TextureSize, TextureSize, 1, 1, TSF_RGBA16F);

			SizeXY = { (float)TextureSize, (float)GradientPixelSize };
			UpdateTextures();
			bRequiresNotifyMaterials = true;
		}
		if (PropertyName == GET_MEMBER_NAME_CHECKED(UCurveLinearColorAtlas, GradientCurves))
		{
			if ((uint32)GradientCurves.Num() > TextureSize)
			{
				int32 OldCurveCount = GradientCurves.Num();
				GradientCurves.RemoveAt(TextureSize, OldCurveCount - TextureSize);
			}
			else
			{
				for (int32 i = 0; i < GradientCurves.Num(); ++i)
				{
					if (GradientCurves[i] != nullptr)
					{
						GradientCurves[i]->OnUpdateGradient.AddUObject(this, &UCurveLinearColorAtlas::UpdateGradientSlot);
					}
				}
				UpdateTextures();
				bRequiresNotifyMaterials = true;
			}
		}	
	}

	// Notify any loaded material instances if changed our compression format
	if (bRequiresNotifyMaterials)
	{
		NotifyMaterials();
	}
}
#endif

void UCurveLinearColorAtlas::PostLoad()
{
#if WITH_EDITOR
	for (int32 i = 0; i < GradientCurves.Num(); ++i)
	{
		if (GradientCurves[i] != nullptr)
		{
			GradientCurves[i]->OnUpdateGradient.AddUObject(this, &UCurveLinearColorAtlas::UpdateGradientSlot);
		}
	}
	Source.Init(TextureSize, TextureSize, 1, 1, TSF_RGBA16F);
	SizeXY = { (float)TextureSize, (float)GradientPixelSize };
	UpdateTextures();
#endif

	Super::PostLoad();
}

#if WITH_EDITOR
static void RenderGradient(TArray<FFloat16Color>& InSrcData, UObject* Gradient, int32 StartXY, FVector2D SizeXY)
{
	if (Gradient == nullptr)
	{
		int32 Start = StartXY;
		for (uint32 y = 0; y < SizeXY.Y; y++)
		{
			// Create base mip for the texture we created.
			for (uint32 x = 0; x < SizeXY.X; x++)
			{
				InSrcData[Start + x + y * SizeXY.X] = FLinearColor::White;
			}
		}
	}
	else if (Gradient->IsA(UCurveLinearColor::StaticClass()))
	{
		// Render a gradient
		UCurveLinearColor* GradientCurve = CastChecked<UCurveLinearColor>(Gradient);
		GradientCurve->PushToSourceData(InSrcData, StartXY, SizeXY);
	}
}

// Immediately render a new material to the specified slot index (SlotIndex must be within this section's range)
void UCurveLinearColorAtlas::UpdateGradientSlot(UCurveLinearColor* Gradient)
{
	check(Gradient);

	int32 SlotIndex = GradientCurves.Find(Gradient);

	if (SlotIndex != INDEX_NONE && (uint32)SlotIndex < MaxSlotsPerTexture())
	{
		// Determine the position of the gradient
		int32 StartXY = SlotIndex * TextureSize * GradientPixelSize;

		// Render the single gradient to the render target
		RenderGradient(SrcData, Gradient, StartXY, SizeXY);

		uint32* TextureData = (uint32*)Source.LockMip(0);
		const int32 TextureDataSize = Source.CalcMipSize(0);
		FMemory::Memcpy(TextureData, SrcData.GetData(), TextureDataSize);

		Source.UnlockMip(0);

		// Immediately update the texture
		UpdateResource();
	}
	
}

// Render any textures
void UCurveLinearColorAtlas::UpdateTextures()
{
	// Save off the data needed to render each gradient.
	// Callback into the section owner to get the Gradients array
	const int32 TextureDataSize = Source.CalcMipSize(0);
	SrcData.Empty();
	SrcData.AddUninitialized(TextureDataSize);

	int32 NumSlotsToRender = FMath::Min(GradientCurves.Num(), (int32)MaxSlotsPerTexture());
	for (int32 i = 0; i < NumSlotsToRender; ++i)
	{
		if (GradientCurves[i] != nullptr)
		{
			int32 StartXY = i * TextureSize * GradientPixelSize;
			RenderGradient(SrcData, GradientCurves[i], StartXY, SizeXY);
		}

	}

	for (uint32 y = 0; y < TextureSize; y++)
	{
		// Create base mip for the texture we created.
		for (uint32 x = GradientCurves.Num(); x < TextureSize; x++)
		{
			SrcData[x*TextureSize + y] = FLinearColor::White;
		}
	}

	uint32* TextureData = (uint32*)Source.LockMip(0);
	FMemory::Memcpy(TextureData, SrcData.GetData(), TextureDataSize);
	Source.UnlockMip(0);
	UpdateResource();

	bIsDirty = false;
}

#endif

bool UCurveLinearColorAtlas::GetCurveIndex(UCurveLinearColor* InCurve, int32& Index)
{
	Index = GradientCurves.Find(InCurve);
	if (Index != INDEX_NONE)
	{
		return true;
	}
	return false;
}

bool UCurveLinearColorAtlas::GetCurvePosition(UCurveLinearColor* InCurve, float& Position)
{
	int32 Index = GradientCurves.Find(InCurve);
	Position = 0.0f;
	if (Index != INDEX_NONE)
	{
		Position = ((float)Index * GradientPixelSize) / TextureSize + (0.5f * GradientPixelSize) / TextureSize;
		return true;
	}
	return false;
}