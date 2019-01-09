// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Framework/Commands/UIAction.h"

class SWidget;
class FDetailWidgetRow;

class DATATABLEEDITOR_API FDataTableRowUtils
{
public:
	static TSharedRef<SWidget> MakeRowActionsMenu(FExecuteAction SearchForReferencesAction);
	static void AddSearchForReferencesContextMenu(FDetailWidgetRow& RowNameDetailWidget, FExecuteAction SearchForReferencesAction);

private:
	static const FText SearchForReferencesActionName;
	static const FText SearchForReferencesActionTooltip;
};

