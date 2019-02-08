// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Widgets/SMediaImage.h"

#include "EditorStyleSet.h"
#include "Materials/Material.h"
#include "Styling/SlateBrush.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"


SMediaImage::SMediaImage()
	: Collector(this)
	, Material(nullptr)
	, MaterialBrush(nullptr)
	, TextureSampler(nullptr)
{ }

void SMediaImage::Construct(const FArguments& InArgs, UTexture* InTexture)
{
	// The Slate brush that renders the material.
	BrushImageSize = InArgs._BrushImageSize;

	if (InTexture != nullptr)
	{
		// create wrapper material
		Material = NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient);

		if (Material != nullptr)
		{
			TextureSampler = NewObject<UMaterialExpressionTextureSample>(Material);
			{
				TextureSampler->Texture = InTexture;
				TextureSampler->AutoSetSampleType();
			}

			FExpressionOutput& Output = TextureSampler->GetOutputs()[0];
			FExpressionInput& Input = Material->EmissiveColor;
			{
				Input.Expression = TextureSampler;
				Input.Mask = Output.Mask;
				Input.MaskR = Output.MaskR;
				Input.MaskG = Output.MaskG;
				Input.MaskB = Output.MaskB;
				Input.MaskA = Output.MaskA;
			}

			Material->Expressions.Add(TextureSampler);
			Material->MaterialDomain = EMaterialDomain::MD_UI;
			Material->PostEditChange();
		}

		// create Slate brush
		MaterialBrush = MakeShareable(new FSlateBrush());
		{
			MaterialBrush->SetResourceObject(Material);
		}
	}

	ChildSlot
	[
		SNew(SScaleBox)
		.Stretch_Lambda([]() -> EStretch::Type { return EStretch::Fill;	})
		[
			SNew(SImage)
			.Image(MaterialBrush.IsValid() ? MaterialBrush.Get() : FEditorStyle::GetBrush("WhiteTexture"))
		]
	];
}

void SMediaImage::Tick(const FGeometry& InAllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SCompoundWidget::Tick(InAllottedGeometry, InCurrentTime, InDeltaTime);

	if (BrushImageSize.IsSet())
	{
		FVector2D Size = BrushImageSize.Get();
		MaterialBrush->ImageSize.X = Size.X;
		MaterialBrush->ImageSize.Y = Size.Y;
	}
}
