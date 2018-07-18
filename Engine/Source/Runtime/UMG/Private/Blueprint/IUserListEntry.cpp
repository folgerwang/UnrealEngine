// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Blueprint/IUserListEntry.h"
#include "Slate/SObjectTableRow.h"
#include "Blueprint/UserWidget.h"

TMap<TWeakObjectPtr<const UUserWidget>, TWeakPtr<const IObjectTableRow>> IObjectTableRow::ObjectRowsByUserWidget;

UNativeUserListEntry::UNativeUserListEntry(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
}

UUserListEntry::UUserListEntry(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
}

void IUserListEntry::ReleaseEntry(UUserWidget& ListEntryWidget)
{
	if (IUserListEntry* NativeImplementation = Cast<IUserListEntry>(&ListEntryWidget))
	{
		NativeImplementation->NativeOnEntryReleased();
	}
	else if (ListEntryWidget.Implements<UUserListEntry>())
	{
		Execute_BP_OnEntryReleased(&ListEntryWidget);
	}
}

void IUserListEntry::UpdateItemSelection(UUserWidget& ListEntryWidget, bool bIsSelected)
{
	if (IUserListEntry* NativeImplementation = Cast<IUserListEntry>(&ListEntryWidget))
	{
		NativeImplementation->NativeOnItemSelectionChanged(bIsSelected);
	}
	else if (ListEntryWidget.Implements<UUserListEntry>())
	{
		Execute_BP_OnItemSelectionChanged(&ListEntryWidget, bIsSelected);
	}
}

void IUserListEntry::UpdateItemExpansion(UUserWidget& ListEntryWidget, bool bIsExpanded)
{
	if (IUserListEntry* NativeImplementation = Cast<IUserListEntry>(&ListEntryWidget))
	{
		NativeImplementation->NativeOnItemExpansionChanged(bIsExpanded);
	}
	else if (ListEntryWidget.Implements<UUserListEntry>())
	{
		Execute_BP_OnItemExpansionChanged(&ListEntryWidget, bIsExpanded);
	}
}

bool IUserListEntry::IsListItemSelected() const
{
	TSharedPtr<const IObjectTableRow> SlateRow = IObjectTableRow::ObjectRowFromUserWidget(CastChecked<const UUserWidget>(this, ECastCheckedType::NullAllowed));
	if (SlateRow.IsValid())
	{
		return SlateRow->IsItemSelected();
	}
	return false;
}

bool IUserListEntry::IsListItemExpanded() const
{
	TSharedPtr<const IObjectTableRow> SlateRow = IObjectTableRow::ObjectRowFromUserWidget(CastChecked<const UUserWidget>(this, ECastCheckedType::NullAllowed));
	if (SlateRow.IsValid())
	{
		return SlateRow->IsItemExpanded();
	}
	return false;
}

void IUserListEntry::NativeOnEntryReleased()
{
	Execute_BP_OnEntryReleased(Cast<UObject>(this));
}

void IUserListEntry::NativeOnItemSelectionChanged(bool bIsSelected)
{
	Execute_BP_OnItemSelectionChanged(Cast<UObject>(this), bIsSelected);
}

void IUserListEntry::NativeOnItemExpansionChanged(bool bIsExpanded)
{
	Execute_BP_OnItemExpansionChanged(Cast<UObject>(this), bIsExpanded);
}