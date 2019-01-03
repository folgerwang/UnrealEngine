// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "SMaterialParametersOverviewWidget.h"
#include "MaterialEditor/DEditorFontParameterValue.h"
#include "MaterialEditor/DEditorMaterialLayersParameterValue.h"
#include "MaterialEditor/DEditorScalarParameterValue.h"
#include "MaterialEditor/DEditorStaticComponentMaskParameterValue.h"
#include "MaterialEditor/DEditorStaticSwitchParameterValue.h"
#include "MaterialEditor/DEditorTextureParameterValue.h"
#include "MaterialEditor/DEditorVectorParameterValue.h"
#include "MaterialEditor/MaterialEditorInstanceConstant.h"
#include "Materials/Material.h"
#include "PropertyHandle.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "ISinglePropertyView.h"
#include "Materials/MaterialInstanceConstant.h"
#include "EditorStyleSet.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "IPropertyRowGenerator.h"
#include "Widgets/Views/STreeView.h"
#include "IDetailTreeNode.h"
#include "AssetThumbnail.h"
#include "MaterialEditorInstanceDetailCustomization.h"
#include "MaterialPropertyHelpers.h"
#include "DetailWidgetRow.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorSupportDelegates.h"
#include "Widgets/Images/SImage.h"
#include "MaterialEditor/MaterialEditorPreviewParameters.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Materials/MaterialFunctionInstance.h"
#include "Framework/Application/SlateApplication.h"
#include "Toolkits/AssetEditorManager.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Curves/CurveLinearColor.h"
#include "Curves/CurveLinearColorAtlas.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "Widgets/Layout/SWidgetSwitcher.h"


#define LOCTEXT_NAMESPACE "MaterialLayerCustomization"

FString SMaterialParametersOverviewTreeItem::GetCurvePath(UDEditorScalarParameterValue* Parameter) const
{
	FString Path = Parameter->AtlasData.Curve->GetPathName();
	return Path;
}

const FSlateBrush* SMaterialParametersOverviewTreeItem::GetBorderImage() const
{
	if (IsHovered())
	{
		return FEditorStyle::GetBrush("DetailsView.CategoryMiddle_Hovered");
	}
	else
	{
		return FEditorStyle::GetBrush("DetailsView.CategoryMiddle");
	}
}

void SMaterialParametersOverviewTreeItem::RefreshOnRowChange(const FAssetData& AssetData, TSharedPtr<SMaterialParametersOverviewTree> InTree)
{
	if (InTree.IsValid())
	{
		InTree->CreateGroupsWidget();
	}
}

void SMaterialParametersOverviewTreeItem::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
{
	StackParameterData = InArgs._StackParameterData;
	MaterialEditorInstance = InArgs._MaterialEditorInstance;
	Tree = InArgs._InTree;
	ColumnSizeData.LeftColumnWidth = TAttribute<float>(Tree.Pin().Get(), &SMaterialParametersOverviewTree::OnGetLeftColumnWidth);
	ColumnSizeData.RightColumnWidth = TAttribute<float>(Tree.Pin().Get(), &SMaterialParametersOverviewTree::OnGetRightColumnWidth);
	ColumnSizeData.OnWidthChanged = SSplitter::FOnSlotResized::CreateSP(Tree.Pin().Get(), &SMaterialParametersOverviewTree::OnSetColumnWidth);

	TSharedRef<SWidget> LeftSideWidget = SNullWidget::NullWidget;
	TSharedRef<SWidget> RightSideWidget = SNullWidget::NullWidget;
	FText NameOverride;
	TSharedRef<SVerticalBox> WrapperWidget = SNew(SVerticalBox);

// GROUP --------------------------------------------------
	if (StackParameterData->StackDataType == EStackDataType::Group)
	{
		NameOverride = FText::FromName(StackParameterData->Group.GroupName);
		LeftSideWidget = SNew(STextBlock)
			.Text(NameOverride)
			.TextStyle(FEditorStyle::Get(), "TinyText");
	}
// END GROUP

// PROPERTY ----------------------------------------------
	if (StackParameterData->StackDataType == EStackDataType::Property)
	{
		UDEditorStaticComponentMaskParameterValue* CompMaskParam = Cast<UDEditorStaticComponentMaskParameterValue>(StackParameterData->Parameter);
		UDEditorVectorParameterValue* VectorParam = Cast<UDEditorVectorParameterValue>(StackParameterData->Parameter);
		UDEditorScalarParameterValue* ScalarParam = Cast<UDEditorScalarParameterValue>(StackParameterData->Parameter);

		TAttribute<bool> IsParamEnabled = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateStatic(&FMaterialPropertyHelpers::IsOverriddenExpression, StackParameterData->Parameter));
		NameOverride = FText::FromName(StackParameterData->Parameter->ParameterInfo.Name);

		IDetailTreeNode& Node = *StackParameterData->ParameterNode;
		TSharedPtr<IDetailPropertyRow> GeneratedRow = StaticCastSharedPtr<IDetailPropertyRow>(Node.GetRow());
		IDetailPropertyRow& Row = *GeneratedRow.Get();
		Row.DisplayName(NameOverride);

		if (VectorParam && VectorParam->bIsUsedAsChannelMask)
		{
			FOnGetPropertyComboBoxStrings GetMaskStrings = FOnGetPropertyComboBoxStrings::CreateStatic(&FMaterialPropertyHelpers::GetVectorChannelMaskComboBoxStrings);
			FOnGetPropertyComboBoxValue GetMaskValue = FOnGetPropertyComboBoxValue::CreateStatic(&FMaterialPropertyHelpers::GetVectorChannelMaskValue, StackParameterData->Parameter);
			FOnPropertyComboBoxValueSelected SetMaskValue = FOnPropertyComboBoxValueSelected::CreateStatic(&FMaterialPropertyHelpers::SetVectorChannelMaskValue, StackParameterData->ParameterNode->CreatePropertyHandle(), StackParameterData->Parameter, (UObject*)MaterialEditorInstance);

			FDetailWidgetRow& CustomWidget = Row.CustomWidget();
			CustomWidget
			.FilterString(NameOverride)
			.NameContent()
			[
				SNew(STextBlock)
				.Text(NameOverride)
				.ToolTipText(FMaterialPropertyHelpers::GetParameterExpressionDescription(StackParameterData->Parameter, MaterialEditorInstance))
				.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
			]
			.ValueContent()
			.MaxDesiredWidth(200.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.AutoWidth()
					[
						PropertyCustomizationHelpers::MakePropertyComboBox(StackParameterData->ParameterNode->CreatePropertyHandle(), GetMaskStrings, GetMaskValue, SetMaskValue)
					]
				]
			];
		}
		else if (ScalarParam && ScalarParam->AtlasData.bIsUsedAsAtlasPosition)
		{
			const FText ParameterName = FText::FromName(StackParameterData->Parameter->ParameterInfo.Name);

			FDetailWidgetRow& CustomWidget = Row.CustomWidget();
			CustomWidget
				.FilterString(ParameterName)
				.NameContent()
				[
					SNew(STextBlock)
					.Text(ParameterName)
					.ToolTipText(FMaterialPropertyHelpers::GetParameterExpressionDescription(StackParameterData->Parameter, MaterialEditorInstance))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
				.ValueContent()
				.HAlign(HAlign_Fill)
				.MaxDesiredWidth(400.0f)
				[
					SNew(SObjectPropertyEntryBox)
					.ObjectPath(this, &SMaterialParametersOverviewTreeItem::GetCurvePath, ScalarParam)
					.AllowedClass(UCurveLinearColor::StaticClass())
					.NewAssetFactories(TArray<UFactory*>())
					.DisplayThumbnail(true)
					.ThumbnailPool(Tree.Pin()->GetTreeThumbnailPool())
					.OnShouldSetAsset(FOnShouldSetAsset::CreateStatic(&FMaterialPropertyHelpers::OnShouldSetCurveAsset, ScalarParam->AtlasData.Atlas))
					.OnObjectChanged(FOnSetObject::CreateStatic(&FMaterialPropertyHelpers::SetPositionFromCurveAsset, ScalarParam->AtlasData.Atlas, ScalarParam, StackParameterData->ParameterHandle, (UObject*)MaterialEditorInstance))
					.DisplayCompactSize(true)
				];
			
		}
		else if (!CompMaskParam)
		{
			FNodeWidgets StoredNodeWidgets = Node.CreateNodeWidgets();
			TSharedRef<SWidget> StoredRightSideWidget = StoredNodeWidgets.ValueWidget.ToSharedRef();
			StackParameterData->ParameterNode->CreatePropertyHandle()->MarkResetToDefaultCustomized(true);
			FDetailWidgetRow& CustomWidget = Row.CustomWidget();
			CustomWidget
			.FilterString(NameOverride)
			.NameContent()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NameOverride)
					.ToolTipText(FMaterialPropertyHelpers::GetParameterExpressionDescription(StackParameterData->Parameter, MaterialEditorInstance))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			]
			.ValueContent()
			[
				StoredRightSideWidget
			];

		}
		else
		{
			TSharedPtr<IPropertyHandle> RMaskProperty = StackParameterData->ParameterNode->CreatePropertyHandle()->GetChildHandle("R");
			TSharedPtr<IPropertyHandle> GMaskProperty = StackParameterData->ParameterNode->CreatePropertyHandle()->GetChildHandle("G");
			TSharedPtr<IPropertyHandle> BMaskProperty = StackParameterData->ParameterNode->CreatePropertyHandle()->GetChildHandle("B");
			TSharedPtr<IPropertyHandle> AMaskProperty = StackParameterData->ParameterNode->CreatePropertyHandle()->GetChildHandle("A");
			FDetailWidgetRow& CustomWidget = Row.CustomWidget();
			CustomWidget
			.FilterString(NameOverride)
			.NameContent()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(NameOverride)
					.ToolTipText(FMaterialPropertyHelpers::GetParameterExpressionDescription(StackParameterData->Parameter, MaterialEditorInstance))
					.Font(FEditorStyle::GetFontStyle(TEXT("PropertyWindow.NormalFont")))
				]
			]
			.ValueContent()
			.MaxDesiredWidth(200.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.AutoWidth()
					[
						RMaskProperty->CreatePropertyNameWidget(FText::GetEmpty(), FText::GetEmpty(), false)
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.AutoWidth()
					[
						RMaskProperty->CreatePropertyValueWidget()
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
					.AutoWidth()
					[
						GMaskProperty->CreatePropertyNameWidget(FText::GetEmpty(), FText::GetEmpty(), false)
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.AutoWidth()
					[
						GMaskProperty->CreatePropertyValueWidget()
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
					.AutoWidth()
					[
						BMaskProperty->CreatePropertyNameWidget(FText::GetEmpty(), FText::GetEmpty(), false)
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.AutoWidth()
					[
						BMaskProperty->CreatePropertyValueWidget()
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.Padding(FMargin(10.0f, 0.0f, 0.0f, 0.0f))
					.AutoWidth()
					[
						AMaskProperty->CreatePropertyNameWidget(FText::GetEmpty(), FText::GetEmpty(), false)
					]
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.AutoWidth()
					[
						AMaskProperty->CreatePropertyValueWidget()
					]
				]	
			];
		}

		FNodeWidgets NodeWidgets = Node.CreateNodeWidgets();
		LeftSideWidget = NodeWidgets.NameWidget.ToSharedRef();
		RightSideWidget = NodeWidgets.ValueWidget.ToSharedRef();
	}
// END PROPERTY

// PROPERTY CHILD ----------------------------------------
	if (StackParameterData->StackDataType == EStackDataType::PropertyChild)
	{
		FNodeWidgets NodeWidgets = StackParameterData->ParameterNode->CreateNodeWidgets();
		LeftSideWidget = NodeWidgets.NameWidget.ToSharedRef();
		RightSideWidget = NodeWidgets.ValueWidget.ToSharedRef();
	}
// END PROPERTY CHILD

// FINAL WRAPPER
	{
		WrapperWidget->AddSlot()
			.AutoHeight()
			[
				SNew(SBorder)
				.Padding(0.0f)
				.BorderImage(this, &SMaterialParametersOverviewTreeItem::GetBorderImage)
				[
					SNew(SSplitter)
					.Style(FEditorStyle::Get(), "DetailsView.Splitter")
					.PhysicalSplitterHandleSize(1.0f)
					.HitDetectionSplitterHandleSize(5.0f)
					+ SSplitter::Slot()
					.Value(ColumnSizeData.LeftColumnWidth)
					.OnSlotResized(ColumnSizeData.OnWidthChanged)
					.Value(0.25f)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						.Padding(FMargin(3.0f))
						[
							SNew(SExpanderArrow, SharedThis(this))
						]
						+ SHorizontalBox::Slot()
						.Padding(FMargin(2.0f))
						.VAlign(VAlign_Center)
						[
							LeftSideWidget
						]
					]
					+ SSplitter::Slot()
					.Value(ColumnSizeData.RightColumnWidth)
					.OnSlotResized(ColumnSizeData.OnWidthChanged)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.MaxWidth(350.0f)
						.Padding(FMargin(5.0f, 2.0f, 0.0f, 2.0f))
						[
							RightSideWidget
						]
					]
				]
			];
	}


	this->ChildSlot
		[
			WrapperWidget
		];


	STableRow< TSharedPtr<FSortedParamData> >::ConstructInternal(
		STableRow::FArguments()
		.Style(FEditorStyle::Get(), "DetailsView.TreeView.TableRow")
		.ShowSelection(false),
		InOwnerTableView
	);
}

void SMaterialParametersOverviewTree::Construct(const FArguments& InArgs)
{
	bHasAnyParameters = false;
	ColumnWidth = 0.5f;
	MaterialEditorInstance = InArgs._InMaterialEditorInstance;
	Owner = InArgs._InOwner;
	CreateGroupsWidget();


	STreeView<TSharedPtr<FSortedParamData>>::Construct(
		STreeView::FArguments()
		.TreeItemsSource(&SortedParameters)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow(this, &SMaterialParametersOverviewTree::OnGenerateRowMaterialLayersFunctionsTreeView)
		.OnGetChildren(this, &SMaterialParametersOverviewTree::OnGetChildrenMaterialLayersFunctionsTreeView)
		.OnExpansionChanged(this, &SMaterialParametersOverviewTree::OnExpansionChanged)
	);
}

TSharedRef< ITableRow > SMaterialParametersOverviewTree::OnGenerateRowMaterialLayersFunctionsTreeView(TSharedPtr<FSortedParamData> Item, const TSharedRef< STableViewBase >& OwnerTable)
{
	TSharedRef< SMaterialParametersOverviewTreeItem > ReturnRow = SNew(SMaterialParametersOverviewTreeItem, OwnerTable)
		.StackParameterData(Item)
		.MaterialEditorInstance(MaterialEditorInstance)
		.InTree(SharedThis(this));
	return ReturnRow;
}

void SMaterialParametersOverviewTree::OnGetChildrenMaterialLayersFunctionsTreeView(TSharedPtr<FSortedParamData> InParent, TArray< TSharedPtr<FSortedParamData> >& OutChildren)
{
	OutChildren = InParent->Children;
}

void SMaterialParametersOverviewTree::OnExpansionChanged(TSharedPtr<FSortedParamData> Item, bool bIsExpanded)
{
	bool* ExpansionValue = MaterialEditorInstance->OriginalMaterial->ParameterOverviewExpansion.Find(Item->NodeKey);
	if (ExpansionValue == nullptr)
	{
		MaterialEditorInstance->OriginalMaterial->ParameterOverviewExpansion.Add(Item->NodeKey, bIsExpanded);
	}
	else if (*ExpansionValue != bIsExpanded)
	{
		MaterialEditorInstance->OriginalMaterial->ParameterOverviewExpansion.Emplace(Item->NodeKey, bIsExpanded);
	}
	// Expand any children that are also expanded
	for (auto Child : Item->Children)
	{
		bool* ChildExpansionValue = MaterialEditorInstance->OriginalMaterial->ParameterOverviewExpansion.Find(Child->NodeKey);
		if (ChildExpansionValue != nullptr && *ChildExpansionValue == true)
		{
			SetItemExpansion(Child, true);
		}
	}
}

void SMaterialParametersOverviewTree::SetParentsExpansionState()
{
	for (const auto& Pair : SortedParameters)
	{
		if (Pair->Children.Num())
		{
			bool* bIsExpanded = MaterialEditorInstance->OriginalMaterial->ParameterOverviewExpansion.Find(Pair->NodeKey);
			if (bIsExpanded)
			{
				SetItemExpansion(Pair, *bIsExpanded);
			}
			else
			{
				SetItemExpansion(Pair, true);
			}
		}
	}
}

TSharedPtr<class FAssetThumbnailPool> SMaterialParametersOverviewTree::GetTreeThumbnailPool()
{
	return Generator->GetGeneratedThumbnailPool();
}

void SMaterialParametersOverviewTree::CreateGroupsWidget()
{
	check(MaterialEditorInstance);
	MaterialEditorInstance->RegenerateArrays();
	UnsortedParameters.Empty();
	SortedParameters.Empty();
	FPropertyEditorModule& Module = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	if (!Generator.IsValid())
	{
		FPropertyRowGeneratorArgs Args;
		Generator = Module.CreatePropertyRowGenerator(Args);

		TArray<UObject*> Objects;
		Objects.Add(MaterialEditorInstance);
		Generator->SetObjects(Objects);
	}
	else
	{
		TArray<UObject*> Objects;
		Objects.Add(MaterialEditorInstance);
		Generator->SetObjects(Objects);
	}
	const TArray<TSharedRef<IDetailTreeNode>> TestData = Generator->GetRootTreeNodes();
	TSharedPtr<IDetailTreeNode> Category = TestData[0];
	TSharedPtr<IDetailTreeNode> ParameterGroups;
	TArray<TSharedRef<IDetailTreeNode>> Children;
	Category->GetChildren(Children);

	for (int32 ChildIdx = 0; ChildIdx < Children.Num(); ChildIdx++)
	{
		if (Children[ChildIdx]->CreatePropertyHandle().IsValid() &&
			Children[ChildIdx]->CreatePropertyHandle()->GetProperty()->GetName() == "ParameterGroups")
		{
			ParameterGroups = Children[ChildIdx];
			break;
		}
	}

	Children.Empty();
	ParameterGroups->GetChildren(Children);
	for (int32 GroupIdx = 0; GroupIdx < Children.Num(); ++GroupIdx)
	{
		bHasAnyParameters = true;
		TArray<void*> GroupPtrs;
		TSharedPtr<IPropertyHandle> ChildHandle = Children[GroupIdx]->CreatePropertyHandle();
		ChildHandle->AccessRawData(GroupPtrs);
		auto GroupIt = GroupPtrs.CreateConstIterator();
		const FEditorParameterGroup* ParameterGroupPtr = reinterpret_cast<FEditorParameterGroup*>(*GroupIt);
		const FEditorParameterGroup& ParameterGroup = *ParameterGroupPtr;

		for (int32 ParamIdx = 0; ParamIdx < ParameterGroup.Parameters.Num(); ParamIdx++)
		{
			UDEditorParameterValue* Parameter = ParameterGroup.Parameters[ParamIdx];

			TSharedPtr<IPropertyHandle> ParametersArrayProperty = ChildHandle->GetChildHandle("Parameters");
			TSharedPtr<IPropertyHandle> ParameterProperty = ParametersArrayProperty->GetChildHandle(ParamIdx);
			TSharedPtr<IPropertyHandle> ParameterValueProperty = ParameterProperty->GetChildHandle("ParameterValue");

			{
				FUnsortedParamData NonLayerProperty;
				UDEditorScalarParameterValue* ScalarParam = Cast<UDEditorScalarParameterValue>(Parameter);

				if (ScalarParam && ScalarParam->SliderMax > ScalarParam->SliderMin)
				{
					ParameterValueProperty->SetInstanceMetaData("UIMin", FString::Printf(TEXT("%f"), ScalarParam->SliderMin));
					ParameterValueProperty->SetInstanceMetaData("UIMax", FString::Printf(TEXT("%f"), ScalarParam->SliderMax));
				}

				NonLayerProperty.Parameter = Parameter;
				NonLayerProperty.ParameterGroup = ParameterGroup;
				NonLayerProperty.ParameterNode = Generator->FindTreeNode(ParameterValueProperty);
				NonLayerProperty.ParameterHandle = NonLayerProperty.ParameterNode->CreatePropertyHandle();
				NonLayerProperty.UnsortedName = Parameter->ParameterInfo.Name;

				UnsortedParameters.Add(NonLayerProperty);
			}
		}
	}
	ShowSubParameters();
	RequestTreeRefresh();
	SetParentsExpansionState();
}



void SMaterialParametersOverviewTree::ShowSubParameters()
{
	for (FUnsortedParamData Property : UnsortedParameters)
	{
		UDEditorParameterValue* Parameter = Property.Parameter;
		{
			TSharedPtr<FSortedParamData> GroupProperty(new FSortedParamData());
			GroupProperty->StackDataType = EStackDataType::Group;
			GroupProperty->ParameterInfo.Index = Parameter->ParameterInfo.Index;
			GroupProperty->ParameterInfo.Association = Parameter->ParameterInfo.Association;
			GroupProperty->Group = Property.ParameterGroup;
			GroupProperty->NodeKey = FString::FromInt(GroupProperty->ParameterInfo.Index) + FString::FromInt(GroupProperty->ParameterInfo.Association) + Property.ParameterGroup.GroupName.ToString();

			bool bAddNewGroup = true;
			for (TSharedPtr<struct FSortedParamData> GroupChild : SortedParameters)
			{
				if (GroupChild->NodeKey == GroupProperty->NodeKey)
				{
					bAddNewGroup = false;
				}
			}
			if (bAddNewGroup)
			{
				SortedParameters.Add(GroupProperty);
			}

			TSharedPtr<FSortedParamData> ChildProperty(new FSortedParamData());
			ChildProperty->StackDataType = EStackDataType::Property;
			ChildProperty->Parameter = Parameter;
			ChildProperty->ParameterInfo.Index = Parameter->ParameterInfo.Index;
			ChildProperty->ParameterInfo.Association = Parameter->ParameterInfo.Association;
			ChildProperty->ParameterNode = Property.ParameterNode;
			ChildProperty->PropertyName = Property.UnsortedName;
			ChildProperty->NodeKey = FString::FromInt(ChildProperty->ParameterInfo.Index) + FString::FromInt(ChildProperty->ParameterInfo.Association) +  Property.ParameterGroup.GroupName.ToString() + Property.UnsortedName.ToString();


			UDEditorStaticComponentMaskParameterValue* CompMaskParam = Cast<UDEditorStaticComponentMaskParameterValue>(Parameter);
			if (!CompMaskParam)
			{
				TArray<TSharedRef<IDetailTreeNode>> ParamChildren;
				Property.ParameterNode->GetChildren(ParamChildren);
				for (int32 ParamChildIdx = 0; ParamChildIdx < ParamChildren.Num(); ParamChildIdx++)
				{
					TSharedPtr<FSortedParamData> ParamChildProperty(new FSortedParamData());
					ParamChildProperty->StackDataType = EStackDataType::PropertyChild;
					ParamChildProperty->ParameterNode = ParamChildren[ParamChildIdx];
					ParamChildProperty->ParameterHandle = ParamChildProperty->ParameterNode->CreatePropertyHandle();
					ParamChildProperty->ParameterInfo.Index = Parameter->ParameterInfo.Index;
					ParamChildProperty->ParameterInfo.Association = Parameter->ParameterInfo.Association;
					ParamChildProperty->Parameter = ChildProperty->Parameter;
					ChildProperty->Children.Add(ParamChildProperty);
				}
			}
			for (TSharedPtr<struct FSortedParamData> GroupChild : SortedParameters)
			{
				if (GroupChild->Group.GroupName == Property.ParameterGroup.GroupName
					&& GroupChild->ParameterInfo.Association == ChildProperty->ParameterInfo.Association
					&&  GroupChild->ParameterInfo.Index == ChildProperty->ParameterInfo.Index)
				{
					GroupChild->Children.Add(ChildProperty);
				}
			}

		}
	}
}

const FSlateBrush* SMaterialParametersOverviewPanel::GetBackgroundImage() const
{
	return FEditorStyle::GetBrush("DetailsView.CategoryTop_Hovered");
}

int32 SMaterialParametersOverviewPanel::GetPanelIndex() const
{
	return NestedTree && NestedTree->HasAnyParameters() ? 1 : 0;
}

void SMaterialParametersOverviewPanel::Refresh()
{
	TSharedPtr<SHorizontalBox> HeaderBox;
	NestedTree->CreateGroupsWidget();

	FOnClicked 	OnChildButtonClicked = FOnClicked();
	if (MaterialEditorInstance->OriginalFunction)
	{
		OnChildButtonClicked = FOnClicked::CreateStatic(&FMaterialPropertyHelpers::OnClickedSaveNewFunctionInstance, ImplicitConv<UMaterialFunctionInterface*>(MaterialEditorInstance->OriginalFunction), ImplicitConv<UMaterialInterface*>(MaterialEditorInstance->PreviewMaterial), ImplicitConv<UObject*>(MaterialEditorInstance));
	}
	else
	{
		OnChildButtonClicked = FOnClicked::CreateStatic(&FMaterialPropertyHelpers::OnClickedSaveNewMaterialInstance, ImplicitConv<UMaterialInterface*>(MaterialEditorInstance->OriginalMaterial), ImplicitConv<UObject*>(MaterialEditorInstance));
	}

	{
		this->ChildSlot
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBorder)
					.BorderImage(this, &SMaterialParametersOverviewPanel::GetBackgroundImage)
					.Padding(FMargin(4.0f))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SAssignNew(HeaderBox, SHorizontalBox)
							+ SHorizontalBox::Slot()
							.Padding(FMargin(3.0f, 1.0f))
							.HAlign(HAlign_Left)
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(LOCTEXT("ParameterDefaults", "Parameter Defaults"))
								.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
								.ShadowOffset(FVector2D(1.0f, 1.0f))
							]
						]
						+ SVerticalBox::Slot()
						.Padding(FMargin(3.0f, 2.0f, 3.0f, 3.0f))
						.AutoHeight()
						[
							SNew(SBorder)
							.BorderImage(FEditorStyle::GetBrush("DetailsView.CategoryTop"))
							[
								SNew(SWidgetSwitcher)
								.WidgetIndex(this, &SMaterialParametersOverviewPanel::GetPanelIndex)
								+SWidgetSwitcher::Slot()
								[
									SNew(STextBlock)
									.Text(LOCTEXT("AddParams", "Add parameters to see them here."))
								]
								+SWidgetSwitcher::Slot()
								[
									NestedTree.ToSharedRef()
								]	
							]
						]
					]
				]
			];

		HeaderBox->AddSlot()
			.FillWidth(1.0f)
			[
				SNullWidget::NullWidget
			];
		
		if (NestedTree->HasAnyParameters())
		{
			HeaderBox->AddSlot()
				.AutoWidth()
				.Padding(2.0f)
				[
					SNew(SButton)
					.ButtonStyle(FEditorStyle::Get(), "FlatButton.DarkGrey")
					.HAlign(HAlign_Center)
					.OnClicked(OnChildButtonClicked)
					.ToolTipText(LOCTEXT("SaveToChildInstance", "Save To Child Instance"))
					.Content()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.10"))
							.TextStyle(FEditorStyle::Get(), "NormalText.Important")
							.Text(FText::FromString(FString(TEXT("\xf0c7 \xf149"))) /*fa-filter*/)
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(STextBlock)
							.TextStyle(FEditorStyle::Get(), "NormalText.Important")
							.Text(FText::FromString(FString(TEXT(" Save Child"))) /*fa-filter*/)
						]
					]
				];
		}
	}
	
}


void SMaterialParametersOverviewPanel::Construct(const FArguments& InArgs)
{
	NestedTree = SNew(SMaterialParametersOverviewTree)
		.InMaterialEditorInstance(InArgs._InMaterialEditorInstance)
		.InOwner(SharedThis(this));

	MaterialEditorInstance = InArgs._InMaterialEditorInstance;
	FEditorSupportDelegates::UpdateUI.AddSP(this, &SMaterialParametersOverviewPanel::Refresh);

}

void SMaterialParametersOverviewPanel::UpdateEditorInstance(UMaterialEditorPreviewParameters* InMaterialEditorInstance)
{
	NestedTree->MaterialEditorInstance = InMaterialEditorInstance;
	Refresh();
}


TSharedPtr<class IPropertyRowGenerator> SMaterialParametersOverviewPanel::GetGenerator()
{
	 return NestedTree->GetGenerator();
}

#undef LOCTEXT_NAMESPACE