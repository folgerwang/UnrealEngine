// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ListView.h"
#include "Widgets/Views/STileView.h"
#include "TileView.generated.h"

/**
 * A ListView that presents the contents as a set of tiles all uniformly sized.
 */
UCLASS()
class UMG_API UTileView : public UListView
{
	GENERATED_BODY()

public:
	UTileView(const FObjectInitializer& ObjectInitializer);

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

	/** Sets the height of every tile entry */
	UFUNCTION(BlueprintCallable, Category = TileView)
	void SetEntryHeight(float NewHeight);

	/** Sets the width if every tile entry */
	UFUNCTION(BlueprintCallable, Category = TileView)
	void SetEntryWidth(float NewWidth);

protected:
	virtual TSharedRef<STableViewBase> RebuildListWidget() override;
	virtual FMargin GetDesiredEntryPadding(UObject* Item) const override;

	float GetTotalEntryHeight() const;
	float GetTotalEntryWidth() const;

	/** STileView construction helper - useful if using a custom STileView subclass */
	template <template<typename> class TileViewT = STileView>
	TSharedRef<TileViewT<UObject*>> ConstructTileView()
	{
		MyListView = MyTileView = ITypedUMGListView<UObject*>::ConstructTileView<TileViewT>(this, ListItems, TileAlignment, EntryHeight, EntryWidth, SelectionMode, bClearSelectionOnClick, bWrapHorizontalNavigation, ConsumeMouseWheel);
		return StaticCastSharedRef<TileViewT<UObject*>>(MyTileView.ToSharedRef());
	}

protected:
	/** The height of each tile */
	UPROPERTY(EditAnywhere, Category = ListEntries)
	float EntryHeight = 128.f;

	/** The width of each tile */
	UPROPERTY(EditAnywhere, Category = ListEntries)
	float EntryWidth = 128.f;

	/** The method by which to align the tile entries in the available space for the tile view */
	UPROPERTY(EditAnywhere, Category = ListEntries)
	EListItemAlignment TileAlignment;

	/** True to allow left/right navigation to wrap back to the tile on the opposite edge */
	UPROPERTY(EditAnywhere, Category = Navigation)
	bool bWrapHorizontalNavigation = false;

	TSharedPtr<STileView<UObject*>> MyTileView;
};
