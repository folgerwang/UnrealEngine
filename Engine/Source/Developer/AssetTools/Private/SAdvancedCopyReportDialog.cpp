// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.


#include "SAdvancedCopyReportDialog.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SWindow.h"
#include "Layout/WidgetPath.h"
#include "SlateOptMacros.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Interfaces/IMainFrameModule.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Input/SCheckBox.h"
#include "AssetToolsModule.h"

#define LOCTEXT_NAMESPACE "AdvancedCopyReportDialog"

static const FName AssetColumnLabel = TEXT("Asset");
static const FName SourceColumnLabel = TEXT("Source");
static const FName RelativeDestinationColumnLabel = TEXT("RelativeDestination");

struct FCompareFAdvancedCopyReportNodeByName
{
	FORCEINLINE bool operator()( TSharedPtr<FAdvancedCopyReportNode> A, TSharedPtr<FAdvancedCopyReportNode> B ) const
	{
		return A->Source < B->Source;
	}
};

FAdvancedCopyReportNode::FAdvancedCopyReportNode()
{

}

FAdvancedCopyReportNode::FAdvancedCopyReportNode(const FString& InSource, const FString& InDestination)
	: Source(InSource)
	, Destination(InDestination)
{}

void FAdvancedCopyReportNode::AddPackage(const FString& InSource, const FString& InDestination, const FString& DependencyOf)
{
	AddPackage_Recursive(InSource, InDestination, DependencyOf);
}

void FAdvancedCopyReportNode::ExpandChildrenRecursively(const TSharedRef<SAdvancedCopyReportTree>& TreeView)
{
	for ( auto ChildIt = Children.CreateConstIterator(); ChildIt; ++ChildIt )
	{
		TreeView->SetItemExpansion(*ChildIt, false);
		(*ChildIt)->ExpandChildrenRecursively(TreeView);
	}
}

bool FAdvancedCopyReportNode::AddPackage_Recursive(const FString& InSource, const FString& InDestination, const FString& DependencyOf)
{
	TSharedPtr<FAdvancedCopyReportNode> Child;
	// If this is not a dependency of an asset, add it to the top of the tree
	if (DependencyOf.IsEmpty())
	{
		int32 ChildIdx = Children.Add(MakeShareable(new FAdvancedCopyReportNode(InSource, InDestination)));
		Child = Children[ChildIdx];
		Children.Sort(FCompareFAdvancedCopyReportNodeByName());
		return true;
	}
	else if (Source == DependencyOf)
	{
		for (auto ChildIt = Children.CreateConstIterator(); ChildIt; ++ChildIt)
		{
			if ((*ChildIt)->Source == InSource)
			{
				Child = (*ChildIt);
				break;
			}
		}

		// If one was not found, create it
		if (!Child.IsValid())
		{
			int32 ChildIdx = Children.Add(MakeShareable(new FAdvancedCopyReportNode(InSource, InDestination)));
			Child = Children[ChildIdx];
			Children.Sort(FCompareFAdvancedCopyReportNodeByName());
			return true;
		}
	}
	else
	{
		bool bFoundDependencyInChild = false;
		for (auto ChildIt = Children.CreateConstIterator(); ChildIt; ++ChildIt)
		{
			bFoundDependencyInChild = ChildIt->Get()->AddPackage_Recursive(InSource, InDestination, DependencyOf);
			if (bFoundDependencyInChild)
			{
				return true;
			}
		}
		return false;
	}
	return false;
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAdvancedCopyReportDialog::Construct( const FArguments& InArgs, const FAdvancedCopyParams& InParams, const FText& InReportMessage, const TArray<TMap<FString, FString>>& DestinationMap, const TArray<TMap<FName, FName>>& DependencyMap, const FOnReportConfirmed& InOnReportConfirmed )
{
	OnReportConfirmed = InOnReportConfirmed;
	CurrentCopyParams = InParams;
	ReportString = InReportMessage.ToString();
	ConstructNodeTree(DestinationMap, DependencyMap);

	TSharedRef< SHeaderRow > HeaderRowWidget =
		SNew(SHeaderRow);

	TSharedPtr<SAdvancedCopyColumn> PackageColumn = MakeShareable(new SAdvancedCopyColumn(AssetColumnLabel));
	Columns.Add(PackageColumn->GetColumnID(), PackageColumn);
	HeaderRowWidget->AddColumn(PackageColumn->ConstructHeaderRowColumn());

	TSharedPtr<SAdvancedCopyColumn> SourceColumn = MakeShareable(new SAdvancedCopyColumn(SourceColumnLabel));
	Columns.Add(SourceColumn->GetColumnID(), SourceColumn);
	HeaderRowWidget->AddColumn(SourceColumn->ConstructHeaderRowColumn());

	TSharedPtr<SAdvancedCopyColumn> DestinationColumn = MakeShareable(new SAdvancedCopyColumn(RelativeDestinationColumnLabel));
	Columns.Add(DestinationColumn->GetColumnID(), DestinationColumn);
	HeaderRowWidget->AddColumn(DestinationColumn->ConstructHeaderRowColumn());


	ChildSlot
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("Docking.Tab.ContentAreaBrush") )
		.Padding(FMargin(4, 8, 4, 4))
		[
			SNew(SVerticalBox)

			// Report Message
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 4)
			[
				SNew(STextBlock)
				.Text(this, &SAdvancedCopyReportDialog::GetHeaderText, InReportMessage)
				.TextStyle( FEditorStyle::Get(), "PackageMigration.DialogTitle" )
			]

			// Tree of packages in the report
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SNew(SBorder)
				.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder") )
				[
					SAssignNew( ReportTreeView, SAdvancedCopyReportTree )
					.HeaderRow(HeaderRowWidget)
					.TreeItemsSource(&PackageReportRootNode.Children)
					.ItemHeight(18)
					.SelectionMode(ESelectionMode::Single)
					.OnGenerateRow( this, &SAdvancedCopyReportDialog::GenerateTreeRow )
					.OnGetChildren( this, &SAdvancedCopyReportDialog::GetChildrenForTree )
				]
			]

			// Options
			+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0, 4)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SCheckBox)
						.ToolTipText(LOCTEXT("GenerateDependenciesToCopyTooltip", "Toggle whether or not to search for dependencies. Toggling this will rebuild the destination list."))
						.Type(ESlateCheckBoxType::CheckBox)
						.IsChecked(this, &SAdvancedCopyReportDialog::IsGeneratingDependencies)
						.OnCheckStateChanged(this, &SAdvancedCopyReportDialog::ToggleGeneratingDependencies)
						.Padding(4)
					]
					+ SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("GenerateDependenciesToCopy", "Generate Dependencies to Copy"))
					]
				]

			// Ok/Cancel buttons
			+SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Right)
			.Padding(0,4,0,0)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(FEditorStyle::GetMargin("StandardDialog.SlotPadding"))
				.MinDesiredSlotWidth(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotWidth"))
				.MinDesiredSlotHeight(FEditorStyle::GetFloat("StandardDialog.MinDesiredSlotHeight"))
				+SUniformGridPanel::Slot(0,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding( FEditorStyle::GetMargin("StandardDialog.ContentPadding") )
					.OnClicked(this, &SAdvancedCopyReportDialog::OkClicked)
					.Text(LOCTEXT("OkButton", "OK"))
				]
				+SUniformGridPanel::Slot(1,0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.ContentPadding( FEditorStyle::GetMargin("StandardDialog.ContentPadding") )
					.OnClicked(this, &SAdvancedCopyReportDialog::CancelClicked)
					.Text(LOCTEXT("CancelButton", "Cancel"))
				]
			]
		]
	];

	if ( ensure(ReportTreeView.IsValid()) )
	{
		PackageReportRootNode.ExpandChildrenRecursively(ReportTreeView.ToSharedRef());
	}
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

FText SAdvancedCopyReportDialog::GetHeaderText(const FText InReportMessage) const
{
	if (PackageReportRootNode.Children.Num() == 0)
	{
		return LOCTEXT("NoValidSources", "You have not selected any valid sources for advanced copying.");
	}
	return FText::Format(LOCTEXT("AdvancedCopyDesc", "The following files will be copied to {0} and references to copied files will be fixed up."), InReportMessage);
}

ECheckBoxState SAdvancedCopyReportDialog::IsGeneratingDependencies() const
{
	return CurrentCopyParams.bShouldCheckForDependencies ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SAdvancedCopyReportDialog::ToggleGeneratingDependencies(ECheckBoxState NewState)
{
	CurrentCopyParams.bShouldCheckForDependencies = NewState == ECheckBoxState::Checked;
	CloseDialog();
	IAssetTools& AssetTools = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools").Get();
	AssetTools.InitAdvancedCopyFromCopyParams(CurrentCopyParams);
}

void SAdvancedCopyReportDialog::OpenPackageReportDialog(const FAdvancedCopyParams& InParams, const FText& ReportMessage, const TArray<TMap<FString, FString>>& DestinationMap, const TArray<TMap<FName, FName>>& DependencyMap, const FOnReportConfirmed& InOnReportConfirmed)
{
	TSharedRef<SWindow> ReportWindow = SNew(SWindow)
		.Title(LOCTEXT("AdvancedCopyReportWindowTitle", "Advanced Copy Asset Report"))
		.ClientSize( FVector2D(800, 600) )
		.SupportsMaximize(true)
		.SupportsMinimize(true)
		[
			SNew(SAdvancedCopyReportDialog, InParams, ReportMessage, DestinationMap, DependencyMap, InOnReportConfirmed)
		];

	IMainFrameModule& MainFrameModule = FModuleManager::LoadModuleChecked<IMainFrameModule>(TEXT("MainFrame"));
	if ( MainFrameModule.GetParentWindow().IsValid() )
	{
		FSlateApplication::Get().AddWindowAsNativeChild(ReportWindow, MainFrameModule.GetParentWindow().ToSharedRef());
	}
	else
	{
		FSlateApplication::Get().AddWindow(ReportWindow);
	}
}

void SAdvancedCopyReportDialog::CloseDialog()
{
	TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());

	if ( Window.IsValid() )
	{
		Window->RequestDestroyWindow();
	}
}

TSharedRef<ITableRow> SAdvancedCopyReportDialog::GenerateTreeRow( TSharedPtr<FAdvancedCopyReportNode> TreeItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(SAdvancedCopyTreeRow, ReportTreeView.ToSharedRef(), SharedThis(this)).Item(TreeItem);
}

void SAdvancedCopyReportDialog::GetChildrenForTree( TSharedPtr<FAdvancedCopyReportNode> TreeItem, TArray< TSharedPtr<FAdvancedCopyReportNode> >& OutChildren )
{
	OutChildren = TreeItem->Children;
}

void SAdvancedCopyReportDialog::ConstructNodeTree(const TArray<TMap<FString, FString>>& DestinationMap, const TArray<TMap<FName, FName>>& DependencyMap)
{
	for (int32 MapIndex = 0; MapIndex < DestinationMap.Num(); MapIndex++)
	{
		TMap<FString, FString> SingleDestinationMap = DestinationMap[MapIndex];
		TMap<FName, FName> SingleDependencyMap = DependencyMap[MapIndex];
		for (auto PackageIt = SingleDestinationMap.CreateConstIterator(); PackageIt; ++PackageIt)
		{
			FString SourceString = *PackageIt.Key();
			const FName* DependencyOfPtr = SingleDependencyMap.Find(FName(*SourceString));
			FString DependencyOf = FString();
			if (DependencyOfPtr)
			{
				DependencyOf = DependencyOfPtr->ToString();
			}
			PackageReportRootNode.AddPackage(SourceString, *PackageIt.Value(), DependencyOf);
		}
	}
}

FReply SAdvancedCopyReportDialog::OkClicked()
{
	CloseDialog();
	OnReportConfirmed.ExecuteIfBound();

	return FReply::Handled();
}

FReply SAdvancedCopyReportDialog::CancelClicked()
{
	CloseDialog();

	return FReply::Handled();
}


void SAdvancedCopyTreeRow::Construct(const FArguments& InArgs, const TSharedRef<SAdvancedCopyReportTree>& OutlinerTreeView, TSharedRef<SAdvancedCopyReportDialog> AdvancedCopyReport)
{
	Item = InArgs._Item;
	ReportDialogWeak = AdvancedCopyReport;
	auto Args = FSuperRowType::FArguments()
		.Style(&FEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("SceneOutliner.TableViewRow"));

	SMultiColumnTableRow<TSharedPtr<FAdvancedCopyReportNode>>::Construct(Args, OutlinerTreeView);
}

TSharedRef<SWidget> SAdvancedCopyTreeRow::GenerateWidgetForColumn(const FName& ColumnName)
{
	auto ItemPtr = Item.Pin();
	if (!ItemPtr.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	// Create the widget for this item
	TSharedRef<SWidget> NewItemWidget = SNullWidget::NullWidget;

	auto Column = ReportDialogWeak.Pin()->GetColumns().FindRef(ColumnName);
	if (Column.IsValid())
	{
		NewItemWidget = Column->ConstructRowWidget(ItemPtr.ToSharedRef(), *this);
	}

	if (ColumnName == AssetColumnLabel)
	{
		// The first column gets the tree expansion arrow for this row
		return
			SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(6, 0, 0, 0)
			[
				SNew(SExpanderArrow, SharedThis(this)).IndentAmount(12)
			]

		+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				NewItemWidget
			];
	}
	else
	{
		// Other columns just get widget content -- no expansion arrow needed
		return NewItemWidget;
	}
}

SHeaderRow::FColumn::FArguments SAdvancedCopyColumn::ConstructHeaderRowColumn()
{
	return SHeaderRow::Column(GetColumnID())
		.FillWidth(2.f)
		[
			SNew(STextBlock)
			.Text(FText::FromString(FName::NameToDisplayString(ColumnName.ToString(), false)))
		];
}

const TSharedRef< SWidget > SAdvancedCopyColumn::ConstructRowWidget(TSharedPtr<struct FAdvancedCopyReportNode> TreeItem, const SAdvancedCopyTreeRow& Row)
{
	if (ColumnName == AssetColumnLabel)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(FPaths::GetBaseFilename(TreeItem.Get()->Source)));
	}
	else if (ColumnName == SourceColumnLabel)
	{
		return SNew(STextBlock)
			.Text(FText::FromString(TreeItem.Get()->Source));
	}
	else if (ColumnName == RelativeDestinationColumnLabel)
	{
		FString CroppedDestination = TreeItem.Get()->Destination;
		const bool bDidRemovePrefix = CroppedDestination.RemoveFromStart(Row.GetReportDialog()->GetReportString());
		if (bDidRemovePrefix)
		{
			CroppedDestination = TEXT(".") + CroppedDestination;
		}
		return SNew(STextBlock)
			.Text(FText::FromString(FPaths::GetPath(CroppedDestination)));
	}
	return SNullWidget::NullWidget;
}

#undef LOCTEXT_NAMESPACE