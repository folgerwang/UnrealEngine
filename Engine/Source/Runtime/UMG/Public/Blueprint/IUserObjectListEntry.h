// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IUserListEntry.h"
#include "IUserObjectListEntry.generated.h"

/**
 * Implement for list entry widgets that represent UObject items.
 * Required for a widget to be usable as an entry in UMG lists (ListView, TileView, and TreeView)
 */
UINTERFACE()
class UMG_API UUserObjectListEntry : public UUserListEntry
{
	GENERATED_UINTERFACE_BODY()
};

class UMG_API IUserObjectListEntry : public IUserListEntry
{
	GENERATED_IINTERFACE_BODY()

public:
	static UObject* GetListItem(UUserWidget& EntryWidget);

	template <typename ItemObjectT = UObject>
	ItemObjectT* GetListItem() const
	{
		static_assert(TIsDerivedFrom<ItemObjectT, UObject>::IsDerived, "Items represented by an ObjectListEntry are always expected to be UObjects.");
		return Cast<ItemObjectT>(Execute_GetListItemObject(Cast<UObject>(this)));
	}

protected:
	virtual void SetListItemObjectInternal(UObject* InObject) {}
	
	/** Returns the item object that this entry currently represents */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = ObjectListEntry, meta = (AllowPrivateAccess = true))
	UObject* GetListItemObject() const;

	/** Called when the item object represented by this entry is established */
	UFUNCTION(BlueprintImplementableEvent, Category = ObjectListEntry)
	void OnListItemObjectSet(UObject* ListItemObject);

private:
	template <typename> friend class SObjectTableRow;
	static void SetListItemObject(UUserWidget& ListEntryWidget, UObject* ListItemObject);
};