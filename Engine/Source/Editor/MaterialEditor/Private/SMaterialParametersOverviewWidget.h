// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Materials/Material.h"
#include "IDetailTreeNode.h"
#include "IDetailPropertyRow.h"
#include "MaterialPropertyHelpers.h"

class IPropertyHandle;
class SMaterialParametersOverviewTree;
class UMaterialEditorPreviewParameters;

// ********* SMaterialParametersOverviewTreeItem *******
class SMaterialParametersOverviewTreeItem : public STableRow< TSharedPtr<FSortedParamData> >
{
public:

	SLATE_BEGIN_ARGS(SMaterialParametersOverviewTreeItem)
		: _StackParameterData(nullptr),
		_MaterialEditorInstance(nullptr)
	{}

	/** The item content. */
	SLATE_ARGUMENT(TSharedPtr<FSortedParamData>, StackParameterData)
	SLATE_ARGUMENT(UMaterialEditorPreviewParameters*, MaterialEditorInstance)
	SLATE_ARGUMENT(TSharedPtr<SMaterialParametersOverviewTree>, InTree)
	SLATE_END_ARGS()

	void RefreshOnRowChange(const FAssetData& AssetData, TSharedPtr<SMaterialParametersOverviewTree> InTree);

	/**
	* Construct the widget
	*
	* @param InArgs   A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

private:
	FString GetCurvePath(class UDEditorScalarParameterValue* Parameter) const;

	const FSlateBrush* GetBorderImage() const;

private:

	/** The node info to build the tree view row from. */
	TSharedPtr<FSortedParamData> StackParameterData;

	/** The tree that contains this item */
	TWeakPtr<SMaterialParametersOverviewTree> Tree;

	/** The set of material parameters this is associated with */
	UMaterialEditorPreviewParameters* MaterialEditorInstance;

	FMaterialTreeColumnSizeData ColumnSizeData;
};

// ********* SMaterialParametersOverviewPanel *******
class SMaterialParametersOverviewPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialParametersOverviewPanel)
		: _InMaterialEditorInstance(nullptr)
	{}

	SLATE_ARGUMENT(UMaterialEditorPreviewParameters*, InMaterialEditorInstance)
	SLATE_END_ARGS()
	void Refresh();
	void Construct(const FArguments& InArgs);
	void UpdateEditorInstance(UMaterialEditorPreviewParameters* InMaterialEditorInstance);
	TSharedPtr<class IPropertyRowGenerator> GetGenerator();

private:
	const FSlateBrush* GetBackgroundImage() const;

	int32 GetPanelIndex() const;

private:
	/** The set of material parameters this is associated with */
	class UMaterialEditorPreviewParameters* MaterialEditorInstance;

	/** The tree contained in this item */
	TSharedPtr<class SMaterialParametersOverviewTree> NestedTree;
};

// ********* SMaterialParametersOverviewTree *******
class SMaterialParametersOverviewTree : public STreeView<TSharedPtr<FSortedParamData>>
{
	friend class SMaterialParametersOverviewTreeItem;

public:

	SLATE_BEGIN_ARGS(SMaterialParametersOverviewTree)
		: _InMaterialEditorInstance(nullptr)
		, _InOwner(nullptr)
	{}

	SLATE_ARGUMENT(UMaterialEditorPreviewParameters*, InMaterialEditorInstance)
	SLATE_ARGUMENT(TSharedPtr<SMaterialParametersOverviewPanel>, InOwner)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	TSharedRef< ITableRow > OnGenerateRowMaterialLayersFunctionsTreeView(TSharedPtr<FSortedParamData> Item, const TSharedRef< STableViewBase >& OwnerTable);
	void OnGetChildrenMaterialLayersFunctionsTreeView(TSharedPtr<FSortedParamData> InParent, TArray< TSharedPtr<FSortedParamData> >& OutChildren);
	void OnExpansionChanged(TSharedPtr<FSortedParamData> Item, bool bIsExpanded);
	void SetParentsExpansionState();

	float OnGetLeftColumnWidth() const { return 1.0f - ColumnWidth; }
	float OnGetRightColumnWidth() const { return ColumnWidth; }
	void OnSetColumnWidth(float InWidth) { ColumnWidth = InWidth; }
	TSharedPtr<class FAssetThumbnailPool> GetTreeThumbnailPool();

	/** Object that stores all of the possible parameters we can edit */
	UMaterialEditorPreviewParameters* MaterialEditorInstance;

	/** Builds the custom parameter groups category */
	void CreateGroupsWidget();

	TWeakPtr<SMaterialParametersOverviewPanel> GetOwner() { return Owner; }

	TSharedPtr<class IPropertyRowGenerator> GetGenerator() { return Generator; }

	bool HasAnyParameters() const { return bHasAnyParameters; }

protected:

	void ShowSubParameters();

private:

	TArray<TSharedPtr<FSortedParamData>> SortedParameters;

	TArray<FUnsortedParamData> UnsortedParameters;

	/** The actual width of the right column.  The left column is 1-ColumnWidth */
	float ColumnWidth;

	TWeakPtr<SMaterialParametersOverviewPanel> Owner;

	TSharedPtr<class IPropertyRowGenerator> Generator;

	bool bHasAnyParameters;

};
