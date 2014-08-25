// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

/////////////////////////////////////////////////////
// UCanvasPanel

UCanvasPanel::UCanvasPanel(const FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{
	bIsVariable = false;

	SConstraintCanvas::FArguments Defaults;
	Visiblity = UWidget::ConvertRuntimeToSerializedVisiblity(Defaults._Visibility.Get());
}

void UCanvasPanel::ReleaseNativeWidget()
{
	Super::ReleaseNativeWidget();

	MyCanvas.Reset();
}

UClass* UCanvasPanel::GetSlotClass() const
{
	return UCanvasPanelSlot::StaticClass();
}

void UCanvasPanel::OnSlotAdded(UPanelSlot* Slot)
{
	// Add the child to the live canvas if it already exists
	if ( MyCanvas.IsValid() )
	{
		Cast<UCanvasPanelSlot>(Slot)->BuildSlot(MyCanvas.ToSharedRef());
	}
}

void UCanvasPanel::OnSlotRemoved(UPanelSlot* Slot)
{
	// Remove the widget from the live slot if it exists.
	if ( MyCanvas.IsValid() )
	{
		TSharedPtr<SWidget> Widget = Slot->Content->GetCachedWidget();
		if ( Widget.IsValid() )
		{
			MyCanvas->RemoveSlot(Widget.ToSharedRef());
		}
	}
}

TSharedRef<SWidget> UCanvasPanel::RebuildWidget()
{
	MyCanvas = SNew(SConstraintCanvas);

	for ( UPanelSlot* Slot : Slots )
	{
		if ( UCanvasPanelSlot* TypedSlot = Cast<UCanvasPanelSlot>(Slot) )
		{
			TypedSlot->Parent = this;
			TypedSlot->BuildSlot(MyCanvas.ToSharedRef());
		}
	}

	return BuildDesignTimeWidget( MyCanvas.ToSharedRef() );
}

TSharedPtr<SConstraintCanvas> UCanvasPanel::GetCanvasWidget() const
{
	return MyCanvas;
}

bool UCanvasPanel::GetGeometryForSlot(int32 SlotIndex, FGeometry& ArrangedGeometry) const
{
	UCanvasPanelSlot* Slot = Cast<UCanvasPanelSlot>(Slots[SlotIndex]);
	return GetGeometryForSlot(Slot, ArrangedGeometry);
}

bool UCanvasPanel::GetGeometryForSlot(UCanvasPanelSlot* Slot, FGeometry& ArrangedGeometry) const
{
	if ( Slot->Content == NULL )
	{
		return false;
	}

	TSharedPtr<SConstraintCanvas> Canvas = GetCanvasWidget();
	if ( Canvas.IsValid() )
	{
		FArrangedChildren ArrangedChildren(EVisibility::All);
		Canvas->ArrangeChildren(Canvas->GetCachedGeometry(), ArrangedChildren);

		for ( int32 ChildIndex = 0; ChildIndex < ArrangedChildren.Num(); ChildIndex++ )
		{
			if ( ArrangedChildren[ChildIndex].Widget == Slot->Content->TakeWidget() )
			{
				ArrangedGeometry = ArrangedChildren[ChildIndex].Geometry;
				return true;
			}
		}
	}

	return false;
}

#if WITH_EDITOR

const FSlateBrush* UCanvasPanel::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.Canvas");
}

#endif