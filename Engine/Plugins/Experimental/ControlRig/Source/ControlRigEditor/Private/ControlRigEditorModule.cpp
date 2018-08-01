// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigEditorModule.h"
#include "PropertyEditorModule.h"
#include "ControlRigVariableDetailsCustomization.h"
#include "BlueprintEditorModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "ControlRig.h"
#include "ControlRigComponent.h"
#include "ISequencerModule.h"
#include "ControlRigTrackEditor.h"
#include "IAssetTools.h"
#include "ControlRigSequenceActions.h"
#include "ControlRigEditorStyle.h"
#include "Framework/Docking/LayoutExtender.h"
#include "LevelEditor.h"
#include "MovieSceneToolsProjectSettings.h"
#include "PropertyEditorModule.h"
#include "Styling/SlateStyle.h"
#include "WorkflowOrientedApp/WorkflowTabManager.h"
#include "Modules/ModuleManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Sequencer/ControlRigSequence.h"
#include "EditorModeRegistry.h"
#include "ControlRigEditMode.h"
#include "ControlRigEditorObjectBinding.h"
#include "ControlRigEditorObjectSpawner.h"
#include "ILevelSequenceModule.h"
#include "ControlRigBindingTrackEditor.h"
#include "EditorModeManager.h"
#include "ControlRigEditMode.h"
#include "Sequencer/MovieSceneControlRigSection.h"
#include "MovieSceneControlRigSectionDetailsCustomization.h"
#include "ControlRigEditModeCommands.h"
#include "Materials/Material.h"
#include "Framework/MultiBox/MultiBoxExtender.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ControlRigEditModeSettings.h"
#include "Components/SkeletalMeshComponent.h"
#include "ControlRigSequenceExporter.h"
#include "ControlRigSequenceExporterSettingsDetailsCustomization.h"
#include "ControlRigSequenceExporterSettings.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "Editor/ControlRigEditor.h"
#include "ControlRigBlueprintActions.h"
#include "Graph/ControlRigGraphSchema.h"
#include "Graph/ControlRigGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "ControlRigGraphNode.h"
#include "EdGraphUtilities.h"
#include "ControlRigGraphPanelNodeFactory.h"
#include "ControlRigGraphPanelPinFactory.h"
#include "ControlRigBlueprintUtils.h"
#include "ControlRigBlueprintCommands.h"
#include "ControlRigHierarchyCommands.h"
#include "Animation/AnimSequence.h"
#include "ControlRigEditorEditMode.h"
#include "ControlRigDetails.h"
#include "Units/RigUnitEditor_TwoBoneIKFK.h"

#define LOCTEXT_NAMESPACE "ControlRigEditorModule"

DEFINE_LOG_CATEGORY(LogControlRigEditor);

TMap<FName, TSubclassOf<URigUnitEditor_Base>> FControlRigEditorModule::RigUnitEditorClasses;

void FControlRigEditorModule::StartupModule()
{
	FControlRigEditModeCommands::Register();
	FControlRigBlueprintCommands::Register();
	FControlRigHierarchyCommands::Register();
	FControlRigEditorStyle::Get();

	CommandBindings = MakeShareable(new FUICommandList());

	BindCommands();

	MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
	ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);

	// Register Blueprint editor variable customization
	FBlueprintEditorModule& BlueprintEditorModule = FModuleManager::LoadModuleChecked<FBlueprintEditorModule>("Kismet");
	BlueprintEditorModule.RegisterVariableCustomization(UProperty::StaticClass(), FOnGetVariableCustomizationInstance::CreateStatic(&FControlRigVariableDetailsCustomization::MakeInstance));

	// Register to fixup newly created BPs
	FKismetEditorUtilities::RegisterOnBlueprintCreatedCallback(this, UControlRig::StaticClass(), FKismetEditorUtilities::FOnBlueprintCreated::CreateRaw(this, &FControlRigEditorModule::HandleNewBlueprintCreated));

	FKismetCompilerContext::RegisterCompilerForBP(UControlRigBlueprint::StaticClass(), &FControlRigEditorModule::GetControlRigCompiler);

	// Register details customizations for animation controller nodes
	FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
	ClassesToUnregisterOnShutdown.Reset();

	ClassesToUnregisterOnShutdown.Add(UMovieSceneControlRigSection::StaticClass()->GetFName());
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown.Last(), FOnGetDetailCustomizationInstance::CreateStatic(&FMovieSceneControlRigSectionDetailsCustomization::MakeInstance));

	ClassesToUnregisterOnShutdown.Add(UControlRigSequenceExporterSettings::StaticClass()->GetFName());
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown.Last(), FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigSequenceExporterSettingsDetailsCustomization::MakeInstance));

	ClassesToUnregisterOnShutdown.Add(UControlRig::StaticClass()->GetFName());
	PropertyEditorModule.RegisterCustomClassLayout(ClassesToUnregisterOnShutdown.Last(), FOnGetDetailCustomizationInstance::CreateStatic(&FControlRigDetails::MakeInstance));

	// same as ClassesToUnregisterOnShutdown but for properties, there is none right now
	PropertiesToUnregisterOnShutdown.Reset();

	// Register blueprint compiler
	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.GetCompilers().Add(&ControlRigBlueprintCompiler);

	// Register asset tools
	auto RegisterAssetTypeAction = [this](const TSharedRef<IAssetTypeActions>& InAssetTypeAction)
	{
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		RegisteredAssetTypeActions.Add(InAssetTypeAction);
		AssetTools.RegisterAssetTypeActions(InAssetTypeAction);
	};

	RegisterAssetTypeAction(MakeShareable(new FControlRigSequenceActions()));
	RegisterAssetTypeAction(MakeShareable(new FControlRigBlueprintActions()));
	
	// Register sequencer track editor
	ISequencerModule& SequencerModule = FModuleManager::Get().LoadModuleChecked<ISequencerModule>("Sequencer");
	SequencerCreatedHandle = SequencerModule.RegisterOnSequencerCreated(FOnSequencerCreated::FDelegate::CreateRaw(this, &FControlRigEditorModule::HandleSequencerCreated));
	ControlRigTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FControlRigTrackEditor::CreateTrackEditor));
	ControlRigBindingTrackCreateEditorHandle = SequencerModule.RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(&FControlRigBindingTrackEditor::CreateTrackEditor));
	ControlRigEditorObjectBindingHandle = SequencerModule.RegisterEditorObjectBinding(FOnCreateEditorObjectBinding::CreateStatic(&FControlRigEditorObjectBinding::CreateEditorObjectBinding));

	SequencerToolbarExtender = MakeShareable(new FExtender());
	SequencerToolbarExtender->AddToolBarExtension(
		"Level Sequence Separator",
		EExtensionHook::Before,
		CommandBindings,
		FToolBarExtensionDelegate::CreateLambda([](FToolBarBuilder& ToolBarBuilder)
		{
			ToolBarBuilder.AddToolBarButton(FControlRigEditModeCommands::Get().ExportAnimSequence);
		}));

	SequencerModule.GetToolBarExtensibilityManager()->AddExtender(SequencerToolbarExtender);

	// Register for assets being opened
	AssetEditorOpenedHandle = FAssetEditorManager::Get().OnAssetEditorOpened().AddRaw(this, &FControlRigEditorModule::HandleAssetEditorOpened);

	// Register level sequence spawner
	ILevelSequenceModule& LevelSequenceModule = FModuleManager::LoadModuleChecked<ILevelSequenceModule>("LevelSequence");
	LevelSequenceSpawnerDelegateHandle = LevelSequenceModule.RegisterObjectSpawner(FOnCreateMovieSceneObjectSpawner::CreateStatic(&FControlRigEditorObjectSpawner::CreateObjectSpawner));

	TrajectoryMaterial = LoadObject<UMaterial>(nullptr, TEXT("/ControlRig/M_Traj.M_Traj"));
	if (TrajectoryMaterial.IsValid())
	{
		TrajectoryMaterial->AddToRoot();
	}

	FEditorModeRegistry::Get().RegisterMode<FControlRigEditMode>(
		FControlRigEditMode::ModeName,
		NSLOCTEXT("AnimationModeToolkit", "DisplayName", "Animation"),
		FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRigEditMode", "ControlRigEditMode.Small"),
		true);

	FEditorModeRegistry::Get().RegisterMode<FControlRigEditorEditMode>(
		FControlRigEditorEditMode::ModeName,
		NSLOCTEXT("RiggingModeToolkit", "DisplayName", "Rigging"),
		FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRigEditMode", "ControlRigEditMode.Small"),
		false);

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
	ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Add(FContentBrowserMenuExtender_SelectedAssets::CreateLambda([this](const TArray<FAssetData>& SelectedAssets)
	{
		TSharedRef<FExtender> Extender = MakeShared<FExtender>();

		if (SelectedAssets.ContainsByPredicate([](const FAssetData& AssetData) { return AssetData.GetClass() == UAnimSequence::StaticClass(); }))
		{
			Extender->AddMenuExtension(
				"GetAssetActions",
				EExtensionHook::After,
				CommandBindings,
				FMenuExtensionDelegate::CreateLambda([this, SelectedAssets](FMenuBuilder& MenuBuilder)
				{
					const TSharedPtr<FUICommandInfo>& ImportFromRigSequence = FControlRigEditModeCommands::Get().ImportFromRigSequence;
					MenuBuilder.AddMenuEntry(
						ImportFromRigSequence->GetLabel(),
						ImportFromRigSequence->GetDescription(),
						ImportFromRigSequence->GetIcon(),
						FUIAction(FExecuteAction::CreateRaw(this, &FControlRigEditorModule::ImportFromRigSequence, SelectedAssets)));
				}));

			// only add this if we find a control rig sequence targeting this anim sequence in the asset registry
			FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
			
			bool bCanReimport = false;
			if (SelectedAssets.Num() > 0)
			{
				// It's faster to find all assets with this tag and then query them against the selection then it is to 
				// query the asset registry each time for a tag with a particular value
				const FName LastExportedToAnimationSequenceTagName = GET_MEMBER_NAME_CHECKED(UControlRigSequence, LastExportedToAnimationSequence);
				TArray<FAssetData> FoundAssets;
				{
					TArray<FName> Tags;
					Tags.Add(LastExportedToAnimationSequenceTagName);
					AssetRegistryModule.Get().GetAssetsByTags(Tags, FoundAssets);
				}

				if (FoundAssets.Num() > 0)
				{
					for(const FAssetData& AssetData : SelectedAssets)
					{
						const bool bFoundAsset = FoundAssets.ContainsByPredicate([&AssetData, LastExportedToAnimationSequenceTagName](const FAssetData& FoundAsset)
						{
							const FName TagValue = FoundAsset.GetTagValueRef<FName>(LastExportedToAnimationSequenceTagName);
							return TagValue == AssetData.ObjectPath;
						});

						if (bFoundAsset)
						{
							bCanReimport = true;
							break;
						}
					}
				}
			}

			if (bCanReimport)
			{
				Extender->AddMenuExtension(
					"GetAssetActions",
					EExtensionHook::After,
					CommandBindings,
					FMenuExtensionDelegate::CreateLambda([this, SelectedAssets](FMenuBuilder& MenuBuilder)
					{
						const TSharedPtr<FUICommandInfo>& ReImportFromRigSequence = FControlRigEditModeCommands::Get().ReImportFromRigSequence;
						MenuBuilder.AddMenuEntry(
							ReImportFromRigSequence->GetLabel(),
							ReImportFromRigSequence->GetDescription(),
							ReImportFromRigSequence->GetIcon(),
							FUIAction(FExecuteAction::CreateRaw(this, &FControlRigEditorModule::ReImportFromRigSequence, SelectedAssets)));
					}));
			}
		}
		else if (SelectedAssets.ContainsByPredicate([](const FAssetData& AssetData) { return AssetData.GetClass() == UControlRigSequence::StaticClass(); }))
		{
			Extender->AddMenuExtension(
				"CommonAssetActions",
				EExtensionHook::Before,
				CommandBindings,
				FMenuExtensionDelegate::CreateLambda([this, SelectedAssets](FMenuBuilder& MenuBuilder)
				{
					MenuBuilder.BeginSection("ControlRigActions", LOCTEXT("ControlRigActions", "Control Rig Sequence Actions"));
					{
						const TSharedPtr<FUICommandInfo>& ExportAnimSequence = FControlRigEditModeCommands::Get().ExportAnimSequence;
						MenuBuilder.AddMenuEntry(
							ExportAnimSequence->GetLabel(),
							ExportAnimSequence->GetDescription(),
							ExportAnimSequence->GetIcon(),
							FUIAction(FExecuteAction::CreateRaw(this, &FControlRigEditorModule::ExportToAnimSequence, SelectedAssets)));

						bool bCanReExport = false;
						for (const FAssetData& AssetData : SelectedAssets)
						{
							if(UControlRigSequence* ControlRigSequence = Cast<UControlRigSequence>(AssetData.GetAsset()))
							{
								if (ControlRigSequence->LastExportedToAnimationSequence.IsValid())
								{
									bCanReExport = true;
									break;
								}
							}
						}

						if (bCanReExport)
						{
							const TSharedPtr<FUICommandInfo>& ReExportAnimSequence = FControlRigEditModeCommands::Get().ReExportAnimSequence;
							MenuBuilder.AddMenuEntry(
								ReExportAnimSequence->GetLabel(),
								ReExportAnimSequence->GetDescription(),
								ReExportAnimSequence->GetIcon(),
								FUIAction(FExecuteAction::CreateRaw(this, &FControlRigEditorModule::ReExportToAnimSequence, SelectedAssets)));
						}
					}
					MenuBuilder.EndSection();
				}));
		}
	
		return Extender;
	}));
	ContentBrowserMenuExtenderHandle = ContentBrowserModule.GetAllAssetViewContextMenuExtenders().Last().GetHandle();

	ControlRigGraphPanelNodeFactory = MakeShared<FControlRigGraphPanelNodeFactory>();
	FEdGraphUtilities::RegisterVisualNodeFactory(ControlRigGraphPanelNodeFactory);

	ControlRigGraphPanelPinFactory = MakeShared<FControlRigGraphPanelPinFactory>();
	FEdGraphUtilities::RegisterVisualPinFactory(ControlRigGraphPanelPinFactory);

	ReconstructAllNodesDelegateHandle = FBlueprintEditorUtils::OnReconstructAllNodesEvent.AddStatic(&FControlRigBlueprintUtils::HandleReconstructAllNodes);
	RefreshAllNodesDelegateHandle = FBlueprintEditorUtils::OnRefreshAllNodesEvent.AddStatic(&FControlRigBlueprintUtils::HandleRefreshAllNodes);
	RenameVariableReferencesDelegateHandle = FBlueprintEditorUtils::OnRenameVariableReferencesEvent.AddStatic(&FControlRigBlueprintUtils::HandleRenameVariableReferencesEvent);

	// register rig unit base editor class
	RegisterRigUnitEditorClass("RigUnit_TwoBoneIKFK", URigUnitEditor_TwoBoneIKFK::StaticClass());
}

void FControlRigEditorModule::ShutdownModule()
{
	FBlueprintEditorUtils::OnRefreshAllNodesEvent.Remove(RefreshAllNodesDelegateHandle);
	FBlueprintEditorUtils::OnReconstructAllNodesEvent.Remove(ReconstructAllNodesDelegateHandle);
	FBlueprintEditorUtils::OnRenameVariableReferencesEvent.Remove(RenameVariableReferencesDelegateHandle);

	FEdGraphUtilities::UnregisterVisualPinFactory(ControlRigGraphPanelPinFactory);
	FEdGraphUtilities::UnregisterVisualNodeFactory(ControlRigGraphPanelNodeFactory);

	FContentBrowserModule* ContentBrowserModule = FModuleManager::GetModulePtr<FContentBrowserModule>(TEXT("ContentBrowser"));
	if (ContentBrowserModule)
	{
		ContentBrowserModule->GetAllAssetViewContextMenuExtenders().RemoveAll([=](const FContentBrowserMenuExtender_SelectedAssets& InDelegate) { return ContentBrowserMenuExtenderHandle == InDelegate.GetHandle(); });
	}

	if (TrajectoryMaterial.IsValid())
	{
		TrajectoryMaterial->RemoveFromRoot();
	}

	FAssetEditorManager::Get().OnAssetEditorOpened().Remove(AssetEditorOpenedHandle);

	FEditorModeRegistry::Get().UnregisterMode(FControlRigEditorEditMode::ModeName);
	FEditorModeRegistry::Get().UnregisterMode(FControlRigEditMode::ModeName);

	ILevelSequenceModule* LevelSequenceModule = FModuleManager::GetModulePtr<ILevelSequenceModule>("LevelSequence");
	if (LevelSequenceModule)
	{
		LevelSequenceModule->UnregisterObjectSpawner(LevelSequenceSpawnerDelegateHandle);
	}

	ISequencerModule* SequencerModule = FModuleManager::GetModulePtr<ISequencerModule>("Sequencer");
	if (SequencerModule)
	{
		SequencerModule->UnregisterOnSequencerCreated(SequencerCreatedHandle);
		SequencerModule->UnRegisterTrackEditor(ControlRigTrackCreateEditorHandle);
		SequencerModule->UnRegisterTrackEditor(ControlRigBindingTrackCreateEditorHandle);
		SequencerModule->UnRegisterEditorObjectBinding(ControlRigEditorObjectBindingHandle);

		SequencerModule->GetToolBarExtensibilityManager()->RemoveExtender(SequencerToolbarExtender);
		SequencerToolbarExtender = nullptr;
	}

	FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");
	if (AssetToolsModule)
	{
		for (TSharedRef<IAssetTypeActions> RegisteredAssetTypeAction : RegisteredAssetTypeActions)
		{
			AssetToolsModule->Get().UnregisterAssetTypeActions(RegisteredAssetTypeAction);
		}
	}

	FKismetEditorUtilities::UnregisterAutoBlueprintNodeCreation(this);

	FBlueprintEditorModule* BlueprintEditorModule = FModuleManager::GetModulePtr<FBlueprintEditorModule>("Kismet");
	if (BlueprintEditorModule)
	{
		BlueprintEditorModule->UnregisterVariableCustomization(UProperty::StaticClass());
	}

	FPropertyEditorModule* PropertyEditorModule = FModuleManager::GetModulePtr<FPropertyEditorModule>("PropertyEditor");
	if (PropertyEditorModule)
	{
		for (int32 Index = 0; Index < ClassesToUnregisterOnShutdown.Num(); ++Index)
		{
			PropertyEditorModule->UnregisterCustomClassLayout(ClassesToUnregisterOnShutdown[Index]);
		}

		for (int32 Index = 0; Index < PropertiesToUnregisterOnShutdown.Num(); ++Index)
		{
			PropertyEditorModule->UnregisterCustomPropertyTypeLayout(PropertiesToUnregisterOnShutdown[Index]);
		}
	}

	IKismetCompilerInterface* KismetCompilerModule = FModuleManager::GetModulePtr<IKismetCompilerInterface>("KismetCompiler");
	if (KismetCompilerModule)
	{
		KismetCompilerModule->GetCompilers().Remove(&ControlRigBlueprintCompiler);
	}

	CommandBindings = nullptr;
}

void FControlRigEditorModule::HandleNewBlueprintCreated(UBlueprint* InBlueprint)
{
	// add an initial graph for us to work in
	const UControlRigGraphSchema* ControlRigGraphSchema = GetDefault<UControlRigGraphSchema>();

	UEdGraph* ControlRigGraph = FBlueprintEditorUtils::CreateNewGraph(InBlueprint, ControlRigGraphSchema->GraphName_ControlRig, UControlRigGraph::StaticClass(), UControlRigGraphSchema::StaticClass());
	ControlRigGraph->bAllowDeletion = false;
	FBlueprintEditorUtils::AddUbergraphPage(InBlueprint, ControlRigGraph);
	InBlueprint->LastEditedDocuments.AddUnique(ControlRigGraph);
}

void FControlRigEditorModule::HandleSequencerCreated(TSharedRef<ISequencer> InSequencer)
{
	TWeakPtr<ISequencer> LocalSequencer = InSequencer;

	// Record the last sequencer we opened that was editing a control rig sequence
	UMovieSceneSequence* FocusedSequence = InSequencer->GetFocusedMovieSceneSequence();
	if (UControlRigSequence* FocusedControlRigSequence = ExactCast<UControlRigSequence>(FocusedSequence))
	{
		WeakSequencer = InSequencer;
	}

	// We want to be informed of sequence activations (subsequences or not)
	auto HandleActivateSequence = [this, LocalSequencer](FMovieSceneSequenceIDRef Ref)
	{
		if (LocalSequencer.IsValid())
		{
			TSharedRef<ISequencer> Sequencer = LocalSequencer.Pin().ToSharedRef();
			UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
			if (UControlRigSequence* ControlRigSequence = ExactCast<UControlRigSequence>(Sequence))
			{
				WeakSequencer = LocalSequencer;

				GLevelEditorModeTools().ActivateMode(FControlRigEditMode::ModeName);

				if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
				{
					ControlRigEditMode->SetSequencer(Sequencer);
				}
			}
			else
			{
				if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
				{
					ControlRigEditMode->SetSequencer(nullptr);
					ControlRigEditMode->SetObjects(TWeakObjectPtr<>(), FGuid());
				}
			}
		}
	};

	InSequencer->OnActivateSequence().AddLambda(HandleActivateSequence);

	// Call into activation callback to handle initial activation
	FMovieSceneSequenceID SequenceID = MovieSceneSequenceID::Root;
	HandleActivateSequence(SequenceID);

	InSequencer->GetSelectionChangedObjectGuids().AddLambda([LocalSequencer](TArray<FGuid> InObjectBindings)
	{
		if (LocalSequencer.IsValid())
		{
			TSharedRef<ISequencer> Sequencer = LocalSequencer.Pin().ToSharedRef();
			UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
			if (UControlRigSequence* ControlRigSequence = ExactCast<UControlRigSequence>(Sequence))
			{
				TWeakObjectPtr<> SelectedObject;
				FGuid ObjectBinding;
				if(InObjectBindings.Num() > 0)
				{
					ObjectBinding = InObjectBindings[0];
					TArrayView<TWeakObjectPtr<>> BoundObjects = Sequencer->FindBoundObjects(ObjectBinding, Sequencer->GetFocusedTemplateID());
					if(BoundObjects.Num() > 0)
					{
						SelectedObject = BoundObjects[0];
					}
				}

				if (SelectedObject.IsValid())
				{
					GLevelEditorModeTools().ActivateMode(FControlRigEditMode::ModeName);
					if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
					{
						ControlRigEditMode->SetObjects(SelectedObject, ObjectBinding);
					}
				}
			}
		}
	});

	InSequencer->OnMovieSceneDataChanged().AddLambda([LocalSequencer](EMovieSceneDataChangeType DataChangeType)
	{
		if (LocalSequencer.IsValid())
		{
			TSharedRef<ISequencer> Sequencer = LocalSequencer.Pin().ToSharedRef();
			UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
			if (UControlRigSequence* ControlRigSequence = ExactCast<UControlRigSequence>(Sequence))
			{
				if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
				{
					ControlRigEditMode->RefreshObjects();
					ControlRigEditMode->RefreshTrajectoryCache();
				}
			}
		}
	});

	InSequencer->GetSelectionChangedTracks().AddLambda([LocalSequencer](TArray<UMovieSceneTrack*> InTracks)
	{
		if (LocalSequencer.IsValid())
		{
			TSharedRef<ISequencer> Sequencer = LocalSequencer.Pin().ToSharedRef();
			UMovieSceneSequence* Sequence = Sequencer->GetFocusedMovieSceneSequence();
			if (UControlRigSequence* ControlRigSequence = ExactCast<UControlRigSequence>(Sequence))
			{
				TArray<FString> PropertyPaths;

				// Look for any property tracks that might drive our rig manipulators
				for (UMovieSceneTrack* Track : InTracks)
				{
					if (UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(Track))
					{
						PropertyPaths.Add(PropertyTrack->GetPropertyPath());
					}
				}

				if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
				{
					ControlRigEditMode->ClearControlSelection();
					ControlRigEditMode->SetControlSelection(PropertyPaths, true);
				}
			}
		}
	});

	InSequencer->OnPostSave().AddLambda([](ISequencer& InSequencerThatSaved)
	{
		UMovieSceneSequence* Sequence = InSequencerThatSaved.GetFocusedMovieSceneSequence();
		if (UControlRigSequence* ControlRigSequence = ExactCast<UControlRigSequence>(Sequence))
		{
			if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
			{
				ControlRigEditMode->ReBindToActor();
			}
		}
	});

	InSequencer->OnGetIsTrackVisible().BindRaw(this, &FControlRigEditorModule::IsTrackVisible);
}

void FControlRigEditorModule::HandleAssetEditorOpened(UObject* InAsset)
{
	if (UControlRigSequence* ControlRigSequence = ExactCast<UControlRigSequence>(InAsset))
	{
		GLevelEditorModeTools().ActivateMode(FControlRigEditMode::ModeName);

		if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
		{
			ControlRigEditMode->ReBindToActor();
		}
	}
}

void FControlRigEditorModule::OnInitializeSequence(UControlRigSequence* Sequence)
{
	auto* ProjectSettings = GetDefault<UMovieSceneToolsProjectSettings>();
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	
	FFrameNumber StartFrame = (ProjectSettings->DefaultStartTime * MovieScene->GetTickResolution()).RoundToFrame();
	int32        Duration   = (ProjectSettings->DefaultDuration  * MovieScene->GetTickResolution()).RoundToFrame().Value;

	MovieScene->SetPlaybackRange(StartFrame, Duration);
}

void FControlRigEditorModule::BindCommands()
{
	const FControlRigEditModeCommands& Commands = FControlRigEditModeCommands::Get();

	CommandBindings->MapAction(
		Commands.ExportAnimSequence,
		FExecuteAction::CreateRaw(this, &FControlRigEditorModule::ExportAnimSequenceFromSequencer),
		FCanExecuteAction(),
		FGetActionCheckState(), 
		FIsActionButtonVisible::CreateRaw(this, &FControlRigEditorModule::CanExportAnimSequenceFromSequencer));
}

bool FControlRigEditorModule::CanExportAnimSequenceFromSequencer() const
{
	if (WeakSequencer.IsValid())
	{
		TSharedRef<ISequencer> Sequencer = WeakSequencer.Pin().ToSharedRef();
		return ExactCast<UControlRigSequence>(Sequencer->GetFocusedMovieSceneSequence()) != nullptr;
	}

	return false;
}

void FControlRigEditorModule::ExportAnimSequenceFromSequencer()
{
	// if we have an active sequencer, get the sequence
	UControlRigSequence* ControlRigSequence = nullptr;
	if (WeakSequencer.IsValid())
	{
		TSharedRef<ISequencer> Sequencer = WeakSequencer.Pin().ToSharedRef();
		ControlRigSequence = ExactCast<UControlRigSequence>(Sequencer->GetFocusedMovieSceneSequence());
	}

	// If we are bound to an actor in the edit mode, auto pick skeletal mesh to use for binding
	USkeletalMesh* SkeletalMesh = nullptr;
	if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
	{
		TLazyObjectPtr<AActor> ActorPtr = ControlRigEditMode->GetSettings()->Actor;
		if (ActorPtr.IsValid())
		{
			if (USkeletalMeshComponent* SkeletalMeshComponent = ActorPtr->FindComponentByClass<USkeletalMeshComponent>())
			{
				SkeletalMesh = SkeletalMeshComponent->SkeletalMesh;
			}
		}
	}

	if (ControlRigSequence)
	{
		ControlRigSequenceConverter::Convert(ControlRigSequence, nullptr, SkeletalMesh);
	}
}

void FControlRigEditorModule::ExportToAnimSequence(TArray<FAssetData> InAssetData)
{
	for (const FAssetData& AssetData : InAssetData)
	{
		UControlRigSequence* ControlRigSequence = Cast<UControlRigSequence>(AssetData.GetAsset());
		if (ControlRigSequence)
		{
			ControlRigSequenceConverter::Convert(ControlRigSequence, nullptr, nullptr);
		}
	}
}

void FControlRigEditorModule::ReExportToAnimSequence(TArray<FAssetData> InAssetData)
{
	for (const FAssetData& AssetData : InAssetData)
	{
		UControlRigSequence* ControlRigSequence = Cast<UControlRigSequence>(AssetData.GetAsset());
		if (ControlRigSequence)
		{
			UAnimSequence* AnimSequence = ControlRigSequence->LastExportedToAnimationSequence.LoadSynchronous();
			USkeletalMesh* SkeletalMesh = ControlRigSequence->LastExportedUsingSkeletalMesh.LoadSynchronous();
			bool bShowDialog = AnimSequence == nullptr || SkeletalMesh == nullptr;

			ControlRigSequenceConverter::Convert(ControlRigSequence, AnimSequence, SkeletalMesh, bShowDialog);
		}
	}
}

void FControlRigEditorModule::ImportFromRigSequence(TArray<FAssetData> InAssetData)
{
	for (const FAssetData& AssetData : InAssetData)
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(AssetData.GetAsset());
		if (AnimSequence)
		{
			ControlRigSequenceConverter::Convert(nullptr, AnimSequence, nullptr);
		}
	}
}

void FControlRigEditorModule::ReImportFromRigSequence(TArray<FAssetData> InAssetData)
{
	for (const FAssetData& AssetData : InAssetData)
	{
		UAnimSequence* AnimSequence = Cast<UAnimSequence>(AssetData.GetAsset());
		USkeletalMesh* SkeletalMesh = nullptr;
		UControlRigSequence* ControlRigSequence = nullptr;

		FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");

		TMultiMap<FName, FString> TagsAndValues;
		TagsAndValues.Add(GET_MEMBER_NAME_CHECKED(UControlRigSequence, LastExportedToAnimationSequence), AssetData.ObjectPath.ToString());

		TArray<FAssetData> FoundAssets;
		AssetRegistryModule.Get().GetAssetsByTagValues(TagsAndValues, FoundAssets);

		if (FoundAssets.Num() > 0)
		{
			ControlRigSequence = Cast<UControlRigSequence>(FoundAssets[0].GetAsset());
			if (ControlRigSequence)
			{
				SkeletalMesh = ControlRigSequence->LastExportedUsingSkeletalMesh.LoadSynchronous();
			}
		}

		bool bShowDialog = ControlRigSequence == nullptr || AnimSequence == nullptr || SkeletalMesh == nullptr;

		ControlRigSequenceConverter::Convert(ControlRigSequence, AnimSequence, SkeletalMesh, bShowDialog);
	}
}

bool FControlRigEditorModule::IsTrackVisible(const UMovieSceneTrack* InTrack)
{
	if (FControlRigEditMode* ControlRigEditMode = static_cast<FControlRigEditMode*>(GLevelEditorModeTools().GetActiveMode(FControlRigEditMode::ModeName)))
	{		
		// If nothing selected, show all nodes
		if (ControlRigEditMode->GetNumSelectedControls() == 0)
		{
			return true;
		}

		return ControlRigEditMode->IsControlSelected(ControlRigEditMode->GetControlFromPropertyPath(InTrack->GetTrackName().ToString()));
	}
	return true;
}

TSharedRef<IControlRigEditor> FControlRigEditorModule::CreateControlRigEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, class UControlRigBlueprint* InBlueprint)
{
	TSharedRef< FControlRigEditor > NewControlRigEditor(new FControlRigEditor());
	NewControlRigEditor->InitControlRigEditor(Mode, InitToolkitHost, InBlueprint);
	return NewControlRigEditor;
}

TSharedPtr<FKismetCompilerContext> FControlRigEditorModule::GetControlRigCompiler(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions)
{
	return TSharedPtr<FKismetCompilerContext>(new FControlRigBlueprintCompilerContext(BP, InMessageLog, InCompileOptions, nullptr));
}

void FControlRigEditorModule::RegisterRigUnitEditorClass(FName RigUnitClassName, TSubclassOf<URigUnitEditor_Base> InClass)
{
	TSubclassOf<URigUnitEditor_Base>& Class = RigUnitEditorClasses.FindOrAdd(RigUnitClassName);
	Class = InClass;
}

void FControlRigEditorModule::UnregisterRigUnitEditorClass(FName RigUnitClassName)
{
	RigUnitEditorClasses.Remove(RigUnitClassName);
}

// It's CDO of the class, so we don't want the object to be writable or even if you write, it won't be per instance
TSubclassOf<URigUnitEditor_Base> FControlRigEditorModule::GetEditorObjectByRigUnit(const FName& RigUnitClassName) 
{
	const TSubclassOf<URigUnitEditor_Base>* Class = RigUnitEditorClasses.Find(RigUnitClassName);
	if (Class)
	{
		return *Class;
	}

	//if you don't find anything, just send out base one
	return URigUnitEditor_Base::StaticClass();
}

IMPLEMENT_MODULE(FControlRigEditorModule, ControlRigEditor)

#undef LOCTEXT_NAMESPACE
