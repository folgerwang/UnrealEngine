// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SlotBase.h"
#include "Widgets/SWidget.h"
#include "Widgets/SNullWidget.h"

FSlotBase::FSlotBase()
	: RawParentPtr(nullptr)
	, Widget( SNullWidget::NullWidget )
{

}

FSlotBase::FSlotBase( const TSharedRef<SWidget>& InWidget )
	: RawParentPtr(nullptr)
	, Widget( InWidget )
{
	
}

const TSharedPtr<SWidget> FSlotBase::DetachWidget()
{
	if (Widget != SNullWidget::NullWidget)
	{
		Widget->ConditionallyDetatchParentWidget(RawParentPtr);

		// Invalidate Prepass?

		const TSharedRef<SWidget> MyExWidget = Widget;
		Widget = SNullWidget::NullWidget;	
		return MyExWidget;
	}
	else
	{
		// Nothing to detach!
		return TSharedPtr<SWidget>();
	}
}

void FSlotBase::DetatchParentFromContent()
{
	if (Widget != SNullWidget::NullWidget)
	{
		Widget->ConditionallyDetatchParentWidget(RawParentPtr);
	}
}

void FSlotBase::AfterContentOrOwnerAssigned()
{
	if (GSlateLayoutCaching && RawParentPtr)
	{
		RawParentPtr->InvalidatePrepass();
	}

	if (RawParentPtr)
	{
		if (Widget != SNullWidget::NullWidget)
		{
			// TODO NDarnell I want to enable this, but too many places in the codebase
			// have made assumptions about being able to freely reparent widgets, while they're
			// still connected to an existing hierarchy.
			//ensure(!Widget->IsParentValid());
			Widget->AssignParentWidget(RawParentPtr->AsShared());
		}
	}
}

FSlotBase::~FSlotBase()
{
	DetatchParentFromContent();
}
