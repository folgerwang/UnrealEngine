// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Widgets/LayerManager/STooltipPresenter.h"
#include "Layout/LayoutUtils.h"
#include "Framework/Application/SlateApplication.h"

void STooltipPresenter::Construct(const FArguments& InArgs)
{
	this->ChildSlot.AttachWidget(InArgs._Content.Widget);
	SetCanTick(false);
}

void STooltipPresenter::SetContent(const TSharedRef<SWidget>& InWidget)
{
	ChildSlot.AttachWidget(InWidget);
}

void STooltipPresenter::OnArrangeChildren(const FGeometry& AllottedGeometry, FArrangedChildren& ArrangedChildren) const
{
	static const FVector2D CursorSize = FVector2D(12, 12);

	// Cached geometry is in desktop space.  We need to convert from desktop space where the mouse is to local space so use CachedGeometry.
	const FVector2D LocalCursorPosition = GetCachedGeometry().AbsoluteToLocal(FSlateApplication::Get().GetCursorPos());

	const FSlateRect CursorAnchorRect(LocalCursorPosition, LocalCursorPosition + CursorSize);
	const FSlateRect TooltipPopup(LocalCursorPosition + CursorSize, LocalCursorPosition + CursorSize + ChildSlot.GetWidget()->GetDesiredSize());
	
	const FVector2D TooltipPosition = ComputePopupFitInRect(CursorAnchorRect, TooltipPopup, EOrientation::Orient_Vertical, FSlateRect(FVector2D::ZeroVector, AllottedGeometry.GetLocalSize()));
	
	// We round the final tooltip position so that our tooltip doesn't begin at a half pixel offset, which avoids the contents of the tooltip
	// jittering in relation to one another.
	const FVector2D TooltipPositionRounded = AllottedGeometry.LocalToRoundedLocal(TooltipPosition);

	ArrangedChildren.AddWidget(AllottedGeometry.MakeChild(
		ChildSlot.GetWidget(),
		ChildSlot.GetWidget()->GetDesiredSize(),
		FSlateLayoutTransform(TooltipPositionRounded)
	));
}

FVector2D STooltipPresenter::ComputeDesiredSize( float ) const
{
	return ChildSlot.GetWidget()->GetDesiredSize();
}

FChildren* STooltipPresenter::GetChildren()
{
	return &ChildSlot;
}
