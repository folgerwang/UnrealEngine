// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Widgets/Views/SListView.h"

class IDetailLayoutBuilder;
class ITableRow;
class STableViewBase;
class UEditorSkeletonNotifyObj;

class FSkeletonNotifyDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

	/** Delegate to handle creating rows for the animations slate list */
	TSharedRef< ITableRow > MakeAnimationRow( TSharedPtr<FString> Item, const TSharedRef< STableViewBase >& OwnerTable );

private:
	/** Look for all aniamtions that reference our notify */
	FReply CollectSequencesUsingNotify();

private:
	/** The object we are customizing */
	TWeakObjectPtr<UEditorSkeletonNotifyObj> NotifyObject;

	/** The names of any animations that reference the notify we are displaying */
	TArray< TSharedPtr<FString> > AnimationNames;

	/** The list view widget */
	TSharedPtr<SListView<TSharedPtr<FString>>> ListView;
};
