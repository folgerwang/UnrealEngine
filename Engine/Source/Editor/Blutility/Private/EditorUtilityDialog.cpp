// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "EditorUtilityDialog.h"
#include "GlobalEditorUtilityBase.h"
#include "Modules/ModuleManager.h"
#include "EditorUtilityBlueprint.h"
#include "PropertyEditorModule.h"

#include "IDetailsView.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "GlobalBlutilityDialog"

const FName NAME_DetailsPanel(TEXT("GlobalBlutilityDialog_DetailsPanel"));
const FName NAME_GlobalBlutilityDialogAppIdentifier = FName(TEXT("GlobalBlutilityDialogApp"));

//////////////////////////////////////////////////////////////////////////
// FGlobalBlutilityDialog

void FGlobalEditorUtilityDialog::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	InTabManager->RegisterTabSpawner( NAME_DetailsPanel, FOnSpawnTab::CreateRaw( this, &FGlobalEditorUtilityDialog::SpawnTab_DetailsPanel ) );

}

void FGlobalEditorUtilityDialog::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	InTabManager->UnregisterTabSpawner( NAME_DetailsPanel );
}

TSharedRef<SDockTab> FGlobalEditorUtilityDialog::SpawnTab_DetailsPanel( const FSpawnTabArgs& SpawnTabArgs )
{
	const TSharedRef<SDockTab> SpawnedTab = SNew(SDockTab)
		//@TODO: Add an icon .Icon( FEditorStyle::GetBrush("SoundClassEditor.Tabs.Properties") )
		.OnCanCloseTab_Lambda([](){ return false; })
		.Label( LOCTEXT("GlobalBlutilityDetailsTitle", "Blutility Details") )
		[
			DetailsView.ToSharedRef()
		];

	// Make sure the blutility instance is selected
	TArray<UObject*> SelectedObjects;
	SelectedObjects.Add(BlutilityInstance.Get());
	UpdatePropertyWindow(SelectedObjects);

	return SpawnedTab;
}

void FGlobalEditorUtilityDialog::InitBlutilityDialog(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit)
{
	// Create an instance of the blutility
	check(ObjectToEdit != NULL);
	UEditorUtilityBlueprint* BlutilityBP = CastChecked<UEditorUtilityBlueprint>(ObjectToEdit);
	check(BlutilityBP->GeneratedClass->IsChildOf(UGlobalEditorUtilityBase::StaticClass()));

	UGlobalEditorUtilityBase* Instance = NewObject<UGlobalEditorUtilityBase>(GetTransientPackage(), BlutilityBP->GeneratedClass);
	Instance->AddToRoot();
	BlutilityInstance = Instance;

	//
	CreateInternalWidgets();

	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_GlobalBlutility_Layout" ) 
	->AddArea
	(
		FTabManager::NewPrimaryArea()
		->Split
		(
			FTabManager::NewStack()
			->AddTab( NAME_DetailsPanel, ETabState::OpenedTab)
		)
	);

	const bool bCreateDefaultStandaloneMenu = false;
	const bool bCreateDefaultToolbar = false;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, NAME_GlobalBlutilityDialogAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectToEdit );

}

FGlobalEditorUtilityDialog::~FGlobalEditorUtilityDialog()
{
	if (UGlobalEditorUtilityBase* Instance = BlutilityInstance.Get())
	{
		Instance->RemoveFromRoot();
	}

	DetailsView.Reset();
}

void FGlobalEditorUtilityDialog::AddReferencedObjects(FReferenceCollector& Collector)
{
	if (UGlobalEditorUtilityBase* Instance = BlutilityInstance.Get())
	{
		Collector.AddReferencedObject(Instance);
	}
}

FName FGlobalEditorUtilityDialog::GetToolkitFName() const
{
	return FName("Blutility");
}

FText FGlobalEditorUtilityDialog::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "Blutility" );
}

FString FGlobalEditorUtilityDialog::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Blutility ").ToString();
}

FLinearColor FGlobalEditorUtilityDialog::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.3f, 0.2f, 0.5f, 0.5f );
}

void FGlobalEditorUtilityDialog::CreateInternalWidgets()
{
	// Create a details view
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
	FDetailsViewArgs DetailsViewArgs(/*bUpdateFromSelection=*/ false, /*bLockable=*/ false, /*bAllowSearch=*/ false, FDetailsViewArgs::HideNameArea, /*bHideSelectionTip=*/ true);
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);
}

void FGlobalEditorUtilityDialog::UpdatePropertyWindow(const TArray<UObject*>& SelectedObjects)
{
	DetailsView->SetObjects(SelectedObjects);
}

#undef LOCTEXT_NAMESPACE
