// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class SWidget;

class SLATECORE_API FSlotBase
{
public:

	FSlotBase();

	FSlotBase( const TSharedRef<SWidget>& InWidget );

	virtual ~FSlotBase();

	FORCEINLINE_DEBUGGABLE void AttachWidgetParent(SWidget* InParent)
	{
		if (RawParentPtr != InParent)
		{
			ensureMsgf(RawParentPtr == nullptr, TEXT("Slots should not be reassigned to different parents."));

			RawParentPtr = InParent;
			AfterContentOrOwnerAssigned();
		}
	}

	FORCEINLINE_DEBUGGABLE void AttachWidget( const TSharedRef<SWidget>& InWidget )
	{
		// TODO: If we don't hold a reference here, ~SWidget() could called on the old widget before the assignment takes place
		// The behavior of TShareRef is going to change in the near future to avoid this issue and this should then be reverted.
		TSharedRef<SWidget> LocalWidget = Widget;
		DetatchParentFromContent();
		Widget = InWidget;
		AfterContentOrOwnerAssigned();
	}

	/**
	 * Access the widget in the current slot.
	 * There will always be a widget in the slot; sometimes it is
	 * the SNullWidget instance.
	 */
	FORCEINLINE_DEBUGGABLE const TSharedRef<SWidget>& GetWidget() const
	{
		return Widget;
	}

	/**
	 * Remove the widget from its current slot.
	 * The removed widget is returned so that operations could be performed on it.
	 * If the null widget was being stored, an invalid shared ptr is returned instead.
	 */
	const TSharedPtr<SWidget> DetachWidget();

protected:
	/** The parent and owner of the slot. */
	SWidget* RawParentPtr;

private:
	void DetatchParentFromContent();
	void AfterContentOrOwnerAssigned();

private:
	// non-copyable
	FSlotBase& operator=(const FSlotBase&);
	FSlotBase(const FSlotBase&);

private:
	/** The content widget of the slot. */
	TSharedRef<SWidget> Widget;
};


template<typename SlotType>
class TSlotBase : public FSlotBase
{
public:

	TSlotBase()
	: FSlotBase()
	{}

	TSlotBase( const TSharedRef<SWidget>& InWidget )
	: FSlotBase( InWidget )
	{}

	SlotType& operator[]( const TSharedRef<SWidget>& InChildWidget )
	{
		this->AttachWidget(InChildWidget);
		return (SlotType&)(*this);
	}

	SlotType& Expose( SlotType*& OutVarToInit )
	{
		OutVarToInit = (SlotType*)this;
		return (SlotType&)(*this);
	}
};
