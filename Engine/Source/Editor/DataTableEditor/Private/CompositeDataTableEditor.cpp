// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CompositeDataTableEditor.h"
#include "SCompositeRowEditor.h"
#include "Engine/CompositeDataTable.h"
#include "Dom/JsonObject.h"
#include "Widgets/Text/STextBlock.h"
#include "Misc/FileHelper.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SScrollBar.h"
#include "Framework/Layout/Overscroll.h"
#include "Widgets/Layout/SScrollBox.h"
#include "EditorStyleSet.h"
#include "DataTableEditorModule.h"
#include "PropertyEditorModule.h"
#include "Editor.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Views/SListView.h"
#include "SRowEditor.h"
#include "IDocumentation.h"
#include "Widgets/SToolTip.h"
 
#define LOCTEXT_NAMESPACE "CompositeDataTableEditor"

const FName FCompositeDataTableEditor::PropertiesTabId("CompositeDataTableEditor_Properties");
const FName FCompositeDataTableEditor::StackTabId("CompositeDataTableEditor_Stack");


FCompositeDataTableEditor::FCompositeDataTableEditor()
{
}

FCompositeDataTableEditor::~FCompositeDataTableEditor()
{
}

void FCompositeDataTableEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FDataTableEditor::RegisterTabSpawners(InTabManager);

	CreateAndRegisterPropertiesTab(InTabManager);
}

void FCompositeDataTableEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FDataTableEditor::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(PropertiesTabId);
	InTabManager->UnregisterTabSpawner(StackTabId);

	DetailsView.Reset();
	StackTabWidget.Reset();
}

void FCompositeDataTableEditor::CreateAndRegisterRowEditorTab(const TSharedRef<class FTabManager>& InTabManager)
{
	// no row editor in the composite data tables
	RowEditorTabWidget.Reset();
}

void FCompositeDataTableEditor::CreateAndRegisterPropertiesTab(const TSharedRef<class FTabManager>& InTabManager)
{
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	const FDetailsViewArgs DetailsViewArgs(/*bIsUpdatable*/false, /*bIsLockable*/false, true, FDetailsViewArgs::ObjectsUseNameArea, false);
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	InTabManager->RegisterTabSpawner(PropertiesTabId, FOnSpawnTab::CreateSP(this, &FCompositeDataTableEditor::SpawnTab_Properties))
		.SetDisplayName(LOCTEXT("PropertiesTab", "Properties"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FCompositeDataTableEditor::InitDataTableEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UDataTable* Table)
{
	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_CompositeDataTableEditor_temp_Layout")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Horizontal)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->SetSizeCoefficient(0.3f)	
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.3f)
					->SetHideTabWell(true)
					->AddTab(PropertiesTabId, ETabState::OpenedTab)
				)
// 				->Split
// 				(
// 					FTabManager::NewStack()		
// 					->SetHideTabWell(true)
// 					->AddTab(StackTabId, ETabState::OpenedTab)
// 				)

			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetOrientation(Orient_Vertical)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.1f)
					->SetHideTabWell(true)
					->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
				)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(DataTableTabId, ETabState::OpenedTab)
				)
// 				->Split
// 				(
// 					FTabManager::NewStack()
// 					->AddTab(RowEditorTabId, ETabState::OpenedTab)
// 				)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, FDataTableEditorModule::DataTableEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Table);

	FDataTableEditorModule& DataTableEditorModule = FModuleManager::LoadModuleChecked<FDataTableEditorModule>("DataTableEditor");
	AddMenuExtender(DataTableEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	// Support undo/redo
	GEditor->RegisterForUndo(this);

	if (DetailsView.IsValid())
	{
		// Make sure details window is pointing to our object
		DetailsView->SetObject(GetEditingObject());
	}
}

TSharedRef<SDockTab> FCompositeDataTableEditor::SpawnTab_Stack(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == StackTabId);

	return SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("DataTableEditor.Tabs.Properties"))
		.Label(LOCTEXT("StackTitle", "Datatable Stack"))
		.TabColorScale(GetTabColorScale())
		[
			SNew(SBorder)
			.Padding(2)
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Fill)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				StackTabWidget.ToSharedRef()
			]
		];
}

TSharedRef<SDockTab> FCompositeDataTableEditor::SpawnTab_Properties(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == PropertiesTabId);

	return SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("DataTableEditor.Tabs.Properties"))
		.Label(LOCTEXT("PropertiesTitle", "Properties"))
		.TabColorScale(GetTabColorScale())
		[
			DetailsView.ToSharedRef()
		];
}

TSharedRef<SWidget> FCompositeDataTableEditor::CreateStackBox()
{
	UDataTable* Table = Cast<UDataTable>(GetEditingObject());

	// Support undo/redo
	if (Table)
	{
		Table->SetFlags(RF_Transactional);
	}

	return CreateRowEditor(Table);
}

TSharedRef<SRowEditor> FCompositeDataTableEditor::CreateRowEditor(UDataTable* Table)
{
	UCompositeDataTable* DataTable = Cast<UCompositeDataTable>(Table);
	return SNew(SCompositeRowEditor, DataTable);

}

#undef LOCTEXT_NAMESPACE
