// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Widgets/Layout/SBorder.h"
#include "Rendering/DrawElements.h"

static FName SBorderTypeName("SBorder");

SBorder::SBorder()
	: BorderImage( FCoreStyle::Get().GetBrush( "Border" ) )
	, BorderBackgroundColor( FLinearColor::White )
	, DesiredSizeScale(FVector2D(1,1))
{
}

void SBorder::Construct( const SBorder::FArguments& InArgs )
{
	// Only do this if we're exactly an SBorder
	if ( GetType() == SBorderTypeName )
	{
		SetCanTick(false);
		bCanSupportFocus = false;
	}

	ContentScale = InArgs._ContentScale;
	ColorAndOpacity = InArgs._ColorAndOpacity;
	DesiredSizeScale = InArgs._DesiredSizeScale;

	ShowDisabledEffect = InArgs._ShowEffectWhenDisabled;

	BorderImage = InArgs._BorderImage;
	BorderBackgroundColor = InArgs._BorderBackgroundColor;
	ForegroundColor = InArgs._ForegroundColor;
	SetOnMouseButtonDown(InArgs._OnMouseButtonDown);
	SetOnMouseButtonUp(InArgs._OnMouseButtonUp);
	SetOnMouseMove(InArgs._OnMouseMove);
	SetOnMouseDoubleClick(InArgs._OnMouseDoubleClick);

	ChildSlot
	.HAlign(InArgs._HAlign)
	.VAlign(InArgs._VAlign)
	.Padding(InArgs._Padding)
	[
		InArgs._Content.Widget
	];
}

void SBorder::SetContent( TSharedRef< SWidget > InContent )
{
	ChildSlot
	[
		InContent
	];
}

const TSharedRef< SWidget >& SBorder::GetContent() const
{
	return ChildSlot.GetWidget();
}

void SBorder::ClearContent()
{
	ChildSlot.DetachWidget();
}

int32 SBorder::OnPaint( const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled ) const
{
	const FSlateBrush* BrushResource = BorderImage.Get();
		
	const bool bEnabled = ShouldBeEnabled(bParentEnabled);

	if ( BrushResource && BrushResource->DrawAs != ESlateBrushDrawType::NoDrawType )
	{
		const bool bShowDisabledEffect = ShowDisabledEffect.Get();
		const ESlateDrawEffect DrawEffects = (bShowDisabledEffect && !bEnabled) ? ESlateDrawEffect::DisabledEffect : ESlateDrawEffect::None;

		FSlateDrawElement::MakeBox(
			OutDrawElements,
			LayerId,
			AllottedGeometry.ToPaintGeometry(),
			BrushResource,
			DrawEffects,
			BrushResource->GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint() * BorderBackgroundColor.Get().GetColor(InWidgetStyle)
		);
	}

	return SCompoundWidget::OnPaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements, LayerId, InWidgetStyle, bEnabled );
}

FVector2D SBorder::ComputeDesiredSize(float LayoutScaleMultiplier) const
{
	return DesiredSizeScale.Get() * SCompoundWidget::ComputeDesiredSize(LayoutScaleMultiplier);
}

void SBorder::SetBorderBackgroundColor(const TAttribute<FSlateColor>& InColorAndOpacity)
{
	if (!BorderBackgroundColor.IdenticalTo(InColorAndOpacity))
	{
		BorderBackgroundColor = InColorAndOpacity;
		Invalidate(EInvalidateWidget::PaintAndVolatility);
	}
}

void SBorder::SetDesiredSizeScale(const TAttribute<FVector2D>& InDesiredSizeScale)
{
	if (!DesiredSizeScale.IdenticalTo(InDesiredSizeScale))
	{
		DesiredSizeScale = InDesiredSizeScale;
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

void SBorder::SetHAlign(EHorizontalAlignment HAlign)
{
	if (ChildSlot.HAlignment != HAlign)
	{
		ChildSlot.HAlignment = HAlign;
		Invalidate(EInvalidateWidget::Layout);
	}
}

void SBorder::SetVAlign(EVerticalAlignment VAlign)
{
	if (ChildSlot.VAlignment != VAlign)
	{
		ChildSlot.VAlignment = VAlign;
		Invalidate(EInvalidateWidget::Layout);
	}
}

void SBorder::SetPadding(const TAttribute<FMargin>& InPadding)
{
	if (!ChildSlot.SlotPadding.IdenticalTo(InPadding))
	{
		ChildSlot.SlotPadding = InPadding;
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

void SBorder::SetShowEffectWhenDisabled(const TAttribute<bool>& InShowEffectWhenDisabled)
{
	if (!ShowDisabledEffect.IdenticalTo(InShowEffectWhenDisabled))
	{
		ShowDisabledEffect = InShowEffectWhenDisabled;
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}

void SBorder::SetBorderImage(const TAttribute<const FSlateBrush*>& InBorderImage)
{
	if (!BorderImage.IdenticalTo(InBorderImage))
	{
		BorderImage = InBorderImage;
		Invalidate(EInvalidateWidget::LayoutAndVolatility);
	}
}
