// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Interface.h"
#include "IUserListEntry.generated.h"

class UUserWidget;
class IObjectTableRow;

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class UMG_API UNativeUserListEntry : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class UMG_API INativeUserListEntry : public IInterface
{
	GENERATED_IINTERFACE_BODY()

public:
	/** Returns true if the item represented by this entry is currently selected. */
	UFUNCTION(BlueprintCallable, Category = UserListEntry)
	virtual bool IsListItemSelected() const = 0;

	/** Returns true if the item represented by this entry is currently expanded and showing its children. Tree view entries only. */
	UFUNCTION(BlueprintCallable, Category = UserListEntry)
	virtual bool IsListItemExpanded() const = 0;

	/** Returns the list view that contains this entry. */
	UFUNCTION(BlueprintCallable, Category = UserListEntry)
	virtual UListViewBase* GetOwningListView() const = 0;

	/** 
	 * Advanced native-only option for specific rows to preclude themselves from any kind of selection. 
	 * Intended primarily for category separators and the like.
	 * Note that this is only relevant when the row is in a list that allows selection in the first place.
	 */
	virtual bool IsListItemSelectable() const { return true; }
};

UINTERFACE()
class UMG_API UUserListEntry : public UNativeUserListEntry
{
	GENERATED_UINTERFACE_BODY()
};

class UMG_API IUserListEntry : public INativeUserListEntry
{
	GENERATED_IINTERFACE_BODY()

public:
	static void ReleaseEntry(UUserWidget& ListEntryWidget);
	static void UpdateItemSelection(UUserWidget& ListEntryWidget, bool bIsSelected);
	static void UpdateItemExpansion(UUserWidget& ListEntryWidget, bool bIsExpanded);
	
	virtual bool IsListItemSelected() const override final;
	virtual bool IsListItemExpanded() const override final;

	virtual UListViewBase* GetOwningListView() const override final;

protected:
	/** These follow the same pattern as the NativeOn[X] methods in UUserWidget - super calls are expected in order to route the event to BP. */
	virtual void NativeOnItemSelectionChanged(bool bIsSelected);
	virtual void NativeOnItemExpansionChanged(bool bIsExpanded);
	virtual void NativeOnEntryReleased();

	/** Called when the selection state of the item represented by this entry changes. */
	UFUNCTION(BlueprintImplementableEvent, Category = UserListEntry, meta = (DisplayName = "On Item Selection Changed"))
	void BP_OnItemSelectionChanged(bool bIsSelected);

	/** Called when the expansion state of the item represented by this entry changes. Tree view entries only. */
	UFUNCTION(BlueprintImplementableEvent, Category = UserListEntry, meta = (DisplayName = "On Item Expansion Changed"))
	void BP_OnItemExpansionChanged(bool bIsExpanded);

	/** Called when this entry is released from the owning table and no longer represents any list item */
	UFUNCTION(BlueprintImplementableEvent, Category = UserListEntry, meta = (DisplayName = "On Entry Released"))
	void BP_OnEntryReleased();
};