// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "AssetEditor/TimecodeSynchronizerEditorToolkit.h"

#include "TimecodeSynchronizer.h"
#include "UI/TimecodeSynchronizerEditorCommand.h"
#include "UI/TimecodeSynchronizerEditorStyle.h"
#include "Widgets/STimecodeSynchronizerSourceViewer.h"

#include "Editor.h"
#include "IDetailsView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Docking/SDockTab.h"
 
#define LOCTEXT_NAMESPACE "TimecodeSynchronizerEditor"

namespace TimecodeSynchronizerEditorToolkit
{
	const FName AppIdentifier = TEXT("TimecodeSynchronizerEditorApp");
	const FName PropertiesTabId(TEXT("TimecodeSynchronizerEditor_Properties"));
	const FName SourceViewerTabId(TEXT("TimecodeSynchronizerEditor_SourceViewer"));
	const FName Layout(TEXT("Standalone_TimecodeSynchronizerEditor_Layout_v0"));
}

TSharedRef<FTimecodeSynchronizerEditorToolkit> FTimecodeSynchronizerEditorToolkit::CreateEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, class UTimecodeSynchronizer* InTimecodeSynchronizer)
{
	TSharedRef<FTimecodeSynchronizerEditorToolkit> NewEditor(new FTimecodeSynchronizerEditorToolkit());
	NewEditor->InitTimecodeSynchronizerEditor(Mode, InitToolkitHost, InTimecodeSynchronizer);
	return NewEditor;
}

void FTimecodeSynchronizerEditorToolkit::InitTimecodeSynchronizerEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UTimecodeSynchronizer* Table)
{
	FEditorDelegates::OnAssetPostImport.AddRaw(this, &FTimecodeSynchronizerEditorToolkit::HandleAssetPostImport);

	const bool bIsUpdatable = false;
	const bool bAllowFavorites = true;
	const bool bIsLockable = false;
	const FDetailsViewArgs DetailsViewArgs(bIsUpdatable, bIsLockable, true, FDetailsViewArgs::ObjectsUseNameArea, false);
	DetailsView = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor").CreateDetailView(DetailsViewArgs);
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout(TimecodeSynchronizerEditorToolkit::Layout)
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->SetHideTabWell(true)
				->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.5f)
				->Split
				(
					// Source display
					FTabManager::NewStack()
					->SetHideTabWell(true)
					->AddTab(TimecodeSynchronizerEditorToolkit::SourceViewerTabId, ETabState::OpenedTab)
				)
			)
			->Split
			(
				FTabManager::NewSplitter()
				->SetSizeCoefficient(0.4f)
				->Split
				(
					FTabManager::NewStack()
					->AddTab(TimecodeSynchronizerEditorToolkit::PropertiesTabId, ETabState::OpenedTab)
				)
			)
		);

	BindCommands();

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	Super::InitAssetEditor(Mode, InitToolkitHost, TimecodeSynchronizerEditorToolkit::AppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, Table);

	ExtendToolBar();

	// Get the list of objects to edit the details of
	TArray<UObject*> ObjectsToEditInDetailsView;
	ObjectsToEditInDetailsView.Add(Table);

	// Ensure all objects are transactable for undo/redo in the details panel
	for (UObject* ObjectToEditInDetailsView : ObjectsToEditInDetailsView)
	{
		ObjectToEditInDetailsView->SetFlags(RF_Transactional);
	}

	if (DetailsView.IsValid())
	{
		// Make sure details window is pointing to our object
		DetailsView->SetObjects(ObjectsToEditInDetailsView);
	}
}

FTimecodeSynchronizerEditorToolkit::~FTimecodeSynchronizerEditorToolkit()
{
	FEditorDelegates::OnAssetPostImport.RemoveAll(this);
}

FName FTimecodeSynchronizerEditorToolkit::GetToolkitFName() const
{
	return TimecodeSynchronizerEditorToolkit::AppIdentifier;
}

FText FTimecodeSynchronizerEditorToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Timecode Synchronizer Editor");
}

FText FTimecodeSynchronizerEditorToolkit::GetToolkitName() const
{
	const UObject* EditingObject = GetEditingObject();

	const bool bDirtyState = EditingObject->GetOutermost()->IsDirty();

	FFormatNamedArguments Args;
	Args.Add(TEXT("ToolkitName"), GetBaseToolkitName());
	Args.Add(TEXT("ObjectName"), FText::FromString(EditingObject->GetName()));
	Args.Add(TEXT("DirtyState"), bDirtyState ? FText::FromString(TEXT("*")) : FText::GetEmpty());
	return FText::Format(LOCTEXT("ToolkitTitle", "{ObjectName}{DirtyState} - {ToolkitName}"), Args);
}

FString FTimecodeSynchronizerEditorToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "TimecodeSynchronizer ").ToString();
}

FLinearColor FTimecodeSynchronizerEditorToolkit::GetWorldCentricTabColorScale() const
{
	return FLinearColor(0.0f, 0.0f, 0.2f, 0.5f);
}

void FTimecodeSynchronizerEditorToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_TimecodeSynchronizerEditor", "Timecode Synchronizer Editor"));

	Super::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(TimecodeSynchronizerEditorToolkit::PropertiesTabId, FOnSpawnTab::CreateSP(this, &FTimecodeSynchronizerEditorToolkit::SpawnPropertiesTab))
		.SetDisplayName(LOCTEXT("PropertiesTab", "Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));
	
	InTabManager->RegisterTabSpawner(TimecodeSynchronizerEditorToolkit::SourceViewerTabId, FOnSpawnTab::CreateSP(this, &FTimecodeSynchronizerEditorToolkit::SpawnSourceViewerTab))
		.SetDisplayName(LOCTEXT("SourceViewerTab", "Sources"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewer"));
}

void FTimecodeSynchronizerEditorToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	Super::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(TimecodeSynchronizerEditorToolkit::PropertiesTabId);
	InTabManager->UnregisterTabSpawner(TimecodeSynchronizerEditorToolkit::SourceViewerTabId);
}

UTimecodeSynchronizer* FTimecodeSynchronizerEditorToolkit::GetTimecodeSynchronizer() const
{
	return Cast<UTimecodeSynchronizer>(GetEditingObject());
}

TSharedRef<SDockTab> FTimecodeSynchronizerEditorToolkit::SpawnPropertiesTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == TimecodeSynchronizerEditorToolkit::PropertiesTabId);

	return SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("GenericEditor.Tabs.Properties"))
		.Label(LOCTEXT("GenericDetailsTitle", "Details"))
		.TabColorScale(GetTabColorScale())
		[
			DetailsView.ToSharedRef()
		];
}

TSharedRef<SDockTab> FTimecodeSynchronizerEditorToolkit::SpawnSourceViewerTab(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == TimecodeSynchronizerEditorToolkit::SourceViewerTabId);

	TSharedPtr<SWidget> TabWidget = SNew(STimecodeSynchronizerSourceViewer, *GetTimecodeSynchronizer());

	return SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("GenericEditor.Tabs.Viewer"))
		.Label(LOCTEXT("GenericSourceViewerTitle", "Sources"))
		.TabColorScale(GetTabColorScale())
		[
			TabWidget.ToSharedRef()
		];
}

void FTimecodeSynchronizerEditorToolkit::HandleAssetPostImport(UFactory* InFactory, UObject* InObject)
{
	if (GetEditingObject() == InObject)
	{
		// The details panel likely needs to be refreshed if an asset was imported again
		TArray<UObject*> PostImportedEditingObjects;
		PostImportedEditingObjects.Add(InObject);
		DetailsView->SetObjects(PostImportedEditingObjects);
	}
}

void FTimecodeSynchronizerEditorToolkit::BindCommands()
{
	ToolkitCommands->MapAction(
		FTimecodeSynchronizerEditorCommand::Get().PreRollCommand,
		FExecuteAction::CreateLambda([&]()
		{
			if (UTimecodeSynchronizer* Asset = GetTimecodeSynchronizer())
			{
				Asset->StartPreRoll();
			}
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([&]()
		{
			if (UTimecodeSynchronizer* Asset = GetTimecodeSynchronizer())
			{
				ETimecodeProviderSynchronizationState State = Asset->GetSynchronizationState();
				return State == ETimecodeProviderSynchronizationState::Synchronized || State == ETimecodeProviderSynchronizationState::Synchronizing;
			}
			return false;
		}));
}

void FTimecodeSynchronizerEditorToolkit::ExtendToolBar()
{
	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);
	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateLambda([&](FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("TimecodeSynchronizer");
			{
				ToolbarBuilder.AddToolBarButton(
					FTimecodeSynchronizerEditorCommand::Get().PreRollCommand,
					NAME_None,
					TAttribute<FText>(),
					TAttribute<FText>(),
					FSlateIcon(FTimecodeSynchronizerEditorStyle::GetStyleSetName(), TEXT("Play"))
					);
			}
			ToolbarBuilder.EndSection();
		})
	);
	AddToolbarExtender(ToolbarExtender);

	RegenerateMenusAndToolbars();
}

#undef LOCTEXT_NAMESPACE
