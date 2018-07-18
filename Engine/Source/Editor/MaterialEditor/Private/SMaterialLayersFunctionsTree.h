// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Engine/EngineTypes.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STreeView.h"
#include "Materials/Material.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "IDetailTreeNode.h"
#include "IDetailPropertyRow.h"
#include "MaterialPropertyHelpers.h"

class IPropertyHandle;
class UMaterialEditorInstanceConstant;
class SMaterialLayersFunctionsInstanceTree;

class SMaterialLayersFunctionsInstanceTreeItem : public STableRow< TSharedPtr<FStackSortedData> >
{
public:

	SLATE_BEGIN_ARGS(SMaterialLayersFunctionsInstanceTreeItem)
		: _StackParameterData(nullptr),
		_MaterialEditorInstance(nullptr)
	{}

	/** The item content. */
	SLATE_ARGUMENT(TSharedPtr<FStackSortedData>, StackParameterData)
	SLATE_ARGUMENT(UMaterialEditorInstanceConstant*, MaterialEditorInstance)
	SLATE_ARGUMENT(SMaterialLayersFunctionsInstanceTree*, InTree)
	SLATE_END_ARGS()

	FMaterialTreeColumnSizeData ColumnSizeData;
	bool bIsBeingDragged;

private:
	bool bIsHoveredDragTarget;


	FString GetCurvePath(UDEditorScalarParameterValue* Parameter) const;
	const FSlateBrush* GetBorderImage() const;

public:

	void RefreshOnRowChange(const FAssetData& AssetData, SMaterialLayersFunctionsInstanceTree* InTree);
	bool GetFilterState(SMaterialLayersFunctionsInstanceTree* InTree, TSharedPtr<FStackSortedData> InStackData) const;
	void FilterClicked(const ECheckBoxState NewCheckedState, SMaterialLayersFunctionsInstanceTree* InTree, TSharedPtr<FStackSortedData> InStackData);
	ECheckBoxState GetFilterChecked(SMaterialLayersFunctionsInstanceTree* InTree, TSharedPtr<FStackSortedData> InStackData) const;
	FText GetLayerName(SMaterialLayersFunctionsInstanceTree* InTree, int32 Counter) const;
	void OnNameChanged(const FText& InText, ETextCommit::Type CommitInfo, SMaterialLayersFunctionsInstanceTree* InTree, int32 Counter);


	void OnLayerDragEnter(const FDragDropEvent& DragDropEvent)
	{
		if (StackParameterData->ParameterInfo.Index != 0)
		{
			bIsHoveredDragTarget = true;
		}
	}

	void OnLayerDragLeave(const FDragDropEvent& DragDropEvent)
	{
		bIsHoveredDragTarget = false;
	}

	void OnLayerDragDetected()
	{
		bIsBeingDragged = true;
	}

	FReply OnLayerDrop(const FDragDropEvent& DragDropEvent);
	/**
	* Construct the widget
	*
	* @param InArgs   A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView);

	/** The node info to build the tree view row from. */
	TSharedPtr<FStackSortedData> StackParameterData;

	SMaterialLayersFunctionsInstanceTree* Tree;

	UMaterialEditorInstanceConstant* MaterialEditorInstance;

	FString GetInstancePath(SMaterialLayersFunctionsInstanceTree* InTree) const;
};

class SMaterialLayersFunctionsInstanceWrapper : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialLayersFunctionsInstanceWrapper)
		: _InMaterialEditorInstance(nullptr)
	{}

	SLATE_ARGUMENT(UMaterialEditorInstanceConstant*, InMaterialEditorInstance)

	SLATE_END_ARGS()
	void Refresh();
	void Construct(const FArguments& InArgs);
	void SetEditorInstance(UMaterialEditorInstanceConstant* InMaterialEditorInstance);

	TAttribute<ECheckBoxState> IsParamChecked;
	class UDEditorParameterValue* LayerParameter;
	class UMaterialEditorInstanceConstant* MaterialEditorInstance;
	TSharedPtr<class SMaterialLayersFunctionsInstanceTree> NestedTree;
	FSimpleDelegate OnLayerPropertyChanged;
};

class SMaterialLayersFunctionsInstanceTree : public STreeView<TSharedPtr<FStackSortedData>>
{
	friend class SMaterialLayersFunctionsInstanceTreeItem;
public:
	SLATE_BEGIN_ARGS(SMaterialLayersFunctionsInstanceTree)
		: _InMaterialEditorInstance(nullptr)
	{}

	SLATE_ARGUMENT(UMaterialEditorInstanceConstant*, InMaterialEditorInstance)
	SLATE_ARGUMENT(SMaterialLayersFunctionsInstanceWrapper*, InWrapper)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	TSharedRef< ITableRow > OnGenerateRowMaterialLayersFunctionsTreeView(TSharedPtr<FStackSortedData> Item, const TSharedRef< STableViewBase >& OwnerTable);
	void OnGetChildrenMaterialLayersFunctionsTreeView(TSharedPtr<FStackSortedData> InParent, TArray< TSharedPtr<FStackSortedData> >& OutChildren);
	void OnExpansionChanged(TSharedPtr<FStackSortedData> Item, bool bIsExpanded);
	void SetParentsExpansionState();

	float OnGetLeftColumnWidth() const { return 1.0f - ColumnWidth; }
	float OnGetRightColumnWidth() const { return ColumnWidth; }
	void OnSetColumnWidth(float InWidth) { ColumnWidth = InWidth; }
	void ShowHiddenValues(bool& bShowHiddenParameters) { bShowHiddenParameters = true; }
	FName LayersFunctionsParameterName;
	class UDEditorParameterValue* FunctionParameter;
	struct FMaterialLayersFunctions* FunctionInstance;
	TSharedPtr<IPropertyHandle> FunctionInstanceHandle;
	void RefreshOnAssetChange(const struct FAssetData& InAssetData, int32 Index, EMaterialParameterAssociation MaterialType);
	void ResetAssetToDefault(TSharedPtr<IPropertyHandle> InHandle, TSharedPtr<FStackSortedData> InData);
	void AddLayer();
	void RemoveLayer(int32 Index);
	FReply ToggleLayerVisibility(int32 Index);
	TSharedPtr<class FAssetThumbnailPool> GetTreeThumbnailPool();

	/** Object that stores all of the possible parameters we can edit */
	UMaterialEditorInstanceConstant* MaterialEditorInstance;

	/** Builds the custom parameter groups category */
	void CreateGroupsWidget();

	bool IsLayerVisible(int32 Index) const;

	SMaterialLayersFunctionsInstanceWrapper* GetWrapper() { return Wrapper; }



	TSharedRef<SWidget> CreateThumbnailWidget(EMaterialParameterAssociation InAssociation, int32 InIndex, float InThumbnailSize);
	void UpdateThumbnailMaterial(TEnumAsByte<EMaterialParameterAssociation> InAssociation, int32 InIndex, bool bAlterBlendIndex = false);
	FReply OnThumbnailDoubleClick(const FGeometry& Geometry, const FPointerEvent& MouseEvent, EMaterialParameterAssociation InAssociation, int32 InIndex);
protected:

	void ShowSubParameters(TSharedPtr<FStackSortedData> ParentParameter);

private:
	TArray<TSharedPtr<FStackSortedData>> LayerProperties;

	TArray<FLayerParameterUnsortedData> NonLayerProperties;

	/** The actual width of the right column.  The left column is 1-ColumnWidth */
	float ColumnWidth;

	SMaterialLayersFunctionsInstanceWrapper* Wrapper;

	TSharedPtr<class IPropertyRowGenerator> Generator;

	bool bLayerIsolated;

};

class UMaterialEditorPreviewParameters;

class SMaterialLayersFunctionsMaterialWrapper : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialLayersFunctionsMaterialWrapper)
		: _InMaterialEditorInstance(nullptr)
	{}

	SLATE_ARGUMENT(UMaterialEditorPreviewParameters*, InMaterialEditorInstance)

	SLATE_END_ARGS()
	void Refresh();
	void Construct(const FArguments& InArgs);
	void SetEditorInstance(UMaterialEditorPreviewParameters* InMaterialEditorInstance);

	class UDEditorParameterValue* LayerParameter;
	UMaterialEditorPreviewParameters* MaterialEditorInstance;
	TSharedPtr<class SMaterialLayersFunctionsMaterialTree> NestedTree;
};


class SMaterialLayersFunctionsMaterialTree : public STreeView<TSharedPtr<FStackSortedData>>
{
	friend class SMaterialLayersFunctionsMaterialTreeItem;
public:
	SLATE_BEGIN_ARGS(SMaterialLayersFunctionsMaterialTree)
		: _InMaterialEditorInstance(nullptr)
	{}

	SLATE_ARGUMENT(UMaterialEditorPreviewParameters*, InMaterialEditorInstance)

	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);
	TSharedRef< ITableRow > OnGenerateRowMaterialLayersFunctionsTreeView(TSharedPtr<FStackSortedData> Item, const TSharedRef< STableViewBase >& OwnerTable);
	void OnGetChildrenMaterialLayersFunctionsTreeView(TSharedPtr<FStackSortedData> InParent, TArray< TSharedPtr<FStackSortedData> >& OutChildren);
	void OnExpansionChanged(TSharedPtr<FStackSortedData> Item, bool bIsExpanded);
	void SetParentsExpansionState();

	float OnGetLeftColumnWidth() const { return 1.0f - ColumnWidth; }
	float OnGetRightColumnWidth() const { return ColumnWidth; }
	void OnSetColumnWidth(float InWidth) { ColumnWidth = InWidth; }
	FName LayersFunctionsParameterName;
	class UDEditorParameterValue* FunctionParameter;
	struct FMaterialLayersFunctions* FunctionInstance;
	TSharedPtr<IPropertyHandle> FunctionInstanceHandle;
	TSharedPtr<class FAssetThumbnailPool> GetTreeThumbnailPool();

	/** Object that stores all of the possible parameters we can edit */
	UMaterialEditorPreviewParameters* MaterialEditorInstance;

	/** Builds the custom parameter groups category */
	void CreateGroupsWidget();

protected:

	void ShowSubParameters(TSharedPtr<FStackSortedData> ParentParameter);

private:
	TArray<TSharedPtr<FStackSortedData>> LayerProperties;

	TArray<FLayerParameterUnsortedData> NonLayerProperties;

	/** The actual width of the right column.  The left column is 1-ColumnWidth */
	float ColumnWidth;

	TSharedPtr<class IPropertyRowGenerator> Generator;

};