// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "Components/ButtonSlot.h"
#include "Widgets/SNullWidget.h"
#include "Widgets/Input/SButton.h"
#include "Components/Widget.h"

/////////////////////////////////////////////////////
// UButtonSlot

UButtonSlot::UButtonSlot(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	Padding = FMargin(4, 2);

	HorizontalAlignment = HAlign_Center;
	VerticalAlignment = VAlign_Center;
}

void UButtonSlot::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	Button.Reset();
}

void UButtonSlot::BuildSlot(TSharedRef<SButton> InButton)
{
	Button = InButton;

	Button.Pin()->SetContentPadding(Padding);
	Button.Pin()->SetHAlign(HorizontalAlignment);
	Button.Pin()->SetVAlign(VerticalAlignment);

	Button.Pin()->SetContent(Content ? Content->TakeWidget() : SNullWidget::NullWidget);
}

void UButtonSlot::SetPadding(FMargin InPadding)
{
	Padding = InPadding;
	if ( Button.IsValid() )
	{
		Button.Pin()->SetContentPadding(InPadding);
	}
}

void UButtonSlot::SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment)
{
	HorizontalAlignment = InHorizontalAlignment;
	if ( Button.IsValid() )
	{
		Button.Pin()->SetHAlign(InHorizontalAlignment);
	}
}

void UButtonSlot::SetVerticalAlignment(EVerticalAlignment InVerticalAlignment)
{
	VerticalAlignment = InVerticalAlignment;
	if ( Button.IsValid() )
	{
		Button.Pin()->SetVAlign(InVerticalAlignment);
	}
}

void UButtonSlot::SynchronizeProperties()
{
	SetPadding(Padding);
	SetHorizontalAlignment(HorizontalAlignment);
	SetVerticalAlignment(VerticalAlignment);
}
