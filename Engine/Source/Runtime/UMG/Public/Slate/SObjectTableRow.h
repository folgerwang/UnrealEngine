// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Slate/SObjectWidget.h"
#include "Blueprint/IUserObjectListEntry.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"

class IObjectTableRow : public ITableRow
{
public:
	virtual UUserWidget* GetUserWidget() const = 0;

	UMG_API static TSharedPtr<const IObjectTableRow> ObjectRowFromUserWidget(const UUserWidget* RowUserWidget)
	{
		TWeakPtr<const IObjectTableRow>* ObjectRow = ObjectRowsByUserWidget.Find(RowUserWidget);
		if (ObjectRow && ObjectRow->IsValid())
		{
			return ObjectRow->Pin();
		}
		return nullptr;
	}

protected:
	// Intentionally being a bit nontraditional here - we track associations between UserWidget rows and their underlying IObjectTableRow.
	// This allows us to mirror the ITableRow API very easily on IUserListEntry without requiring rote setting/getting of row states on every UMG subclass.
	UMG_API static TMap<TWeakObjectPtr<const UUserWidget>, TWeakPtr<const IObjectTableRow>> ObjectRowsByUserWidget;
};

DECLARE_DELEGATE_OneParam(FOnRowHovered, UUserWidget&);

/** 
 * It's an SObjectWidget! It's an ITableRow! It does it all!
 *
 * By using UUserWidget::TakeDerivedWidget<T>(), this class allows UMG to fully leverage the robust Slate list view widgets.
 * The major gain from this is item virtualization, which is an even bigger deal when unnecessary widgets come with a boatload of additional UObject allocations.
 * 
 * The owning UUserWidget is expected to implement the IUserListItem UInterface, which allows the row widget to respond to various list-related events.
 *
 * Note: Much of the implementation here matches STableRow<T> exactly, so refer there if looking for additional information.
 */
template <typename ItemType>
class SObjectTableRow : public SObjectWidget, public IObjectTableRow
{
public:
	SLATE_BEGIN_ARGS(SObjectTableRow<ItemType>) {}
		SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_EVENT(FOnRowHovered, OnHovered)
		SLATE_EVENT(FOnRowHovered, OnUnhovered)
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, UUserWidget& InWidgetObject)
	{
		TSharedPtr<SWidget> ContentWidget;

		if (ensureMsgf(InWidgetObject.Implements<UUserListEntry>(), TEXT("Any UserWidget generated as a table row must implement the IUserListEntry interface")))
		{
			ObjectRowsByUserWidget.Add(&InWidgetObject, SharedThis(this));

			OwnerTablePtr = StaticCastSharedRef<SListView<ItemType>>(InOwnerTableView);

			OnHovered = InArgs._OnHovered;
			OnUnhovered = InArgs._OnUnhovered;
			ContentWidget = InArgs._Content.Widget;
		}
		else
		{
			ContentWidget = SNew(STextBlock)
				.Text(NSLOCTEXT("SObjectTableRow", "InvalidWidgetClass", "Any UserWidget generated as a table row must implement the IUserListEntry interface"));
		}

		SObjectWidget::Construct(
			SObjectWidget::FArguments()
			.Content()
			[
				ContentWidget.ToSharedRef()
			], &InWidgetObject);
	}

	virtual ~SObjectTableRow()
	{
		// Remove the association between this widget and its user widget
		ObjectRowsByUserWidget.Remove(WidgetObject);
	}

	virtual UUserWidget* GetUserWidget() const
	{
		return WidgetObject;
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		SObjectWidget::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

		// List views were built assuming the use of attributes on rows to check on selection status, so there is no
		// clean way to inform individual rows of changes to the selection state of their current items.
		// Since event-based selection changes are only really needed in a game scenario, we (crudely) monitor it here to generate events.
		// If desired, per-item selection events could be added as a longer-term todo
		TSharedPtr<ITypedTableView<ItemType>> OwnerTable = OwnerTablePtr.Pin();
		if (OwnerTable.IsValid())
		{
			const ItemType& MyItem = *OwnerTable->Private_ItemFromWidget(this);
			if (bIsAppearingSelected != OwnerTable->Private_IsItemSelected(MyItem))
			{
				bIsAppearingSelected = !bIsAppearingSelected;
				OnItemSelectionChanged(bIsAppearingSelected);
			}
		}
	}

	virtual void NotifyItemExpansionChanged(bool bIsExpanded)
	{
		if (WidgetObject)
		{
			IUserListEntry::UpdateItemExpansion(*WidgetObject, bIsExpanded);
		}
	}

	// ITableRow interface
	virtual void InitializeRow() override final
	{
		// ObjectRows can be generated in the widget designer with dummy data, which we want to ignore
		if (WidgetObject && !WidgetObject->IsDesignTime())
		{
			InitializeObjectRow();
		}
	}

	virtual void ResetRow() override final
	{
		if (WidgetObject && !WidgetObject->IsDesignTime())
		{
			ResetObjectRow();
		}
	}

	virtual TSharedRef<SWidget> AsWidget() override { return SharedThis(this); }
	virtual void SetIndexInList(int32 InIndexInList) override { IndexInList = InIndexInList; }
	virtual TSharedPtr<SWidget> GetContent() override { return ChildSlot.GetChildAt(0); }
	virtual int32 GetIndentLevel() const override { return OwnerTablePtr.Pin()->Private_GetNestingDepth(IndexInList); }
	virtual int32 DoesItemHaveChildren() const override { return OwnerTablePtr.Pin()->Private_DoesItemHaveChildren(IndexInList); }
	virtual void Private_OnExpanderArrowShiftClicked() override { /* Intentionally blank - far too specific to be a valid game UI interaction */ }
	virtual ESelectionMode::Type GetSelectionMode() const override { return OwnerTablePtr.Pin()->Private_GetSelectionMode(); }
	virtual FVector2D GetRowSizeForColumn(const FName& InColumnName) const override { return FVector2D::ZeroVector; }

	virtual bool IsItemExpanded() const override
	{
		TSharedPtr<ITypedTableView<ItemType>> OwnerTable = OwnerTablePtr.Pin();
		const ItemType& MyItem = *OwnerTable->Private_ItemFromWidget(this);
		return OwnerTable->Private_IsItemExpanded(MyItem);
	}

	virtual void ToggleExpansion() override
	{
		TSharedPtr<ITypedTableView<ItemType>> OwnerTable = OwnerTablePtr.Pin();
		if (OwnerTable->Private_DoesItemHaveChildren(IndexInList))
		{
			const ItemType& MyItem = *OwnerTable->Private_ItemFromWidget(this);
			OwnerTable->Private_SetItemExpansion(MyItem, !OwnerTable->Private_IsItemExpanded(MyItem));
		}
	}

	virtual bool IsItemSelected() const override
	{
		TSharedPtr<ITypedTableView<ItemType>> OwnerTable = OwnerTablePtr.Pin();
		const ItemType& MyItem = *OwnerTable->Private_ItemFromWidget(this);
		return OwnerTable->Private_IsItemSelected(MyItem);
	}

	virtual TBitArray<> GetWiresNeededByDepth() const override
	{
		return OwnerTablePtr.Pin()->Private_GetWiresNeededByDepth(IndexInList);
	}

	virtual bool IsLastChild() const override
	{
		return OwnerTablePtr.Pin()->Private_IsLastChild(IndexInList);
	}

	// ~ITableRow interface

	// SWidget interface
	virtual bool SupportsKeyboardFocus() const override { return true; }

	virtual void OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		SObjectWidget::OnMouseEnter(MyGeometry, MouseEvent);
		if (WidgetObject && OnHovered.IsBound())
		{
			OnHovered.ExecuteIfBound(*WidgetObject);
		}
	}

	virtual void OnMouseLeave(const FPointerEvent& MouseEvent) override
	{
		SObjectWidget::OnMouseLeave(MouseEvent);
		if (WidgetObject && OnUnhovered.IsBound())
		{
			OnUnhovered.ExecuteIfBound(*WidgetObject);
		}
	}

	virtual FReply OnMouseButtonDoubleClick(const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent) override
	{
		if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			TSharedRef<ITypedTableView<ItemType>> OwnerTable = OwnerTablePtr.Pin().ToSharedRef();
			OwnerTable->Private_OnItemDoubleClicked(*OwnerTable->Private_ItemFromWidget(this));
			
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnTouchStarted(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override
	{
		//TODO: FReply Reply = SObjectWidget::OnTouchStarted(MyGeometry, InTouchEvent);
		bProcessingSelectionTouch = true;

		return FReply::Handled()
			.DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	}

	virtual FReply OnTouchEnded(const FGeometry& MyGeometry, const FPointerEvent& InTouchEvent) override
	{
		FReply Reply = SObjectWidget::OnTouchEnded(MyGeometry, InTouchEvent);

		if (bProcessingSelectionTouch)
		{
			bProcessingSelectionTouch = false;
			TSharedRef<ITypedTableView<ItemType>> OwnerTable = OwnerTablePtr.Pin().ToSharedRef();
			if (const ItemType* MyItem = OwnerTable->Private_ItemFromWidget(this))
			{
				ESelectionMode::Type SelectionMode = GetSelectionMode();
				if (SelectionMode != ESelectionMode::None)
				{
					const bool bIsSelected = OwnerTable->Private_IsItemSelected(*MyItem);
					if (!bIsSelected)
					{
						if (SelectionMode != ESelectionMode::Multi)
						{
							OwnerTable->Private_ClearSelection();
						}
						OwnerTable->Private_SetItemSelection(*MyItem, true, true);
						OwnerTable->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);

						Reply = FReply::Handled();
					}
					else if (SelectionMode == ESelectionMode::SingleToggle || SelectionMode == ESelectionMode::Multi)
					{
						OwnerTable->Private_SetItemSelection(*MyItem, true, true);
						OwnerTable->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);

						Reply = FReply::Handled();
					}
				}

				if (OwnerTable->Private_OnItemClicked(*MyItem))
				{
					Reply = FReply::Handled();
				}
			}
		}

		return Reply;
	}

	virtual FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bProcessingSelectionTouch)
		{
			bProcessingSelectionTouch = false;
			return FReply::Handled().CaptureMouse(OwnerTablePtr.Pin()->AsWidget());
		}
		//@todo DanH TableRow: does this potentially trigger twice? If not, why does an unhandled drag detection result in not calling mouse up?
		else if (HasMouseCapture() && bChangedSelectionOnMouseDown)
		{
			TSharedRef<ITypedTableView<ItemType>> OwnerTable = OwnerTablePtr.Pin().ToSharedRef();
			OwnerTable->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
		}

		return SObjectWidget::OnDragDetected(MyGeometry, MouseEvent);
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		bChangedSelectionOnMouseDown = false;

		FReply Reply = SObjectWidget::OnMouseButtonDown(MyGeometry, MouseEvent);
		if (!Reply.IsEventHandled())
		{
			TSharedRef<ITypedTableView<ItemType>> OwnerTable = OwnerTablePtr.Pin().ToSharedRef();

			const ESelectionMode::Type SelectionMode = GetSelectionMode();
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && SelectionMode != ESelectionMode::None)
			{
				if (IsItemSelectable())
				{
					// New selections are handled on mouse down, deselection is handled on mouse up
					const ItemType& MyItem = *OwnerTable->Private_ItemFromWidget(this);
					if (!OwnerTable->Private_IsItemSelected(MyItem))
					{
						if (SelectionMode != ESelectionMode::Multi)
						{
							OwnerTable->Private_ClearSelection();
						}
						OwnerTable->Private_SetItemSelection(MyItem, true, true);
						bChangedSelectionOnMouseDown = true;
					}
				}

				Reply = FReply::Handled()
					.DetectDrag(SharedThis(this), EKeys::LeftMouseButton)
					.SetUserFocus(OwnerTable->AsWidget(), EFocusCause::Mouse)
					.CaptureMouse(SharedThis(this));
			}
		}
		
		return Reply;
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		FReply Reply = SObjectWidget::OnMouseButtonUp(MyGeometry, MouseEvent);

		TSharedRef<ITypedTableView<ItemType>> OwnerTable = OwnerTablePtr.Pin().ToSharedRef();
		const ItemType* MyItem = OwnerTable->Private_ItemFromWidget(this);
		if (!Reply.IsEventHandled() && MyItem)
		{
			const ESelectionMode::Type SelectionMode = GetSelectionMode();
			if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton && HasMouseCapture())
			{
				if (IsItemSelectable() && MyGeometry.IsUnderLocation(MouseEvent.GetScreenSpacePosition()))
				{
					if (SelectionMode == ESelectionMode::SingleToggle)
					{
						OwnerTable->Private_ClearSelection();
						OwnerTable->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);

						Reply = FReply::Handled();
					}
					else if (SelectionMode == ESelectionMode::Multi &&
						OwnerTable->Private_GetNumSelectedItems() > 1 &&
						OwnerTable->Private_IsItemSelected(*MyItem))
					{
						// Releasing mouse over one of the multiple selected items - leave this one as the sole selected item
						OwnerTable->Private_ClearSelection();
						OwnerTable->Private_SetItemSelection(*MyItem, true, true);
						OwnerTable->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);

						Reply = FReply::Handled();
					}
				}

				if (OwnerTable->Private_OnItemClicked(*MyItem))
				{
					Reply = FReply::Handled();
				}

				if (bChangedSelectionOnMouseDown)
				{
					Reply = FReply::Handled();
					OwnerTable->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
				}

				Reply = Reply.ReleaseMouseCapture();
			}
			else if (SelectionMode != ESelectionMode::None && MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
			{
				// Ignore the right click release if it was being used for scrolling
				TSharedRef<STableViewBase> OwnerTableViewBase = StaticCastSharedRef<SListView<ItemType>>(OwnerTable);
				if (!OwnerTableViewBase->IsRightClickScrolling())
				{
					if (IsItemSelectable() && !OwnerTable->Private_IsItemSelected(*MyItem))
					{
						// If this item isn't selected, it becomes the sole selected item. Otherwise we leave selection untouched.
						OwnerTable->Private_ClearSelection();
						OwnerTable->Private_SetItemSelection(*MyItem, true, true);
						OwnerTable->Private_SignalSelectionChanged(ESelectInfo::OnMouseClick);
					}

					OwnerTable->Private_OnItemRightClicked(*MyItem, MouseEvent);

					Reply = FReply::Handled();
				}
			}
		}

		return Reply;
	}
	// ~SWidget interface

protected:
	virtual void InitializeObjectRow()
	{
		TSharedPtr<ITypedTableView<ItemType>> OwnerTable = OwnerTablePtr.Pin();
		const ItemType& MyItem = *OwnerTable->Private_ItemFromWidget(this);
		
		InitObjectRowInternal(*WidgetObject, MyItem);

		// Unselectable items should never be selected
		if (!ensure(!OwnerTable->Private_IsItemSelected(MyItem) || IsItemSelectable()))
		{
			OwnerTable->Private_SetItemSelection(MyItem, false, false);
		}
	}

	virtual void ResetObjectRow()
	{
		bIsAppearingSelected = false;
		if (WidgetObject)
		{
			IUserListEntry::ReleaseEntry(*WidgetObject);
		}
	}

	virtual void OnItemSelectionChanged(bool bIsItemSelected)
	{
		if (WidgetObject)
		{
			IUserListEntry::UpdateItemSelection(*WidgetObject, bIsItemSelected);
		}
	}

	bool IsItemSelectable() const 
	{
		IUserListEntry* NativeListEntryImpl = Cast<IUserListEntry>(WidgetObject);
		return NativeListEntryImpl ? NativeListEntryImpl->IsListItemSelectable() : true;
	}

	FOnRowHovered OnHovered;
	FOnRowHovered OnUnhovered;

	TWeakPtr<ITypedTableView<ItemType>> OwnerTablePtr;

private:
	void InitObjectRowInternal(UUserWidget& ListEntryWidget, ItemType ListItemObject) {}
	
	int32 IndexInList = INDEX_NONE;
	bool bChangedSelectionOnMouseDown = false;
	bool bIsAppearingSelected = false;

	bool bProcessingSelectionTouch = false;
};

template <>
inline void SObjectTableRow<UObject*>::InitObjectRowInternal(UUserWidget& ListEntryWidget, UObject* ListItemObject)
{
	if (ListEntryWidget.Implements<UUserObjectListEntry>())
	{
		IUserObjectListEntry::SetListItemObject(*WidgetObject, ListItemObject);
	}
}