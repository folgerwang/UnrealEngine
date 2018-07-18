// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "Layout/Visibility.h"
#include "Materials/MaterialLayersFunctions.h"
#include "Input/Reply.h"
#include "Widgets/Layout/SSplitter.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "MaterialPropertyHelpers.generated.h"


struct FAssetData;
class IDetailGroup;
class IDetailLayoutBuilder;
class IPropertyHandle;
class UMaterialEditorInstanceConstant;
enum class ECheckBoxState : uint8;
class UMaterialInterface;
class SMaterialLayersFunctionsInstanceTreeItem;

DECLARE_DELEGATE_OneParam(FGetShowHiddenParameters, bool&);

enum EStackDataType
{
	Stack,
	Asset,
	Group,
	Property,
	PropertyChild,
};

USTRUCT()
struct MATERIALEDITOR_API FStackSortedData
{
	GENERATED_USTRUCT_BODY()

public:
	EStackDataType StackDataType;

	class UDEditorParameterValue* Parameter;

	FName PropertyName;

	FEditorParameterGroup Group;

	FMaterialParameterInfo ParameterInfo;

	TSharedPtr<class IDetailTreeNode> ParameterNode;

	TSharedPtr<class IPropertyHandle> ParameterHandle;

	TArray<TSharedPtr<struct FStackSortedData>> Children;

	FString NodeKey;
};

struct FLayerParameterUnsortedData
{
	class UDEditorParameterValue* Parameter;
	FEditorParameterGroup ParameterGroup;
	TSharedPtr<class IDetailTreeNode> ParameterNode;
	FName UnsortedName;
	TSharedPtr<class IPropertyHandle> ParameterHandle;
};

struct FMaterialTreeColumnSizeData
{
	TAttribute<float> LeftColumnWidth;
	TAttribute<float> RightColumnWidth;
	SSplitter::FOnSlotResized OnWidthChanged;

	void SetColumnWidth(float InWidth) { OnWidthChanged.ExecuteIfBound(InWidth); }
};

class SLayerHandle : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SLayerHandle)
	{}
	SLATE_DEFAULT_SLOT(FArguments, Content)
		SLATE_ARGUMENT(TSharedPtr<SMaterialLayersFunctionsInstanceTreeItem>, OwningStack)
		SLATE_END_ARGS()

		void Construct(const FArguments& InArgs);

	FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		return FReply::Handled().DetectDrag(SharedThis(this), EKeys::LeftMouseButton);
	};


	FReply OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
	TSharedPtr<class FLayerDragDropOp> CreateDragDropOperation(TSharedPtr<SMaterialLayersFunctionsInstanceTreeItem> InOwningStack);

private:
	TWeakPtr<SMaterialLayersFunctionsInstanceTreeItem> OwningStack;
};


class FLayerDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FLayerDragDropOp, FDecoratedDragDropOp)

	FLayerDragDropOp(TSharedPtr<SMaterialLayersFunctionsInstanceTreeItem> InOwningStack)
	{
		OwningStack = InOwningStack;
		DecoratorWidget = SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("Graph.ConnectorFeedback.Border"))
			.Content()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NSLOCTEXT("LayerDragDrop", "PlaceLayerHere", "Place Layer and Blend Here"))
				]
			];

		Construct();
	};

	TSharedPtr<SWidget> DecoratorWidget;

	virtual TSharedPtr<SWidget> GetDefaultDecorator() const override
	{
		return DecoratorWidget;
	}

	TWeakPtr<class SMaterialLayersFunctionsInstanceTreeItem> OwningStack;
};

/*-----------------------------------------------------------------------------
   FMaterialInstanceBaseParameterDetails
-----------------------------------------------------------------------------*/

class MATERIALEDITOR_API FMaterialPropertyHelpers
{
public:
	/** Returns true if the parameter is being overridden */
	static bool IsOverriddenExpression(class UDEditorParameterValue* Parameter);
	static ECheckBoxState IsOverriddenExpressionCheckbox(UDEditorParameterValue* Parameter);

	/** Gets the expression description of this parameter from the base material */
	static	FText GetParameterExpressionDescription(class UDEditorParameterValue* Parameter, UObject* MaterialEditorInstance);
	
	/**
	 * Called when a parameter is overridden;
	 */
	static void OnOverrideParameter(bool NewValue, class UDEditorParameterValue* Parameter, UMaterialEditorInstanceConstant* MaterialEditorInstance);

	static EVisibility ShouldShowExpression(UDEditorParameterValue* Parameter, UMaterialEditorInstanceConstant* MaterialEditorInstance, FGetShowHiddenParameters ShowHiddenDelegate);

	/** Generic material property reset to default implementation.  Resets Parameter to default */
	static void ResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle, class UDEditorParameterValue* Parameter, UMaterialEditorInstanceConstant* MaterialEditorInstance);
	static bool ShouldShowResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle, class UDEditorParameterValue* Parameter, UMaterialEditorInstanceConstant* MaterialEditorInstance);
	
	/** Specific resets for layer and blend asses */
	static void ResetLayerAssetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle, class UDEditorParameterValue* InParameter, TEnumAsByte<EMaterialParameterAssociation> InAssociation, int32 Index, UMaterialEditorInstanceConstant* MaterialEditorInstance);
	/** If reset to default button should show for a layer or blend asset*/
	static bool ShouldLayerAssetShowResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle, TSharedPtr<struct FStackSortedData> InParameterData, UMaterialInterface* InMaterial);

	static void OnMaterialLayerAssetChanged(const struct FAssetData& InAssetData, int32 Index, EMaterialParameterAssociation MaterialType, TSharedPtr<class IPropertyHandle> InHandle, FMaterialLayersFunctions* InMaterialFunction);

	static bool FilterLayerAssets(const struct FAssetData& InAssetData, FMaterialLayersFunctions* LayerFunction, EMaterialParameterAssociation MaterialType, int32 Index);

	static FReply OnClickedSaveNewMaterialInstance(class UMaterialInterface* Object, UObject* EditorObject);

	static void CopyMaterialToInstance(class UMaterialInstanceConstant* ChildInstance, TArray<struct FEditorParameterGroup> &ParameterGroups);
	static void TransitionAndCopyParameters(class UMaterialInstanceConstant* ChildInstance, TArray<struct FEditorParameterGroup> &ParameterGroups, bool bForceCopy = false);
	static FReply OnClickedSaveNewFunctionInstance(class UMaterialFunctionInterface* Object, class UMaterialInterface* PreviewMaterial, UObject* EditorObject);
	static FReply OnClickedSaveNewLayerInstance(class UMaterialFunctionInterface* Object, TSharedPtr<FStackSortedData> InSortedData);

	static void GetVectorChannelMaskComboBoxStrings(TArray<TSharedPtr<FString>>& OutComboBoxStrings, TArray<TSharedPtr<class SToolTip>>& OutToolTips, TArray<bool>& OutRestrictedItems);
	static FString GetVectorChannelMaskValue(class UDEditorParameterValue* InParameter);
	static void SetVectorChannelMaskValue(const FString& StringValue, TSharedPtr<IPropertyHandle> PropertyHandle, class UDEditorParameterValue* InParameter, UObject* MaterialEditorInstance);

	static TArray<class UFactory*> GetAssetFactories(EMaterialParameterAssociation AssetType);
	/**
	*  Returns group for parameter. Creates one if needed.
	*
	* @param ParameterGroup		Name to be looked for.
	*/
	static FEditorParameterGroup&  GetParameterGroup(class UMaterial* InMaterial, FName& ParameterGroup, TArray<struct FEditorParameterGroup>& ParameterGroups);

	static TSharedRef<SWidget> MakeStackReorderHandle(TSharedPtr<SMaterialLayersFunctionsInstanceTreeItem> InOwningStack);

	static bool OnShouldSetCurveAsset(const FAssetData& AssetData, TSoftObjectPtr<class UCurveLinearColorAtlas> InAtlas);
	static void SetPositionFromCurveAsset(const FAssetData& AssetData, TSoftObjectPtr<class UCurveLinearColorAtlas> InAtlas, class UDEditorScalarParameterValue* InParameter, TSharedPtr<IPropertyHandle> PropertyHandle, UObject* MaterialEditorInstance);

	static void ResetCurveToDefault(TSharedPtr<IPropertyHandle> PropertyHandle, class UDEditorParameterValue* Parameter, UMaterialEditorInstanceConstant* MaterialEditorInstance);

	static FText LayerID;
	static FText BlendID;
	static FName LayerParamName;
};

