// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#include "FbxCompareWindow.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/STableRow.h"
#include "EditorStyleSet.h"
#include "Factories/FbxAnimSequenceImportData.h"
#include "IDocumentation.h"
#include "PropertyEditorModule.h"
#include "IDetailsView.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Factories/FbxSceneImportFactory.h"
#include "Toolkits/AssetEditorManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Materials/Material.h"


#define LOCTEXT_NAMESPACE "FBXOption"
bool SFbxCompareWindow::HasConflict()
{
	if (ResultObject->IsA(USkeletalMesh::StaticClass()))
	{
		for (TSharedPtr<FSkeletonCompareData> SkeletonCompareData : DisplaySkeletonTreeItem)
		{
			if (SkeletonCompareData->bChildConflict)
			{
				//We have at least one skeleton conflict
				return true;
			}
		}
	}
	return false;
}

void SFbxCompareWindow::Construct(const FArguments& InArgs)
{
	bRevertReimport = false;

	bShowSectionFlag[EFBXCompareSection_General] = false;
	bShowSectionFlag[EFBXCompareSection_Skeleton] = true;

	WidgetWindow = InArgs._WidgetWindow;
	if (InArgs._AssetReferencingSkeleton != nullptr)
	{
		//Copy the aray
		AssetReferencingSkeleton = *(InArgs._AssetReferencingSkeleton);
	}
	SourceData = InArgs._SourceData;
	ResultData = InArgs._ResultData;
	ResultObject = InArgs._ResultObject;
	SourceObject = InArgs._SourceObject;
	FbxGeneralInfo = InArgs._FbxGeneralInfo;

	FillGeneralListItem();


	if (ResultObject->IsA(USkeletalMesh::StaticClass()))
	{
		FilSkeletonTreeItem();
	}

	SetMatchJointInfo();

	// Skeleton comparison
	TSharedPtr<SWidget> SkeletonCompareSection = ConstructSkeletonComparison();
	// General section
	TSharedPtr<SWidget> GeneralInfoSection = ConstructGeneralInfo();

	TSharedRef<SScrollBar> VerticalScrollBar = SNew(SScrollBar)
		.Orientation(EOrientation::Orient_Vertical)
		.AlwaysShowScrollbar(false);

	this->ChildSlot
		[
		SNew(SBox)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			[
				SNew(SScrollBox)
				+ SScrollBox::Slot()
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.FillHeight(1.0f)
						.Padding(2)
						[
							SNew(SVerticalBox)
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(2)
							[
								GeneralInfoSection.ToSharedRef()
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(2)
							[
								// Skeleton Compare section
								SkeletonCompareSection.ToSharedRef()
							]
						]
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0.0f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("SFbxCompareWindow_Preview_Done", "Done"))
					.OnClicked(this, &SFbxCompareWindow::OnDone)
				]
			]
		]
	];	
}

FReply SFbxCompareWindow::SetSectionVisible(EFBXCompareSection SectionIndex)
{
	bShowSectionFlag[SectionIndex] = !bShowSectionFlag[SectionIndex];
	return FReply::Handled();
}

EVisibility SFbxCompareWindow::IsSectionVisible(EFBXCompareSection SectionIndex)
{
	return bShowSectionFlag[SectionIndex] ? EVisibility::All : EVisibility::Collapsed;
}

const FSlateBrush* SFbxCompareWindow::GetCollapsableArrow(EFBXCompareSection SectionIndex) const
{
	return bShowSectionFlag[SectionIndex] ? FEditorStyle::GetBrush("Symbols.DownArrow") : FEditorStyle::GetBrush("Symbols.RightArrow");
}

TSharedPtr<SWidget> SFbxCompareWindow::ConstructGeneralInfo()
{
	return SNew(SBox)
	.MaxDesiredHeight(205)
	[
		SNew(SBorder)
		.Padding(FMargin(3))
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.IsFocusable(false)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.OnClicked(this, &SFbxCompareWindow::SetSectionVisible, EFBXCompareSection_General)
					[
						SNew(SImage).Image(this, &SFbxCompareWindow::GetCollapsableArrow, EFBXCompareSection_General)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
					.Text(LOCTEXT("SFbxCompareWindow_GeneralInfoHeader", "Fbx File Information"))
				]
			
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2)
			[
				SNew(SBox)
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SFbxCompareWindow::IsSectionVisible, EFBXCompareSection_General)))
				[
					SNew(SBorder)
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					[
						//Show the general fbx information
						SNew(SListView<TSharedPtr<FString>>)
						.ListItemsSource(&GeneralListItem)
						.OnGenerateRow(this, &SFbxCompareWindow::OnGenerateRowGeneralFbxInfo)
					]
				]
			]
		]
	];
}

TSharedRef<ITableRow> SFbxCompareWindow::OnGenerateRowGeneralFbxInfo(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	int32 GeneralListIndex = GeneralListItem.Find(InItem);
	bool LightBackgroundColor = GeneralListIndex % 2 == 1;
	return SNew(STableRow<TSharedPtr<FString> >, OwnerTable)
		[
			SNew(STextBlock).Text(FText::FromString(*(InItem.Get())))
		];
}

FSlateColor FMaterialCompareData::GetCellColor(FCompMesh *DataA, int32 MaterialIndexA, int32 MaterialMatchA, FCompMesh *DataB, int32 MaterialIndexB, bool bSkinxxError) const
{
	if (!DataA->CompMaterials.IsValidIndex(MaterialIndexA))
	{
		return FSlateColor::UseForeground();
	}

	bool bMatchIndexChanged = MaterialMatchA == INDEX_NONE || (DataA->CompMaterials.IsValidIndex(MaterialIndexA) && DataB->CompMaterials.IsValidIndex(MaterialIndexB) && MaterialMatchA == MaterialIndexB);

	if ((CompareDisplayOption == NoMatch || CompareDisplayOption == All) && MaterialMatchA == INDEX_NONE)
	{
		//There is no match for this material, so it will be add to the material array
		return FSlateColor(FLinearColor(0.7f, 0.3f, 0.0f));
	}
	if ((CompareDisplayOption == IndexChanged || CompareDisplayOption == All) && !bMatchIndexChanged)
	{
		//The match index has changed, so index base gameplay will be broken
		return FSlateColor(FLinearColor::Yellow);
	}
	if ((CompareDisplayOption == SkinxxError || CompareDisplayOption == All) && bSkinxxError)
	{
		//Skinxx error
		return FSlateColor(FLinearColor::Red);
	}
	return FSlateColor::UseForeground();
}

TSharedRef<SWidget> FMaterialCompareData::ConstructCell(FCompMesh *MeshData, int32 MeshMaterialIndex)
{
	if (!MeshData->CompMaterials.IsValidIndex(MeshMaterialIndex))
	{
		return SNew(SBox)
			.Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
			[
				SNew(STextBlock)
				.Text(FText::GetEmpty())
			];
	}

	return SNew(SBorder)
		.Padding(FMargin(5.0f, 0.0f, 0.0f, 0.0f))
		[
			SNew(STextBlock)
			.Text(this, &FMaterialCompareData::GetCellString, MeshData == FbxData)
			.ToolTipText(this, &FMaterialCompareData::GetCellTooltipString, MeshData == FbxData)
			.ColorAndOpacity(this, MeshData == CurrentData ? &FMaterialCompareData::GetCurrentCellColor : &FMaterialCompareData::GetFbxCellColor)
		];
}

FText FMaterialCompareData::GetCellString(bool IsFbxData) const
{
	FCompMesh *MeshData = IsFbxData ? FbxData : CurrentData;
	int32 MaterialIndex = IsFbxData ? FbxMaterialIndex : CurrentMaterialIndex;

	if (!MeshData->CompMaterials.IsValidIndex(MaterialIndex))
	{
		return FText(LOCTEXT("GetCellString_InvalidIndex", "-"));
	}

	FString CellContent = MeshData->CompMaterials[MaterialIndex].ImportedMaterialSlotName.ToString();
	return FText::FromString(CellContent);
}

FText FMaterialCompareData::GetCellTooltipString(bool IsFbxData) const
{
	FCompMesh *MeshData = IsFbxData ? FbxData : CurrentData;
	int32 MaterialIndex = IsFbxData ? FbxMaterialIndex : CurrentMaterialIndex;
	bool bSkinxxDuplicate = IsFbxData ? bFbxSkinxxDuplicate : bCurrentSkinxxDuplicate;
	bool bSkinxxMissing = IsFbxData ? bFbxSkinxxMissing : bCurrentSkinxxMissing;
	if (!MeshData->CompMaterials.IsValidIndex(MaterialIndex))
	{
		return FText(LOCTEXT("GetCellString_InvalidIndex", "-"));
	}

	FString CellTooltip = TEXT("Material Slot Name: ") + MeshData->CompMaterials[MaterialIndex].MaterialSlotName.ToString();
	if (bSkinxxDuplicate)
	{
		CellTooltip += TEXT(" (skinxx duplicate)");
	}
	if (bSkinxxMissing)
	{
		CellTooltip += TEXT(" (skinxx missing)");
	}
	return FText::FromString(CellTooltip);
}


FSlateColor FMaterialCompareData::GetCurrentCellColor() const
{
	return GetCellColor(CurrentData, CurrentMaterialIndex, CurrentMaterialMatch, FbxData, FbxMaterialIndex, bCurrentSkinxxDuplicate || bCurrentSkinxxMissing);
}

TSharedRef<SWidget> FMaterialCompareData::ConstructCellCurrent()
{
	return ConstructCell(CurrentData, CurrentMaterialIndex);
}

FSlateColor FMaterialCompareData::GetFbxCellColor() const
{
	return GetCellColor(FbxData, FbxMaterialIndex, FbxMaterialMatch, CurrentData, CurrentMaterialIndex, bFbxSkinxxDuplicate || bFbxSkinxxMissing);
}

TSharedRef<SWidget> FMaterialCompareData::ConstructCellFbx()
{
	return ConstructCell(FbxData, FbxMaterialIndex);
}

void SFbxCompareWindow::FillGeneralListItem()
{
	GeneralListItem.Add(MakeShareable(new FString(FbxGeneralInfo.UE4SdkVersion)));
	GeneralListItem.Add(MakeShareable(new FString(FbxGeneralInfo.ApplicationCreator)));
	GeneralListItem.Add(MakeShareable(new FString(FbxGeneralInfo.CreationDate)));
	GeneralListItem.Add(MakeShareable(new FString(FbxGeneralInfo.FileVersion)));
	GeneralListItem.Add(MakeShareable(new FString(FbxGeneralInfo.AxisSystem)));
	GeneralListItem.Add(MakeShareable(new FString(FbxGeneralInfo.UnitSystem)));
}

TSharedPtr<SWidget> SFbxCompareWindow::ConstructSkeletonComparison()
{
	if (!ResultObject->IsA(USkeletalMesh::StaticClass()))
	{
		//Return an empty widget, we do not show the skeleton when the mesh is not a skeletal mesh
		return SNew(SBox);
	}

	FString SkeletonStatusTooltip;
	if (AssetReferencingSkeleton.Num() > 0)
	{
		SkeletonStatusTooltip += TEXT("Skeleton is references by ") + FString::FromInt(AssetReferencingSkeleton.Num()) + TEXT(" assets.");
	}
	
	FText SkeletonStatus = FText(ResultData->CompSkeleton.bSkeletonFitMesh ? LOCTEXT("SFbxCompareWindow_ConstructSkeletonComparison_MatchAndMerge", "The skeleton can be merged") : LOCTEXT("SFbxCompareWindow_ConstructSkeletonComparison_CannotMatchAndMerge", "The skeleton must be regenerated, it cannot be merged"));
	
	CompareTree = SNew(STreeView< TSharedPtr<FSkeletonCompareData> >)
		.ItemHeight(24)
		.SelectionMode(ESelectionMode::None)
		.TreeItemsSource(&DisplaySkeletonTreeItem)
		.OnGenerateRow(this, &SFbxCompareWindow::OnGenerateRowCompareTreeView)
		.OnGetChildren(this, &SFbxCompareWindow::OnGetChildrenRowCompareTreeView);


	return SNew(SBox)
	.MaxDesiredHeight(600)
	[
		SNew(SBorder)
		.Padding(FMargin(3))
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.IsFocusable(false)
					.ButtonStyle(FEditorStyle::Get(), "NoBorder")
					.OnClicked(this, &SFbxCompareWindow::SetSectionVisible, EFBXCompareSection_Skeleton)
					[
						SNew(SImage).Image(this, &SFbxCompareWindow::GetCollapsableArrow, EFBXCompareSection_Skeleton)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
					.Text(LOCTEXT("SFbxCompareWindow_SkeletonCompareHeader", "Skeleton"))
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(2)
			[
				SNew(SBox)
				.Visibility(TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateSP(this, &SFbxCompareWindow::IsSectionVisible, EFBXCompareSection_Skeleton)))
				[
					SNew(SBorder)
					.Padding(FMargin(3))
					.BorderImage(FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
					[
						SNew(SVerticalBox)
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2)
						[
							SNew(STextBlock)
							.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
							.Text(SkeletonStatus)
							.ToolTipText(FText::FromString(SkeletonStatusTooltip))
							.ColorAndOpacity(ResultData->CompSkeleton.bSkeletonFitMesh ? FSlateColor::UseForeground() : FSlateColor(FLinearColor(0.7f, 0.3f, 0.0f)))
						]
						+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(2)
						[
							SNew(SSeparator)
							.Orientation(EOrientation::Orient_Horizontal)
						]
						+SVerticalBox::Slot()
						.FillHeight(1.0f)
						.Padding(2)
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
							.FillHeight(1.0f)
							[
								CompareTree.ToSharedRef()
							]
							+ SVerticalBox::Slot()
							.AutoHeight()
							.Padding(2)
							[
								SNew(SSeparator)
								.Orientation(EOrientation::Orient_Horizontal)
							]
							+SVerticalBox::Slot()
							.AutoHeight()
							.MaxHeight(200.0f)
							[
								//Show the general fbx information
								SNew(SListView<TSharedPtr<FString>>)
								.ListItemsSource(&AssetReferencingSkeleton)
								.OnGenerateRow(this, &SFbxCompareWindow::OnGenerateRowAssetReferencingSkeleton)
							]
						]
					]
				]
			]
		]
	];
}

class SCompareSkeletonTreeViewItem : public STableRow< TSharedPtr<FSkeletonCompareData> >
{
public:

	SLATE_BEGIN_ARGS(SCompareSkeletonTreeViewItem)
		: _SkeletonCompareData(nullptr)
		, _SourceData(nullptr)
		, _ResultData(nullptr)
	{}

	/** The item content. */
	SLATE_ARGUMENT(TSharedPtr<FSkeletonCompareData>, SkeletonCompareData)
	SLATE_ARGUMENT(FCompMesh*, SourceData)
	SLATE_ARGUMENT(FCompMesh*, ResultData)
	SLATE_END_ARGS()

	/**
	* Construct the widget
	*
	* @param InArgs   A declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		SkeletonCompareData = InArgs._SkeletonCompareData;
		SourceData = InArgs._SourceData;
		ResultData = InArgs._ResultData;

		//This is suppose to always be valid
		check(SkeletonCompareData.IsValid());
		check(SourceData != nullptr);
		check(ResultData != nullptr);

		const FSlateBrush* JointIcon = SkeletonCompareData->bMatchJoint ? FEditorStyle::GetDefaultBrush() : SkeletonCompareData->FbxJointIndex != INDEX_NONE ? FEditorStyle::GetBrush("FBXIcon.ReimportCompareAdd") : FEditorStyle::GetBrush("FBXIcon.ReimportCompareRemoved");

		//Prepare the tooltip
		FString Tooltip = SkeletonCompareData->bMatchJoint ? TEXT("") : FText(SkeletonCompareData->FbxJointIndex != INDEX_NONE ? LOCTEXT("SCompareSkeletonTreeViewItem_AddJoint_tooltip", "Fbx reimport will add this joint") : LOCTEXT("SCompareSkeletonTreeViewItem_RemoveJoint_tooltip", "Fbx reimport will remove this joint")).ToString();

		this->ChildSlot
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SExpanderArrow, SharedThis(this))
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 2.0f, 6.0f, 2.0f)
				[
					SNew(SImage)
					.Image(JointIcon)
					.Visibility(JointIcon != FEditorStyle::GetDefaultBrush() ? EVisibility::Visible : EVisibility::Collapsed)
				]
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.Padding(0.0f, 3.0f, 6.0f, 3.0f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(FText::FromString(SkeletonCompareData->JointName.ToString()))
					.ToolTipText(FText::FromString(Tooltip))
					.ColorAndOpacity(SkeletonCompareData->bMatchJoint && !SkeletonCompareData->bChildConflict ? FSlateColor::UseForeground() : FSlateColor(FLinearColor(0.7f, 0.3f, 0.0f)))
				]
			];

		STableRow< TSharedPtr<FSkeletonCompareData> >::ConstructInternal(
			STableRow::FArguments()
			.ShowSelection(true),
			InOwnerTableView
			);
	}

private:
	/** The node info to build the tree view row from. */
	TSharedPtr<FSkeletonCompareData> SkeletonCompareData;
	FCompMesh *SourceData;
	FCompMesh *ResultData;
};


TSharedRef<ITableRow> SFbxCompareWindow::OnGenerateRowCompareTreeView(TSharedPtr<FSkeletonCompareData> RowData, const TSharedRef<STableViewBase>& Table)
{
	TSharedRef< SCompareSkeletonTreeViewItem > ReturnRow = SNew(SCompareSkeletonTreeViewItem, Table)
		.SkeletonCompareData(RowData)
		.SourceData(SourceData)
		.ResultData(ResultData);
	return ReturnRow;
}

void SFbxCompareWindow::OnGetChildrenRowCompareTreeView(TSharedPtr<FSkeletonCompareData> InParent, TArray< TSharedPtr<FSkeletonCompareData> >& OutChildren)
{
	for (int32 ChildIndex = 0; ChildIndex < InParent->ChildJoints.Num(); ++ChildIndex)
	{
		TSharedPtr<FSkeletonCompareData> ChildJoint = InParent->ChildJoints[ChildIndex];
		if (ChildJoint.IsValid())
		{
			OutChildren.Add(ChildJoint);
		}
	}
}

TSharedRef<ITableRow> SFbxCompareWindow::OnGenerateRowAssetReferencingSkeleton(TSharedPtr<FString> InItem, const TSharedRef<STableViewBase>& OwnerTable)
{
	int32 AssetListIndex = AssetReferencingSkeleton.Find(InItem);
	bool LightBackgroundColor = AssetListIndex % 2 == 1;
	return SNew(STableRow<TSharedPtr<FString> >, OwnerTable)
		[
			SNew(SBorder)
			.BorderImage(LightBackgroundColor ? FEditorStyle::GetBrush("ToolPanel.GroupBorder") : FEditorStyle::GetBrush("ToolPanel.DarkGroupBorder"))
			[
				SNew(STextBlock)
				.Text(FText::FromString(*(InItem.Get())))
				//.ColorAndOpacity(LightBackgroundColor ? FSlateColor::UseSubduedForeground() : FSlateColor::UseForeground())
			]
		];
}

void SFbxCompareWindow::FilSkeletonTreeItem()
{
	//Create all the entries
	for (int32 RowIndex = 0; RowIndex < SourceData->CompSkeleton.Joints.Num(); ++RowIndex)
	{
		TSharedPtr<FSkeletonCompareData> CompareRowData = MakeShareable(new FSkeletonCompareData());
		int32 AddedIndex = CurrentSkeletonTreeItem.Add(CompareRowData);
		check(AddedIndex == RowIndex);
		CompareRowData->CurrentJointIndex = RowIndex;
		CompareRowData->JointName = SourceData->CompSkeleton.Joints[RowIndex].Name;
		CompareRowData->ChildJointIndexes = SourceData->CompSkeleton.Joints[RowIndex].ChildIndexes;
	}

	//Set the childrens and parent pointer
	for (int32 RowIndex = 0; RowIndex < SourceData->CompSkeleton.Joints.Num(); ++RowIndex)
	{
		check(CurrentSkeletonTreeItem.IsValidIndex(RowIndex));
		TSharedPtr<FSkeletonCompareData> CompareRowData = CurrentSkeletonTreeItem[RowIndex];
		if (CurrentSkeletonTreeItem.IsValidIndex(SourceData->CompSkeleton.Joints[RowIndex].ParentIndex))
		{
			CompareRowData->ParentJoint = CurrentSkeletonTreeItem[SourceData->CompSkeleton.Joints[RowIndex].ParentIndex];
		}
		
		for (int32 ChildJointIndex = 0; ChildJointIndex < CompareRowData->ChildJointIndexes.Num(); ++ChildJointIndex)
		{
			if (CurrentSkeletonTreeItem.IsValidIndex(CompareRowData->ChildJointIndexes[ChildJointIndex]))
			{
				CompareRowData->ChildJoints.Add(CurrentSkeletonTreeItem[CompareRowData->ChildJointIndexes[ChildJointIndex]]);
			}
		}
	}

	for (int32 RowIndex = 0; RowIndex < ResultData->CompSkeleton.Joints.Num(); ++RowIndex)
	{
		TSharedPtr<FSkeletonCompareData> CompareRowData = MakeShareable(new FSkeletonCompareData());
		int32 AddedIndex = FbxSkeletonTreeItem.Add(CompareRowData);
		check(AddedIndex == RowIndex);
		CompareRowData->FbxJointIndex = RowIndex;
		CompareRowData->JointName = ResultData->CompSkeleton.Joints[RowIndex].Name;
		CompareRowData->ChildJointIndexes = ResultData->CompSkeleton.Joints[RowIndex].ChildIndexes;
	}

	//Set the childrens and parent pointer
	for (int32 RowIndex = 0; RowIndex < ResultData->CompSkeleton.Joints.Num(); ++RowIndex)
	{
		check(FbxSkeletonTreeItem.IsValidIndex(RowIndex));
		TSharedPtr<FSkeletonCompareData> CompareRowData = FbxSkeletonTreeItem[RowIndex];
		if (FbxSkeletonTreeItem.IsValidIndex(ResultData->CompSkeleton.Joints[RowIndex].ParentIndex))
		{
			CompareRowData->ParentJoint = FbxSkeletonTreeItem[ResultData->CompSkeleton.Joints[RowIndex].ParentIndex];
		}

		for (int32 ChildJointIndex = 0; ChildJointIndex < CompareRowData->ChildJointIndexes.Num(); ++ChildJointIndex)
		{
			if (FbxSkeletonTreeItem.IsValidIndex(CompareRowData->ChildJointIndexes[ChildJointIndex]))
			{
				CompareRowData->ChildJoints.Add(FbxSkeletonTreeItem[CompareRowData->ChildJointIndexes[ChildJointIndex]]);
			}
		}
	}
}

void SFbxCompareWindow::RecursiveMatchJointInfo(TSharedPtr<FSkeletonCompareData> SkeletonItem)
{
	TArray<TSharedPtr<FSkeletonCompareData>> DisplayChilds;
	//Find the display child
	if (CurrentSkeletonTreeItem.IsValidIndex(SkeletonItem->CurrentJointIndex))
	{
		for (int32 ChildIndex = 0; ChildIndex < CurrentSkeletonTreeItem[SkeletonItem->CurrentJointIndex]->ChildJoints.Num(); ++ChildIndex)
		{
			DisplayChilds.Add(CurrentSkeletonTreeItem[SkeletonItem->CurrentJointIndex]->ChildJoints[ChildIndex]);
		}
	}
	if (FbxSkeletonTreeItem.IsValidIndex(SkeletonItem->FbxJointIndex))
	{
		for (int32 ChildIndex = 0; ChildIndex < FbxSkeletonTreeItem[SkeletonItem->FbxJointIndex]->ChildJoints.Num(); ++ChildIndex)
		{
			TSharedPtr<FSkeletonCompareData> FbxSkeletonItem = FbxSkeletonTreeItem[SkeletonItem->FbxJointIndex]->ChildJoints[ChildIndex];
			bool bFoundChildMatch = false;
			for (TSharedPtr<FSkeletonCompareData> DisplayChildJoint : DisplayChilds)
			{
				if (DisplayChildJoint->JointName == FbxSkeletonItem->JointName)
				{
					bFoundChildMatch = true;
					DisplayChildJoint->bMatchJoint = true;
					DisplayChildJoint->FbxJointIndex = FbxSkeletonItem->FbxJointIndex;
					break;
				}
			}
			if (!bFoundChildMatch)
			{
				DisplayChilds.Add(FbxSkeletonItem);
			}
		}
	}

	if (!SkeletonItem->bMatchJoint)
	{
		TSharedPtr<FSkeletonCompareData> ParentSkeletonItem = SkeletonItem->ParentJoint;
		while (ParentSkeletonItem.IsValid() && ParentSkeletonItem->bChildConflict == false)
		{
			ParentSkeletonItem->bChildConflict = true;
			ParentSkeletonItem = ParentSkeletonItem->ParentJoint;
		}
	}
	//Set the new child list to the display joint
	SkeletonItem->ChildJoints = DisplayChilds;
	SkeletonItem->ChildJointIndexes.Empty();
	for (TSharedPtr<FSkeletonCompareData> ChildJoint : SkeletonItem->ChildJoints)
	{
		ChildJoint->ParentJoint = SkeletonItem;
		RecursiveMatchJointInfo(ChildJoint);
	}
}

void SFbxCompareWindow::SetMatchJointInfo()
{
	TArray<TSharedPtr<FSkeletonCompareData>> RootJoint;
	for (TSharedPtr<FSkeletonCompareData> CurrentSkeletonItem : CurrentSkeletonTreeItem)
	{
		if (!CurrentSkeletonItem->ParentJoint.IsValid())
		{
			DisplaySkeletonTreeItem.Add(CurrentSkeletonItem);
		}
	}
	for (TSharedPtr<FSkeletonCompareData> CurrentSkeletonItem : FbxSkeletonTreeItem)
	{
		if (!CurrentSkeletonItem->ParentJoint.IsValid())
		{
			bool bInsertJoint = true;
			for (TSharedPtr<FSkeletonCompareData> DisplayTreeItem : DisplaySkeletonTreeItem)
			{
				if(DisplayTreeItem->JointName == CurrentSkeletonItem->JointName)
				{
					DisplayTreeItem->FbxJointIndex = CurrentSkeletonItem->FbxJointIndex;
					DisplayTreeItem->bMatchJoint = true;
					bInsertJoint = false;
				}
			}
			if (bInsertJoint)
			{
				DisplaySkeletonTreeItem.Add(CurrentSkeletonItem);
			}
		}
	}

	for (int32 SkeletonTreeIndex = 0; SkeletonTreeIndex < DisplaySkeletonTreeItem.Num(); ++SkeletonTreeIndex)
	{
		RecursiveMatchJointInfo(DisplaySkeletonTreeItem[SkeletonTreeIndex]);
	}
}
#undef LOCTEXT_NAMESPACE
