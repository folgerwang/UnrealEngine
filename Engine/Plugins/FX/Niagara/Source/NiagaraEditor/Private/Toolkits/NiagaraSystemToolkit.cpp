// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraSystemToolkit.h"
#include "NiagaraEditorModule.h"
#include "NiagaraSystem.h"
#include "NiagaraEmitter.h"
#include "NiagaraObjectSelection.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "NiagaraSystemScriptViewModel.h"
#include "Widgets/SNiagaraCurveEditor.h"
#include "Widgets/SNiagaraSystemScript.h"
#include "Widgets/SNiagaraSystemViewport.h"
#include "Widgets/SNiagaraSelectedObjectsDetails.h"
#include "Widgets/SNiagaraParameterMapView.h"
#include "Widgets/SNiagaraSelectedEmitterHandles.h"
#include "Widgets/SNiagaraSpreadsheetView.h"
#include "Widgets/SNiagaraGeneratedCodeView.h"
#include "Widgets/SNiagaraScriptGraph.h"
#include "NiagaraEditorCommands.h"
#include "NiagaraEditorStyle.h"
#include "NiagaraEditorSettings.h"
#include "NiagaraSystemFactoryNew.h"
#include "NiagaraEmitter.h"
#include "NiagaraComponent.h"

#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"

#include "EditorStyleSet.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "Framework/Docking/WorkspaceItem.h"
#include "ScopedTransaction.h"

#include "Framework/Application/SlateApplication.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Docking/SDockTab.h"
#include "AdvancedPreviewSceneModule.h"
#include "BusyCursor.h"
#include "Misc/FeedbackContext.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "AssetRegistryModule.h"
#include "IAssetRegistry.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"

#define LOCTEXT_NAMESPACE "NiagaraSystemEditor"

DECLARE_CYCLE_STAT(TEXT("Niagara - SystemToolkit - OnApply"), STAT_NiagaraEditor_SystemToolkit_OnApply, STATGROUP_NiagaraEditor);

const FName FNiagaraSystemToolkit::ViewportTabID(TEXT("NiagaraSystemEditor_Viewport"));
const FName FNiagaraSystemToolkit::CurveEditorTabID(TEXT("NiagaraSystemEditor_CurveEditor"));
const FName FNiagaraSystemToolkit::SequencerTabID(TEXT("NiagaraSystemEditor_Sequencer"));
const FName FNiagaraSystemToolkit::SystemScriptTabID(TEXT("NiagaraSystemEditor_SystemScript"));
const FName FNiagaraSystemToolkit::SystemDetailsTabID(TEXT("NiagaraSystemEditor_SystemDetails"));
const FName FNiagaraSystemToolkit::SystemParametersTabID(TEXT("NiagaraSystemEditor_SystemParameters"));
const FName FNiagaraSystemToolkit::SelectedEmitterStackTabID(TEXT("NiagaraSystemEditor_SelectedEmitterStack"));
const FName FNiagaraSystemToolkit::SelectedEmitterGraphTabID(TEXT("NiagaraSystemEditor_SelectedEmitterGraph"));
const FName FNiagaraSystemToolkit::DebugSpreadsheetTabID(TEXT("NiagaraSystemEditor_DebugAttributeSpreadsheet"));
const FName FNiagaraSystemToolkit::PreviewSettingsTabId(TEXT("NiagaraSystemEditor_PreviewSettings"));
const FName FNiagaraSystemToolkit::GeneratedCodeTabID(TEXT("NiagaraSystemEditor_GeneratedCode"));

static int32 GbLogNiagaraSystemChanges = 0;
static FAutoConsoleVariableRef CVarSuppressNiagaraSystems(
	TEXT("fx.LogNiagaraSystemChanges"),
	GbLogNiagaraSystemChanges,
	TEXT("If > 0 Niagara Systems will be written to a text format when opened and closed in the editor. \n"),
	ECVF_Default
);

static int32 GbShowNiagaraDeveloperWindows = 0;
static FAutoConsoleVariableRef CVarShowNiagaraDeveloperWindows(
	TEXT("fx.ShowNiagaraDeveloperWindows"),
	GbShowNiagaraDeveloperWindows,
	TEXT("If > 0 the niagara system and emitter editors will show additional developer windows.\nThese windows are for niagara tool development and debugging and editing the data\n directly in these windows can cause instability.\n"),
	ECVF_Default
);

void FNiagaraSystemToolkit::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_NiagaraSystemEditor", "Niagara System"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner(ViewportTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_Viewport))
		.SetDisplayName(LOCTEXT("Preview", "Preview"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Viewports"));

	InTabManager->RegisterTabSpawner(CurveEditorTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_CurveEd))
		.SetDisplayName(LOCTEXT("Curves", "Curves"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(SequencerTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_Sequencer))
		.SetDisplayName(LOCTEXT("Timeline", "Timeline"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(SystemScriptTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_SystemScript))
		.SetDisplayName(LOCTEXT("SystemScript", "System Script"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetAutoGenerateMenuEntry(GbShowNiagaraDeveloperWindows != 0);

	InTabManager->RegisterTabSpawner(SystemDetailsTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_SystemDetails))
		.SetDisplayName(LOCTEXT("SystemDetails", "System Details"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(SystemParametersTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_SystemParameters))
		.SetDisplayName(LOCTEXT("SystemParameters", "Parameters"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(SelectedEmitterStackTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_SelectedEmitterStack))
		.SetDisplayName(LOCTEXT("SelectedEmitterStacks", "Selected Emitters"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(SelectedEmitterGraphTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_SelectedEmitterGraph))
		.SetDisplayName(LOCTEXT("SelectedEmitterGraph", "Selected Emitter Graph"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetAutoGenerateMenuEntry(GbShowNiagaraDeveloperWindows != 0);

	InTabManager->RegisterTabSpawner(DebugSpreadsheetTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_DebugSpreadsheet))
		.SetDisplayName(LOCTEXT("DebugSpreadsheet", "Attribute Spreadsheet"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());

	InTabManager->RegisterTabSpawner(PreviewSettingsTabId, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_PreviewSettings))
		.SetDisplayName(LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Tabs.Details"));

	InTabManager->RegisterTabSpawner(GeneratedCodeTabID, FOnSpawnTab::CreateSP(this, &FNiagaraSystemToolkit::SpawnTab_GeneratedCode))
		.SetDisplayName(LOCTEXT("GeneratedCode", "Generated Code"))
		.SetGroup(WorkspaceMenuCategory.ToSharedRef());
}

void FNiagaraSystemToolkit::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner(ViewportTabID);
	InTabManager->UnregisterTabSpawner(CurveEditorTabID);
	InTabManager->UnregisterTabSpawner(SequencerTabID);
	InTabManager->UnregisterTabSpawner(SystemScriptTabID);
	InTabManager->UnregisterTabSpawner(SystemDetailsTabID);
	InTabManager->UnregisterTabSpawner(SystemParametersTabID);
	InTabManager->UnregisterTabSpawner(SelectedEmitterStackTabID);
	InTabManager->UnregisterTabSpawner(SelectedEmitterGraphTabID);
	InTabManager->UnregisterTabSpawner(DebugSpreadsheetTabID);
	InTabManager->UnregisterTabSpawner(PreviewSettingsTabId);
	InTabManager->UnregisterTabSpawner(GeneratedCodeTabID);
}


FNiagaraSystemToolkit::~FNiagaraSystemToolkit()
{
	if (SystemViewModel.IsValid())
	{
		SystemViewModel->Cleanup();
		SystemViewModel->GetOnPinnedCurvesChanged().RemoveAll(this);
	}
	SystemViewModel.Reset();
}

void FNiagaraSystemToolkit::AddReferencedObjects(FReferenceCollector& Collector) 
{
	Collector.AddReferencedObject(System);
}



void FNiagaraSystemToolkit::InitializeWithSystem(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraSystem& InSystem)
{
	System = &InSystem;
	Emitter = nullptr;

	FNiagaraSystemViewModelOptions SystemOptions;
	SystemOptions.bCanModifyEmittersFromTimeline = true;
	SystemOptions.EditMode = ENiagaraSystemViewModelEditMode::SystemAsset;
	SystemOptions.OnGetSequencerAddMenuContent.BindSP(this, &FNiagaraSystemToolkit::GetSequencerAddMenuContent);

	SystemViewModel = MakeShareable(new FNiagaraSystemViewModel(*System, SystemOptions));
	SystemViewModel->SetToolkitCommands(GetToolkitCommands());
	SystemToolkitMode = ESystemToolkitMode::System;

	if (GbLogNiagaraSystemChanges > 0)
	{
		FString ExportText;
		SystemViewModel->DumpToText(ExportText);
		FString FilePath = System->GetOutermost()->FileName.ToString();
		FString PathPart, FilenamePart, ExtensionPart;
		FPaths::Split(FilePath, PathPart, FilenamePart, ExtensionPart);

		FNiagaraEditorUtilities::WriteTextFileToDisk(FPaths::ProjectLogDir(), FilenamePart + TEXT(".onLoad.txt"), ExportText, true);
	}

	InitializeInternal(Mode, InitToolkitHost);
}

void FNiagaraSystemToolkit::InitializeWithEmitter(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UNiagaraEmitter& InEmitter)
{
	System = NewObject<UNiagaraSystem>(GetTransientPackage(), NAME_None, RF_Transient | RF_Transactional);
	UNiagaraSystemFactoryNew::InitializeSystem(System, true);

	Emitter = &InEmitter;

	// Before copying the emitter prepare the rapid iteration parameters so that the post compile prepare doesn't
	// cause the change ids to become out of sync.
	FString EmitterName = "Emitter";
	TArray<UNiagaraScript*> Scripts;
	TMap<UNiagaraScript*, UNiagaraScript*> ScriptDependencyMap;
	TMap<UNiagaraScript*, FString> ScriptToEmitterNameMap;

	Scripts.Add(Emitter->EmitterSpawnScriptProps.Script);
	ScriptToEmitterNameMap.Add(Emitter->EmitterSpawnScriptProps.Script, EmitterName);

	Scripts.Add(Emitter->EmitterUpdateScriptProps.Script);
	ScriptToEmitterNameMap.Add(Emitter->EmitterUpdateScriptProps.Script, EmitterName);

	Scripts.Add(Emitter->SpawnScriptProps.Script);
	ScriptToEmitterNameMap.Add(Emitter->SpawnScriptProps.Script, EmitterName);

	Scripts.Add(Emitter->UpdateScriptProps.Script);
	ScriptToEmitterNameMap.Add(Emitter->UpdateScriptProps.Script, EmitterName);

	if (Emitter->SimTarget == ENiagaraSimTarget::GPUComputeSim)
	{
		Scripts.Add(Emitter->GetGPUComputeScript());
		ScriptToEmitterNameMap.Add(Emitter->GetGPUComputeScript(), EmitterName);
		ScriptDependencyMap.Add(Emitter->SpawnScriptProps.Script, Emitter->GetGPUComputeScript());
		ScriptDependencyMap.Add(Emitter->UpdateScriptProps.Script, Emitter->GetGPUComputeScript());
	}
	else if (Emitter->bInterpolatedSpawning)
	{
		ScriptDependencyMap.Add(Emitter->UpdateScriptProps.Script, Emitter->SpawnScriptProps.Script);
	}

	FNiagaraUtilities::PrepareRapidIterationParameters(Scripts, ScriptDependencyMap, ScriptToEmitterNameMap);

	ResetLoaders(GetTransientPackage()); // Make sure that we're not going to get invalid version number linkers into the package we are going into. 
	GetTransientPackage()->LinkerCustomVersion.Empty();

	UNiagaraEmitter* EditableEmitter = (UNiagaraEmitter*)StaticDuplicateObject(Emitter, GetTransientPackage(), NAME_None, ~RF_Standalone, UNiagaraEmitter::StaticClass());
	
	// We set this to the copy's change id here instead of the original emitter's change id because the copy's change id may have been
	// updated from the original as part of post load and we use this id to detect if the editable emitter has been changed.
	LastSyncedEmitterChangeId = EditableEmitter->GetChangeId();
	bEmitterThumbnailUpdated = false;

	FNiagaraSystemViewModelOptions SystemOptions;
	SystemOptions.bCanModifyEmittersFromTimeline = false;
	SystemOptions.EditMode = ENiagaraSystemViewModelEditMode::EmitterAsset;

	SystemViewModel = MakeShareable(new FNiagaraSystemViewModel(*System, SystemOptions));
	SystemViewModel->SetToolkitCommands(GetToolkitCommands());
	SystemViewModel->AddEmitter(*EditableEmitter);
	SystemViewModel->GetSystemScriptViewModel()->RebuildEmitterNodes();
	SystemToolkitMode = ESystemToolkitMode::Emitter;

	if (GbLogNiagaraSystemChanges > 0)
	{
		FString ExportText;
		SystemViewModel->DumpToText(ExportText);
		FString FilePath = Emitter->GetOutermost()->FileName.ToString();
		FString PathPart, FilenamePart, ExtensionPart;
		FPaths::Split(FilePath, PathPart, FilenamePart, ExtensionPart);

		FNiagaraEditorUtilities::WriteTextFileToDisk(FPaths::ProjectLogDir(), FilenamePart + TEXT(".onLoad.txt"), ExportText, true);
	}

	InitializeInternal(Mode, InitToolkitHost);
}

void FNiagaraSystemToolkit::InitializeInternal(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost)
{
	if (SystemViewModel->GetEmitterHandleViewModels().Num() > 0)
	{
		SystemViewModel->SetSelectedEmitterHandleById(SystemViewModel->GetEmitterHandleViewModels()[0]->GetId());
	}

	SystemViewModel->OnEmitterHandleViewModelsChanged().AddSP(this, &FNiagaraSystemToolkit::OnRefresh);
	SystemViewModel->OnSelectedEmitterHandlesChanged().AddSP(this, &FNiagaraSystemToolkit::OnRefresh);
	SystemViewModel->GetOnPinnedEmittersChanged().AddSP(this, &FNiagaraSystemToolkit::OnRefresh);
	SystemViewModel->GetOnPinnedCurvesChanged().AddSP(this, &FNiagaraSystemToolkit::OnPinnedCurvesChanged);

	const float InTime = -0.02f;
	const float OutTime = 3.2f;

	TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout("Standalone_Niagara_System_Layout_v17")
		->AddArea
		(
			FTabManager::NewPrimaryArea()->SetOrientation(Orient_Vertical)
			->Split
			(
				FTabManager::NewStack()
				->SetSizeCoefficient(0.1f)
				->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
				->SetHideTabWell(true)
			)
			->Split
			(
				FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
				->Split
				(
					FTabManager::NewSplitter()->SetOrientation(Orient_Vertical)
					->SetSizeCoefficient(.60f)
					->Split
					(
						FTabManager::NewSplitter()->SetOrientation(Orient_Horizontal)
						->SetSizeCoefficient(0.75f)
						->Split
						(
							FTabManager::NewStack()
							->SetSizeCoefficient(.80f)
							->AddTab(ViewportTabID, ETabState::OpenedTab)
						)
						->Split
						(
							FTabManager::NewStack()
							->SetSizeCoefficient(0.20f)
							->AddTab(SystemParametersTabID, ETabState::OpenedTab)
						)
					)
					->Split
					(
						FTabManager::NewStack()
						->SetSizeCoefficient(0.25f)
						->AddTab(CurveEditorTabID, ETabState::OpenedTab)
						->AddTab(SequencerTabID, ETabState::OpenedTab)
					)
				)
				->Split
				(
					FTabManager::NewStack()
					->SetSizeCoefficient(0.40f)
					->AddTab(SelectedEmitterStackTabID, ETabState::OpenedTab)
					->AddTab(SelectedEmitterGraphTabID, ETabState::ClosedTab)
					->AddTab(SystemScriptTabID, ETabState::ClosedTab)
					->AddTab(SystemDetailsTabID, ETabState::ClosedTab)
					->AddTab(DebugSpreadsheetTabID, ETabState::ClosedTab)
					->AddTab(PreviewSettingsTabId, ETabState::ClosedTab)
					->AddTab(GeneratedCodeTabID, ETabState::ClosedTab)
				)
			)
		);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	UObject* ToolkitObject = SystemToolkitMode == ESystemToolkitMode::System ? (UObject*)System : (UObject*)Emitter;
	FAssetEditorToolkit::InitAssetEditor(Mode, InitToolkitHost, FNiagaraEditorModule::NiagaraEditorAppIdentifier,
		StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ToolkitObject);
	
	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	AddMenuExtender(NiagaraEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	SetupCommands();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	bChangesDiscarded = false;
}

FName FNiagaraSystemToolkit::GetToolkitFName() const
{
	return FName("Niagara");
}

FText FNiagaraSystemToolkit::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Niagara");
}

FString FNiagaraSystemToolkit::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Niagara ").ToString();
}


FLinearColor FNiagaraSystemToolkit::GetWorldCentricTabColorScale() const
{
	return FNiagaraEditorModule::WorldCentricTabColorScale;
}


TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_Viewport(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == ViewportTabID);

	Viewport = SNew(SNiagaraSystemViewport)
		.OnThumbnailCaptured(this, &FNiagaraSystemToolkit::OnThumbnailCaptured);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			Viewport.ToSharedRef()
		];

	Viewport->SetPreviewComponent(SystemViewModel->GetPreviewComponent());
	Viewport->OnAddedToTab(SpawnedTab);

	return SpawnedTab;
}

TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_PreviewSettings(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId() == PreviewSettingsTabId);

	TSharedRef<SWidget> InWidget = SNullWidget::NullWidget;
	if (Viewport.IsValid())
	{
		FAdvancedPreviewSceneModule& AdvancedPreviewSceneModule = FModuleManager::LoadModuleChecked<FAdvancedPreviewSceneModule>("AdvancedPreviewScene");
		InWidget = AdvancedPreviewSceneModule.CreateAdvancedPreviewSceneSettingsWidget(Viewport->GetPreviewScene());
	}

	TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		.Icon(FEditorStyle::GetBrush("LevelEditor.Tabs.Details"))
		.Label(LOCTEXT("PreviewSceneSettingsTab", "Preview Scene Settings"))
		[
			InWidget
		];

	return SpawnedTab;
}


TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_CurveEd(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == CurveEditorTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraCurveEditor, SystemViewModel.ToSharedRef())
		];

	return SpawnedTab;
}


TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_Sequencer(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == SequencerTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SystemViewModel->GetSequencer()->GetSequencerWidget()
		];

	return SpawnedTab;
}


TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_SystemScript(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == SystemScriptTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraSystemScript, SystemViewModel.ToSharedRef())
		];

	return SpawnedTab;
}


TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_SystemDetails(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == SystemDetailsTabID);

	TSharedRef<FNiagaraObjectSelection> SystemSelection = MakeShareable(new FNiagaraObjectSelection());
	SystemSelection->SetSelectedObject(System);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraSelectedObjectsDetails, SystemSelection)
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_SystemParameters(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == SystemParametersTabID);

	TSharedRef<FNiagaraObjectSelection> ObjectSelection = MakeShareable(new FNiagaraObjectSelection());
	if (SystemToolkitMode == ESystemToolkitMode::Emitter)
	{
		TSharedPtr<FNiagaraEmitterViewModel> EditableEmitterViewModel = SystemViewModel->GetEmitterHandleViewModels()[0]->GetEmitterViewModel();
		UNiagaraEmitter* EditableEmitter = EditableEmitterViewModel->GetEmitter();
		ObjectSelection->SetSelectedObject(EditableEmitter);
	}
	else if (SystemToolkitMode == ESystemToolkitMode::System)
	{
		ObjectSelection->SetSelectedObject(System);
	}

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SAssignNew(ParameterMapView, SNiagaraParameterMapView, ObjectSelection, SNiagaraParameterMapView::EToolkitType::SYSTEM, GetToolkitCommands())
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_SelectedEmitterStack(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == SelectedEmitterStackTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraSelectedEmitterHandles, SystemViewModel.ToSharedRef())
		];

	return SpawnedTab;
}

class SNiagaraSelectedEmitterGraph : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraSelectedEmitterGraph)
	{}
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedRef<FNiagaraSystemViewModel> InSystemViewModel)
	{
		SystemViewModel = InSystemViewModel;
		SystemViewModel->OnSelectedEmitterHandlesChanged().AddRaw(this, &SNiagaraSelectedEmitterGraph::SelectedEmitterHandlesChanged);
		ChildSlot
		[
			SAssignNew(GraphWidgetContainer, SBox)
		];
		UpdateGraphWidget();
	}

	~SNiagaraSelectedEmitterGraph()
	{
		if (SystemViewModel.IsValid())
		{
			SystemViewModel->OnEmitterHandleViewModelsChanged().RemoveAll(this);
			SystemViewModel->OnSelectedEmitterHandlesChanged().RemoveAll(this);
			SystemViewModel->GetOnPinnedEmittersChanged().RemoveAll(this);
			SystemViewModel->OnSelectedEmitterHandlesChanged().RemoveAll(this);
		}
	}

private:
	void SelectedEmitterHandlesChanged()
	{
		UpdateGraphWidget();
	}

	void UpdateGraphWidget()
	{
		TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandles;
		SystemViewModel->GetSelectedEmitterHandles(SelectedEmitterHandles);
		if (SelectedEmitterHandles.Num() == 1)
		{
			GraphWidgetContainer->SetContent(SNew(SNiagaraScriptGraph, SelectedEmitterHandles[0]->GetEmitterViewModel()->GetSharedScriptViewModel()->GetGraphViewModel()));
		}
		else
		{
			GraphWidgetContainer->SetContent(SNullWidget::NullWidget);
		}
	}

private:
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel;
	TSharedPtr<SBox> GraphWidgetContainer;
};

TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_SelectedEmitterGraph(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == SelectedEmitterGraphTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraSelectedEmitterGraph, SystemViewModel.ToSharedRef())
		];

	return SpawnedTab;
}

TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_DebugSpreadsheet(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == DebugSpreadsheetTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraSpreadsheetView, SystemViewModel.ToSharedRef())
		];

	return SpawnedTab;
}


TSharedRef<SDockTab> FNiagaraSystemToolkit::SpawnTab_GeneratedCode(const FSpawnTabArgs& Args)
{
	check(Args.GetTabId().TabType == GeneratedCodeTabID);

	TSharedRef<SDockTab> SpawnedTab =
		SNew(SDockTab)
		[
			SNew(SNiagaraGeneratedCodeView, SystemViewModel.ToSharedRef())
		];

	return SpawnedTab;
}

void FNiagaraSystemToolkit::SetupCommands()
{
	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().Compile,
		FExecuteAction::CreateRaw(this, &FNiagaraSystemToolkit::CompileSystem, false));
	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().ResetSimulation,
		FExecuteAction::CreateRaw(this, &FNiagaraSystemToolkit::ResetSimulation));

	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().ToggleBounds,
		FExecuteAction::CreateSP(this, &FNiagaraSystemToolkit::OnToggleBounds),
		FCanExecuteAction(),
		FIsActionChecked::CreateSP(this, &FNiagaraSystemToolkit::IsToggleBoundsChecked));

	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().ToggleBounds_SetFixedBounds,
		FExecuteAction::CreateSP(this, &FNiagaraSystemToolkit::OnToggleBoundsSetFixedBounds));

	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().SaveThumbnailImage,
		FExecuteAction::CreateSP(this, &FNiagaraSystemToolkit::OnSaveThumbnailImage));

	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().Apply,
		FExecuteAction::CreateSP(this, &FNiagaraSystemToolkit::OnApply),
		FCanExecuteAction::CreateSP(this, &FNiagaraSystemToolkit::OnApplyEnabled));

	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().ToggleAutoPlay,
		FExecuteAction::CreateLambda([]()
		{
			UNiagaraEditorSettings* Settings = GetMutableDefault<UNiagaraEditorSettings>();
			Settings->SetAutoPlay(!Settings->GetAutoPlay());
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]() { return GetDefault<UNiagaraEditorSettings>()->GetAutoPlay(); }));

	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().ToggleResetSimulationOnChange,
		FExecuteAction::CreateLambda([]()
		{
			UNiagaraEditorSettings* Settings = GetMutableDefault<UNiagaraEditorSettings>();
			Settings->SetResetSimulationOnChange(!Settings->GetResetSimulationOnChange());
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]() { return GetDefault<UNiagaraEditorSettings>()->GetResetSimulationOnChange(); }));

	GetToolkitCommands()->MapAction(
		FNiagaraEditorCommands::Get().ToggleResimulateOnChangeWhilePaused,
		FExecuteAction::CreateLambda([]()
		{
			UNiagaraEditorSettings* Settings = GetMutableDefault<UNiagaraEditorSettings>();
			Settings->SetResimulateOnChangeWhilePaused(!Settings->GetResimulateOnChangeWhilePaused());
		}),
		FCanExecuteAction(),
		FIsActionChecked::CreateLambda([]() { return GetDefault<UNiagaraEditorSettings>()->GetResimulateOnChangeWhilePaused(); }));
}

void FNiagaraSystemToolkit::OnSaveThumbnailImage()
{
	if (Viewport.IsValid())
	{
		Viewport->CreateThumbnail(SystemToolkitMode == ESystemToolkitMode::System ? (UObject*)System : Emitter);
	}
}

void FNiagaraSystemToolkit::OnThumbnailCaptured(UTexture2D* Thumbnail)
{
	if (SystemToolkitMode == ESystemToolkitMode::System)
	{
		System->MarkPackageDirty();
		System->ThumbnailImage = Thumbnail;
	}
	else if (SystemToolkitMode == ESystemToolkitMode::Emitter) 
	{
		TSharedPtr<FNiagaraEmitterViewModel> EditableEmitterViewModel = SystemViewModel->GetEmitterHandleViewModels()[0]->GetEmitterViewModel();
		UNiagaraEmitter* EditableEmitter = EditableEmitterViewModel->GetEmitter();
		EditableEmitter->ThumbnailImage = Thumbnail;
		bEmitterThumbnailUpdated = true;
	}
}

void FNiagaraSystemToolkit::ResetSimulation()
{
	SystemViewModel->ResetSystem();
}

void FNiagaraSystemToolkit::ExtendToolbar()
{
	struct Local
	{
		static TSharedRef<SWidget> FillSimulationOptionsMenu(FNiagaraSystemToolkit* Toolkit)
		{
			FMenuBuilder MenuBuilder(true, Toolkit->GetToolkitCommands());
			MenuBuilder.AddMenuEntry(FNiagaraEditorCommands::Get().ToggleAutoPlay);
			MenuBuilder.AddMenuEntry(FNiagaraEditorCommands::Get().ToggleResetSimulationOnChange);
			MenuBuilder.AddMenuEntry(FNiagaraEditorCommands::Get().ToggleResimulateOnChangeWhilePaused);
			return MenuBuilder.MakeWidget();
		}

		static void FillToolbar(FToolBarBuilder& ToolbarBuilder, FNiagaraSystemToolkit* Toolkit)
		{
			if (Toolkit->Emitter != nullptr)
			{
				ToolbarBuilder.BeginSection("Apply");
				{
					ToolbarBuilder.AddToolBarButton(FNiagaraEditorCommands::Get().Apply,
						NAME_None, TAttribute<FText>(), TAttribute<FText>(),
						FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "NiagaraEditor.Apply"),
						FName(TEXT("ApplyNiagaraEmitter")));
				}
				ToolbarBuilder.EndSection();
			}
			ToolbarBuilder.BeginSection("Compile");
			{
				ToolbarBuilder.AddToolBarButton(FNiagaraEditorCommands::Get().Compile,
					NAME_None,
					TAttribute<FText>(),
					TAttribute<FText>(Toolkit, &FNiagaraSystemToolkit::GetCompileStatusTooltip),
					TAttribute<FSlateIcon>(Toolkit, &FNiagaraSystemToolkit::GetCompileStatusImage),
					FName(TEXT("CompileNiagaraSystem")));
				ToolbarBuilder.AddComboButton(
					FUIAction(),
					FOnGetContent::CreateRaw(Toolkit, &FNiagaraSystemToolkit::GenerateCompileMenuContent),
					LOCTEXT("BuildCombo_Label", "Auto-Compile Options"),
					LOCTEXT("BuildComboToolTip", "Auto-Compile options menu"),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "LevelEditor.Build"),
					true);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("NiagaraThumbnail");
			{
				ToolbarBuilder.AddToolBarButton(FNiagaraEditorCommands::Get().SaveThumbnailImage, NAME_None,
					LOCTEXT("GenerateThumbnail", "Thumbnail"),
					LOCTEXT("GenerateThumbnailTooltip","Generate a thumbnail image."),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "Cascade.SaveThumbnailImage"));
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("NiagaraPreviewOptions");
			{
				ToolbarBuilder.AddToolBarButton(FNiagaraEditorCommands::Get().ToggleBounds, NAME_None,
					LOCTEXT("ShowBounds", "Bounds"),
					LOCTEXT("ShowBoundsTooltip", "Show the bounds for the scene."),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "Cascade.ToggleBounds"));
				ToolbarBuilder.AddComboButton(
					FUIAction(),
					FOnGetContent::CreateRaw(Toolkit, &FNiagaraSystemToolkit::GenerateBoundsMenuContent, Toolkit->GetToolkitCommands()),
					LOCTEXT("BoundsMenuCombo_Label", "Bounds Options"),
					LOCTEXT("BoundsMenuCombo_ToolTip", "Bounds options"),
					FSlateIcon(FEditorStyle::GetStyleSetName(), "Cascade.ToggleBounds"),
					true
				);
			}
			ToolbarBuilder.EndSection();

			ToolbarBuilder.BeginSection("PlaybackOptions");
			{
				ToolbarBuilder.AddComboButton(
					FUIAction(),
					FOnGetContent::CreateStatic(Local::FillSimulationOptionsMenu, Toolkit),
					LOCTEXT("SimulationOptions", "Simulation"),
					LOCTEXT("SimulationOptionsTooltip", "Simulation options"),
					FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "NiagaraEditor.SimulationOptions")
				);
			}
			ToolbarBuilder.EndSection();
		}
	};

	TSharedPtr<FExtender> ToolbarExtender = MakeShareable(new FExtender);

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar, this)
		);

	AddToolbarExtender(ToolbarExtender);

	FNiagaraEditorModule& NiagaraEditorModule = FModuleManager::LoadModuleChecked<FNiagaraEditorModule>("NiagaraEditor");
	AddToolbarExtender(NiagaraEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

TSharedRef<SWidget> FNiagaraSystemToolkit::GenerateBoundsMenuContent(TSharedRef<FUICommandList> InCommandList)
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, InCommandList);

	MenuBuilder.AddMenuEntry(FNiagaraEditorCommands::Get().ToggleBounds_SetFixedBounds);

	return MenuBuilder.MakeWidget();
}

void FNiagaraSystemToolkit::GetSequencerAddMenuContent(FMenuBuilder& MenuBuilder, TSharedRef<ISequencer> Sequencer)
{
	MenuBuilder.AddSubMenu(
		LOCTEXT("EmittersLabel", "Emitters..."),
		LOCTEXT("EmittersToolTip", "Add an existing emitter..."),
		FNewMenuDelegate::CreateLambda([&](FMenuBuilder& InMenuBuilder)
		{
			InMenuBuilder.AddWidget(CreateAddEmitterMenuContent(), FText());
		}));
}

TSharedRef<SWidget> FNiagaraSystemToolkit::CreateAddEmitterMenuContent()
{
	FAssetPickerConfig AssetPickerConfig;
	{
		AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateSP(this, &FNiagaraSystemToolkit::EmitterAssetSelected);
		AssetPickerConfig.bAllowNullSelection = false;
		AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
		AssetPickerConfig.Filter.ClassNames.Add(UNiagaraEmitter::StaticClass()->GetFName());
	}

	FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	return SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.f)
		[
			ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig)
		];
}

TSharedRef<SWidget> FNiagaraSystemToolkit::GenerateCompileMenuContent()
{
	const bool bShouldCloseWindowAfterMenuSelection = true;
	FMenuBuilder MenuBuilder(bShouldCloseWindowAfterMenuSelection, nullptr);

	FUIAction Action(
		FExecuteAction::CreateStatic(&FNiagaraSystemToolkit::ToggleCompileEnabled),
		FCanExecuteAction(),
		FIsActionChecked::CreateStatic(&FNiagaraSystemToolkit::IsAutoCompileEnabled));

	FUIAction FullRebuildAction(
		FExecuteAction::CreateRaw(this, &FNiagaraSystemToolkit::CompileSystem, true));

	MenuBuilder.AddMenuEntry(LOCTEXT("FullRebuild", "Full Rebuild"),
		LOCTEXT("FullRebuildTooltip", "Triggers a full rebuild of this system, ignoring the change tracking."),
		FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "Niagara.CompileStatus.Unknown"),
		FullRebuildAction, NAME_None, EUserInterfaceActionType::Button);
	MenuBuilder.AddMenuEntry(LOCTEXT("AutoCompile", "Automatically compile when graph changes"),
		FText(), FSlateIcon(), Action, NAME_None, EUserInterfaceActionType::ToggleButton);

	return MenuBuilder.MakeWidget();
}

FSlateIcon FNiagaraSystemToolkit::GetCompileStatusImage() const
{
	ENiagaraScriptCompileStatus Status = SystemViewModel->GetLatestCompileStatus();

	switch (Status)
	{
	default:
	case ENiagaraScriptCompileStatus::NCS_Unknown:
	case ENiagaraScriptCompileStatus::NCS_Dirty:
		return FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "Niagara.CompileStatus.Unknown");
	case ENiagaraScriptCompileStatus::NCS_Error:
		return FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "Niagara.CompileStatus.Error");
	case ENiagaraScriptCompileStatus::NCS_UpToDate:
		return FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "Niagara.CompileStatus.Good");
	case ENiagaraScriptCompileStatus::NCS_UpToDateWithWarnings:
		return FSlateIcon(FNiagaraEditorStyle::GetStyleSetName(), "Niagara.CompileStatus.Warning");
	}
}

FText FNiagaraSystemToolkit::GetCompileStatusTooltip() const
{
	ENiagaraScriptCompileStatus Status = SystemViewModel->GetLatestCompileStatus();
	return FNiagaraEditorUtilities::StatusToText(Status);
}


void FNiagaraSystemToolkit::CompileSystem(bool bFullRebuild)
{
	SystemViewModel->CompileSystem(bFullRebuild);
}

TSharedPtr<FNiagaraSystemViewModel> FNiagaraSystemToolkit::GetSystemViewModel()
{
	return SystemViewModel;
}

void FNiagaraSystemToolkit::OnToggleBounds()
{
	ToggleDrawOption(SNiagaraSystemViewport::Bounds);
}

bool FNiagaraSystemToolkit::IsToggleBoundsChecked() const
{
	return IsDrawOptionEnabled(SNiagaraSystemViewport::Bounds);
}

void FNiagaraSystemToolkit::ToggleDrawOption(int32 Element)
{
	if (Viewport.IsValid() && Viewport->GetViewportClient().IsValid())
	{
		Viewport->ToggleDrawElement((SNiagaraSystemViewport::EDrawElements)Element);
		Viewport->RefreshViewport();
	}
}

bool FNiagaraSystemToolkit::IsDrawOptionEnabled(int32 Element) const
{
	if (Viewport.IsValid() && Viewport->GetViewportClient().IsValid())
	{
		return Viewport->GetDrawElement((SNiagaraSystemViewport::EDrawElements)Element);
	}
	else
	{
		return false;
	}
}

void FNiagaraSystemToolkit::OnToggleBoundsSetFixedBounds()
{
	FScopedTransaction Transaction(LOCTEXT("SetFixedBounds", "Set Fixed Bounds"));

	SystemViewModel->UpdateEmitterFixedBounds();

	/*
	// Force the component to update its bounds.
	ParticleSystemComponent->ForceUpdateBounds();

	// Grab the current bounds of the PSysComp & set it on the PSystem itself
	ParticleSystem->Modify();
	ParticleSystem->FixedRelativeBoundingBox.Min = ParticleSystemComponent->Bounds.GetBoxExtrema(0);
	ParticleSystem->FixedRelativeBoundingBox.Max = ParticleSystemComponent->Bounds.GetBoxExtrema(1);
	ParticleSystem->FixedRelativeBoundingBox.IsValid = true;
	ParticleSystem->bUseFixedRelativeBoundingBox = true;

	ParticleSystem->MarkPackageDirty();

	EndTransaction(Transaction);

	if ((SelectedModule == NULL) && (SelectedEmitter == NULL))
	{
		TArray<UObject*> NewSelection;
		NewSelection.Add(ParticleSystem);
		SetSelection(NewSelection);
	}

	ReassociateParticleSystem();
	*/
}

void FNiagaraSystemToolkit::UpdateOriginalEmitter()
{
	checkf(SystemToolkitMode == ESystemToolkitMode::Emitter, TEXT("There is no original emitter to update in system mode."));

	TSharedPtr<FNiagaraEmitterViewModel> EditableEmitterViewModel = SystemViewModel->GetEmitterHandleViewModels()[0]->GetEmitterViewModel();
	UNiagaraEmitter* EditableEmitter = EditableEmitterViewModel->GetEmitter();

	if (EditableEmitter->GetChangeId() != LastSyncedEmitterChangeId)
	{
		const FScopedBusyCursor BusyCursor;
		const FText LocalizedScriptEditorApply = NSLOCTEXT("UnrealEd", "ToolTip_NiagaraEmitterEditorApply", "Apply changes to original emitter and its use in the world.");
		GWarn->BeginSlowTask(LocalizedScriptEditorApply, true);
		GWarn->StatusUpdate(1, 1, LocalizedScriptEditorApply);

		if (Emitter->IsSelected())
		{
			GEditor->GetSelectedObjects()->Deselect(Emitter);
		}

		ResetLoaders(Emitter->GetOutermost()); // Make sure that we're not going to get invalid version number linkers into the package we are going into. 
		Emitter->GetOutermost()->LinkerCustomVersion.Empty();

		TArray<UNiagaraScript*> AllScripts;
		EditableEmitter->GetScripts(AllScripts, true);
		for (UNiagaraScript* Script : AllScripts)
		{
			checkSlow(Script->AreScriptAndSourceSynchronized());
		}
		checkSlow(EditableEmitter->AreAllScriptAndSourcesSynchronized());

		// overwrite the original script in place by constructing a new one with the same name
		Emitter = (UNiagaraEmitter*)StaticDuplicateObject(EditableEmitter, Emitter->GetOuter(),
			Emitter->GetFName(), RF_AllFlags, Emitter->GetClass());

		// Record the last synced change id to detect future changes.
		LastSyncedEmitterChangeId = EditableEmitter->GetChangeId();
		bEmitterThumbnailUpdated = false;

		checkSlow(UNiagaraEmitter::GetForceCompileOnLoad() || Emitter->GetChangeId() == EditableEmitter->GetChangeId());

		// Restore RF_Standalone on the original emitter, as it had been removed from the preview emitter so that it could be GC'd.
		Emitter->SetFlags(RF_Standalone);

		TArray<UNiagaraEmitter*> AffectedEmitters;
		AffectedEmitters.Add(Emitter);
		UpdateExistingEmitters();

		checkSlow(UNiagaraEmitter::GetForceCompileOnLoad() || Emitter->GetChangeId() == EditableEmitter->GetChangeId());

		GWarn->EndSlowTask();
	}
	else if(bEmitterThumbnailUpdated)
	{
		Emitter->MarkPackageDirty();
		Emitter->ThumbnailImage = (UTexture2D*)StaticDuplicateObject(EditableEmitter->ThumbnailImage, Emitter);
		bEmitterThumbnailUpdated = false;
	}
}

void FNiagaraSystemToolkit::UpdateExistingEmitters()
{
	for (TObjectIterator<UNiagaraSystem> SystemIterator; SystemIterator; ++SystemIterator)
	{
		UNiagaraSystem* LoadedSystem = *SystemIterator;
		if (LoadedSystem->IsPendingKill() == false && 
			LoadedSystem->HasAnyFlags(RF_ClassDefaultObject) == false &&
			LoadedSystem->ReferencesSourceEmitter(*Emitter))
		{
			LoadedSystem->UpdateFromEmitterChanges(*Emitter);
			TArray<TSharedPtr<FNiagaraSystemViewModel>> ReferencingSystemViewModels;
			FNiagaraSystemViewModel::GetAllViewModelsForObject(LoadedSystem, ReferencingSystemViewModels);

			for (TSharedPtr<FNiagaraSystemViewModel> ReferencingSystemViewModel : ReferencingSystemViewModels)
			{
				ReferencingSystemViewModel->RefreshAll();
			}

			if (ReferencingSystemViewModels.Num() == 0)
			{
				for (TObjectIterator<UNiagaraComponent> ComponentIterator; ComponentIterator; ++ComponentIterator)
				{
					UNiagaraComponent* Component = *ComponentIterator;
					if (Component->GetAsset() == LoadedSystem)
					{
						Component->ReinitializeSystem();
					}
				}
			}
		}
	}
}

void FNiagaraSystemToolkit::GetSaveableObjects(TArray<UObject*>& OutObjects) const
{
	if (SystemToolkitMode == ESystemToolkitMode::Emitter)
	{
		OutObjects.Add(Emitter);
	}
	else
	{
		FAssetEditorToolkit::GetSaveableObjects(OutObjects);
	}
}

void FNiagaraSystemToolkit::SaveAsset_Execute()
{
	if (SystemToolkitMode == ESystemToolkitMode::Emitter)
	{
		UE_LOG(LogNiagaraEditor, Log, TEXT("Saving and Compiling NiagaraEmitter %s"), *GetEditingObjects()[0]->GetName());
		UpdateOriginalEmitter();
	}
	SystemViewModel->OnPreSave();
	FAssetEditorToolkit::SaveAsset_Execute();
}

void FNiagaraSystemToolkit::SaveAssetAs_Execute()
{
	if (SystemToolkitMode == ESystemToolkitMode::Emitter)
	{
		UE_LOG(LogNiagaraEditor, Log, TEXT("Saving and Compiling NiagaraEmitter %s"), *GetEditingObjects()[0]->GetName());
		UpdateOriginalEmitter();
	}
	SystemViewModel->OnPreSave();
	FAssetEditorToolkit::SaveAssetAs_Execute();
}

bool FNiagaraSystemToolkit::OnRequestClose()
{
	if (GbLogNiagaraSystemChanges > 0)
	{
		FString ExportText;
		SystemViewModel->DumpToText(ExportText);
		FString FilePath;

		if (SystemToolkitMode == ESystemToolkitMode::System)
		{
			FilePath = System->GetOutermost()->FileName.ToString();
		}
		else if (SystemToolkitMode == ESystemToolkitMode::Emitter)
		{
			FilePath = Emitter->GetOutermost()->FileName.ToString();
		}

		FString PathPart, FilenamePart, ExtensionPart;
		FPaths::Split(FilePath, PathPart, FilenamePart, ExtensionPart);

		FNiagaraEditorUtilities::WriteTextFileToDisk(FPaths::ProjectLogDir(), FilenamePart + TEXT(".onClose.txt"), ExportText, true);
	}

	SystemViewModel->OnPreClose();

	if (SystemToolkitMode == ESystemToolkitMode::Emitter)
	{
		TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModel = SystemViewModel->GetEmitterHandleViewModels()[0]->GetEmitterViewModel();
		if (bChangesDiscarded == false && (EmitterViewModel->GetEmitter()->GetChangeId() != LastSyncedEmitterChangeId || bEmitterThumbnailUpdated))
		{
			// find out the user wants to do with this dirty NiagaraScript
			EAppReturnType::Type YesNoCancelReply = FMessageDialog::Open(EAppMsgType::YesNoCancel,
				FText::Format(
					NSLOCTEXT("UnrealEd", "Prompt_NiagaraEmitterEditorClose", "Would you like to apply changes to this Emitter to the original Emitter?\n{0}\n(No will lose all changes!)"),
					FText::FromString(Emitter->GetPathName())));

			// act on it
			switch (YesNoCancelReply)
			{
			case EAppReturnType::Yes:
				// update NiagaraScript and exit
				UpdateOriginalEmitter();
				break;
			case EAppReturnType::No:
				// Set the changes discarded to avoid showing the dialog multiple times when request close is called multiple times on shut down.
				bChangesDiscarded = true;
				break;
			case EAppReturnType::Cancel:
				// don't exit
				return false;
			}
		}
		return true;
	}
	
	return FAssetEditorToolkit::OnRequestClose();
}

void FNiagaraSystemToolkit::EmitterAssetSelected(const FAssetData& AssetData)
{
	FSlateApplication::Get().DismissAllMenus();
	SystemViewModel->AddEmitterFromAssetData(AssetData);
}

void FNiagaraSystemToolkit::ToggleCompileEnabled()
{
	UNiagaraEditorSettings* Settings = GetMutableDefault<UNiagaraEditorSettings>();
	Settings->SetAutoCompile(!Settings->GetAutoCompile());
}

bool FNiagaraSystemToolkit::IsAutoCompileEnabled()
{
	return GetDefault<UNiagaraEditorSettings>()->GetAutoCompile();
}

void FNiagaraSystemToolkit::OnApply()
{
	SCOPE_CYCLE_COUNTER(STAT_NiagaraEditor_SystemToolkit_OnApply);
	UpdateOriginalEmitter();
}

bool FNiagaraSystemToolkit::OnApplyEnabled() const
{
	if (Emitter != nullptr)
	{
		TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModel = SystemViewModel->GetEmitterHandleViewModels()[0]->GetEmitterViewModel();
		return EmitterViewModel->GetEmitter()->GetChangeId() != LastSyncedEmitterChangeId || bEmitterThumbnailUpdated;
	}
	return false;
}

void FNiagaraSystemToolkit::OnPinnedCurvesChanged()
{
	TabManager->InvokeTab(CurveEditorTabID);
}

void FNiagaraSystemToolkit::OnRefresh()
{
	if (ParameterMapView.IsValid())
	{
		TArray<TSharedPtr<FNiagaraEmitterHandleViewModel>> EmitterHandlesToDisplay;
		EmitterHandlesToDisplay.Append(SystemViewModel->GetPinnedEmitterHandles());
		TArray<TSharedRef<FNiagaraEmitterHandleViewModel>> SelectedEmitterHandles;
		SystemViewModel->GetSelectedEmitterHandles(SelectedEmitterHandles);
		for (auto Handle : SelectedEmitterHandles)
		{
			EmitterHandlesToDisplay.AddUnique(Handle);
		}

		ParameterMapView->RefreshEmitterHandles(EmitterHandlesToDisplay);
	}
}

#undef LOCTEXT_NAMESPACE
