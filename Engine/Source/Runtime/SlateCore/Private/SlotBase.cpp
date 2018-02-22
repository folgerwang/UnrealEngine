// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

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
#if SLATE_PARENT_POINTERS
		Widget->AssignParentWidget(TSharedPtr<SWidget>());
#endif

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

void FSlotBase::AfterContentOrOwnerChanges()
{
#if SLATE_DYNAMIC_PREPASS
	if (RawParentPtr)
	{
		RawParentPtr->InvalidatePrepass();
	}
#endif

#if SLATE_PARENT_POINTERS
	if (RawParentPtr)
	{
		if (Widget != SNullWidget::NullWidget)
		{
			Widget->AssignParentWidget(RawParentPtr->AsShared());
		}
	}
#endif
}

FSlotBase::~FSlotBase()
{
#if SLATE_PARENT_POINTERS
	if (Widget != SNullWidget::NullWidget)
	{
		Widget->AssignParentWidget(TSharedPtr<SWidget>());
	}
#endif
}
