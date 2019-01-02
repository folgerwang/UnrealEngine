// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Components/ScaleBoxSlot.h"
#include "Widgets/SNullWidget.h"
#include "Components/Widget.h"
#include "Widgets/Layout/SScaleBox.h"

/////////////////////////////////////////////////////
// UScaleBoxSlot

UScaleBoxSlot::UScaleBoxSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Padding = FMargin(0, 0);

	HorizontalAlignment = HAlign_Center;
	VerticalAlignment = VAlign_Center;
}

void UScaleBoxSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	ScaleBox.Reset();
}

void UScaleBoxSlot::BuildSlot(TSharedRef<SScaleBox> InScaleBox)
{
	ScaleBox = InScaleBox;

	//ScaleBox->SetPadding(Padding);
	ScaleBox.Pin()->SetHAlign(HorizontalAlignment);
	ScaleBox.Pin()->SetVAlign(VerticalAlignment);

	ScaleBox.Pin()->SetContent(Content ? Content->TakeWidget() : SNullWidget::NullWidget);
}

void UScaleBoxSlot::SetPadding(FMargin InPadding)
{
	Padding = InPadding;
	if ( ScaleBox.IsValid() )
	{
		//ScaleBox.Pin()->SetPadding(InPadding);
	}
}

void UScaleBoxSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	HorizontalAlignment = InHorizontalAlignment;
	if ( ScaleBox.IsValid() )
	{
		ScaleBox.Pin()->SetHAlign(InHorizontalAlignment);
	}
}

void UScaleBoxSlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	VerticalAlignment = InVerticalAlignment;
	if ( ScaleBox.IsValid() )
	{
		ScaleBox.Pin()->SetVAlign(InVerticalAlignment);
	}
}

void UScaleBoxSlot::SynchronizeProperties()
{
	SetPadding(Padding);
	SetHorizontalAlignment(HorizontalAlignment);
	SetVerticalAlignment(VerticalAlignment);
}
