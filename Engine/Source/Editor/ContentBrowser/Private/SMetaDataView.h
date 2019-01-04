// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"

class ITableRow;
class STableViewBase;

struct FMetaDataLine;

/**
 * The widget to display metadata as a table of tag/value rows
 */
class SMetaDataView : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMetaDataView)	{}
	SLATE_END_ARGS()

	/**
	 * Construct this widget.  Called by the SNew() Slate macro.
	 *
	 * @param	InArgs				Declaration used by the SNew() macro to construct this widget
	 * @param	InMetaData			The metadata tags/values to display in the table view widget
	 */
	void Construct(const FArguments& InArgs, const TMap<FName, FString>& InMetadata);

private:
	TArray< TSharedPtr< FMetaDataLine > > MetaDataLines;

	TSharedRef< ITableRow > OnGenerateRow(const TSharedPtr< FMetaDataLine > Item, const TSharedRef< STableViewBase >& OwnerTable);
};
