// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "Blueprint/IUserObjectListEntry.h"
#include "Blueprint/UserWidget.h"

UUserObjectListEntry::UUserObjectListEntry(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
}

UObject* IUserObjectListEntry::GetListItem(UUserWidget& EntryWidget)
{
	if (EntryWidget.Implements<UUserObjectListEntry>())
	{
		return Execute_GetListItemObject(&EntryWidget);
	}
	return nullptr;
}

void IUserObjectListEntry::SetListItemObject(UUserWidget& ListEntryWidget, UObject* ListItemObject)
{
	if (IUserObjectListEntry* NativeImplementation = Cast<IUserObjectListEntry>(&ListEntryWidget))
	{
		NativeImplementation->SetListItemObjectInternal(ListItemObject);
		Execute_OnListItemObjectSet(&ListEntryWidget, ListItemObject);
	}
	else if (ListEntryWidget.Implements<UUserObjectListEntry>())
	{
		Execute_OnListItemObjectSet(&ListEntryWidget, ListItemObject);
	}
}