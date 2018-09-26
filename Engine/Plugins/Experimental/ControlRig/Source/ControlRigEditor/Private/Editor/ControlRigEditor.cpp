// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ControlRigEditor.h"
#include "Modules/ModuleManager.h"
#include "ControlRigEditorModule.h"
#include "ControlRigBlueprint.h"
#include "SBlueprintEditorToolbar.h"
#include "ControlRigEditorMode.h"
#include "SKismetInspector.h"
#include "Framework/Commands/GenericCommands.h"
#include "Editor.h"
#include "ControlRigGraphNode.h"
#include "BlueprintActionDatabase.h"
#include "ControlRigBlueprintCommands.h"
#include "SControlRig.h"
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "IPersonaToolkit.h"
#include "PersonaModule.h"
#include "ControlRigEditorEditMode.h"
#include "AssetEditorModeManager.h"
#include "ControlRigBlueprintGeneratedClass.h"
#include "Sequencer/ControlRigSequencerAnimInstance.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "IPersonaPreviewScene.h"
#include "Animation/AnimData/BoneMaskFilter.h"
#include "ControlRig.h"
#include "ControlRigSkeletalMeshComponent.h"
#include "ControlRigSkeletalMeshBinding.h"
#include "ControlRigBlueprintUtils.h"
#include "IPersonaViewport.h"
#include "EditorViewportClient.h"
#include "AnimationEditorPreviewActor.h"
#include "Misc/MessageDialog.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ControlRigEditorStyle.h"
#include "EditorFontGlyphs.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#define LOCTEXT_NAMESPACE "ControlRigEditor"

const FName ControlRigEditorAppName(TEXT("ControlRigEditorApp"));

const FName FControlRigEditorModes::ControlRigEditorMode("Rigging");

namespace ControlRigEditorTabs
{
	const FName DetailsTab(TEXT("DetailsTab"));
// 	const FName ViewportTab(TEXT("Viewport"));
// 	const FName AdvancedPreviewTab(TEXT("AdvancedPreviewTab"));
};

FControlRigEditor::FControlRigEditor()
	: ControlRig(nullptr)
	, bSelecting(false)
{
}

FControlRigEditor::~FControlRigEditor()
{
}

UControlRigBlueprint* FControlRigEditor::GetControlRigBlueprint() const
{
	return Cast<UControlRigBlueprint>(GetBlueprintObj());
}

void FControlRigEditor::ExtendMenu()
{
	if(MenuExtender.IsValid())
	{
		RemoveMenuExtender(MenuExtender);
		MenuExtender.Reset();
	}

	MenuExtender = MakeShareable(new FExtender);

	AddMenuExtender(MenuExtender);

	// add extensible menu if exists
	FControlRigEditorModule& ControlRigEditorModule = FModuleManager::LoadModuleChecked<FControlRigEditorModule>("ControlRigEditor");
	AddMenuExtender(ControlRigEditorModule.GetMenuExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));
}

void FControlRigEditor::InitControlRigEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UControlRigBlueprint* InControlRigBlueprint)
{
	FPersonaModule& PersonaModule = FModuleManager::GetModuleChecked<FPersonaModule>("Persona");

	FPersonaToolkitArgs PersonaToolkitArgs;
	PersonaToolkitArgs.OnPreviewSceneCreated = FOnPreviewSceneCreated::FDelegate::CreateSP(this, &FControlRigEditor::HandlePreviewSceneCreated);
	PersonaToolkit = PersonaModule.CreatePersonaToolkit(InControlRigBlueprint, PersonaToolkitArgs);

	// Set a default preview mesh, if any
	PersonaToolkit->SetPreviewMesh(InControlRigBlueprint->GetPreviewMesh(), false);
	PersonaToolkit->GetPreviewScene()->RegisterOnPreviewMeshChanged(FOnPreviewMeshChanged::CreateSP(this, &FControlRigEditor::HandlePreviewMeshChanged));

	Toolbox = SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(0.f);

	if (!Toolbar.IsValid())
	{
		Toolbar = MakeShareable(new FBlueprintEditorToolbar(SharedThis(this)));
	}

	// Build up a list of objects being edited in this asset editor
	TArray<UObject*> ObjectsBeingEdited;
	ObjectsBeingEdited.Add(InControlRigBlueprint);

	// Initialize the asset editor and spawn tabs
	const TSharedRef<FTabManager::FLayout> DummyLayout = FTabManager::NewLayout("NullLayout")->AddArea(FTabManager::NewPrimaryArea());
	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	InitAssetEditor(Mode, InitToolkitHost, ControlRigEditorAppName, DummyLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, ObjectsBeingEdited);

	TArray<UBlueprint*> ControlRigBlueprints;
	ControlRigBlueprints.Add(InControlRigBlueprint);

	CommonInitialization(ControlRigBlueprints);

	BindCommands();

	AddApplicationMode(
		FControlRigEditorModes::ControlRigEditorMode,
		MakeShareable(new FControlRigEditorMode(SharedThis(this))));

	ExtendMenu();
	ExtendToolbar();
	RegenerateMenusAndToolbars();

	// Activate the initial mode (which will populate with a real layout)
	SetCurrentMode(FControlRigEditorModes::ControlRigEditorMode);

	// Activate our edit mode
//	GetAssetEditorModeManager()->SetToolkitHost(GetToolkitHost());
	GetAssetEditorModeManager()->SetDefaultMode(FControlRigEditorEditMode::ModeName);
	GetAssetEditorModeManager()->ActivateMode(FControlRigEditorEditMode::ModeName);
	GetEditMode().OnControlsSelected().AddSP(this, &FControlRigEditor::SetSelectedNodes);
	GetEditMode().OnGetJointTransform() = FOnGetJointTransform::CreateSP(this, &FControlRigEditor::GetJointTransform);
	GetEditMode().OnSetJointTransform() = FOnSetJointTransform::CreateSP(this, &FControlRigEditor::SetJointTransform);
	UpdateControlRig();

	// Post-layout initialization
	PostLayoutBlueprintEditorInitialization();
}

void FControlRigEditor::BindCommands()
{
	GetToolkitCommands()->MapAction(
		FControlRigBlueprintCommands::Get().ExecuteGraph,
		FExecuteAction::CreateSP(this, &FControlRigEditor::ToggleExecuteGraph), 
		FCanExecuteAction(), 
		FIsActionChecked::CreateSP(this, &FControlRigEditor::IsExecuteGraphOn));
}

void FControlRigEditor::ToggleExecuteGraph()
{
	if (ControlRig)
	{
		ControlRig->bExecutionOn = !ControlRig->bExecutionOn;
	}
}

bool FControlRigEditor::IsExecuteGraphOn() const
{
	return (ControlRig)? ControlRig->bExecutionOn : false;
}

void FControlRigEditor::ExtendToolbar()
{
	// If the ToolbarExtender is valid, remove it before rebuilding it
	if(ToolbarExtender.IsValid())
	{
		RemoveToolbarExtender(ToolbarExtender);
		ToolbarExtender.Reset();
	}

	ToolbarExtender = MakeShareable(new FExtender);

	AddToolbarExtender(ToolbarExtender);

	FControlRigEditorModule& ControlRigEditorModule = FModuleManager::LoadModuleChecked<FControlRigEditorModule>("ControlRigEditor");
	AddToolbarExtender(ControlRigEditorModule.GetToolBarExtensibilityManager()->GetAllExtenders(GetToolkitCommands(), GetEditingObjects()));

	TArray<IControlRigEditorModule::FControlRigEditorToolbarExtender> ToolbarExtenderDelegates = ControlRigEditorModule.GetAllControlRigEditorToolbarExtenders();

	for (auto& ToolbarExtenderDelegate : ToolbarExtenderDelegates)
	{
		if(ToolbarExtenderDelegate.IsBound())
		{
			AddToolbarExtender(ToolbarExtenderDelegate.Execute(GetToolkitCommands(), SharedThis(this)));
		}
	}

	struct Local
	{
		static void FillToolbar(FToolBarBuilder& ToolbarBuilder)
		{
			ToolbarBuilder.BeginSection("Toolbar");
			{
				ToolbarBuilder.AddToolBarButton(FControlRigBlueprintCommands::Get().ExecuteGraph, 
					NAME_None, TAttribute<FText>(), TAttribute<FText>(), FSlateIcon(FControlRigEditorStyle::Get().GetStyleSetName(), "ControlRig.ExecuteGraph"));
			}
			ToolbarBuilder.EndSection();
		}
	};

	ToolbarExtender->AddToolBarExtension(
		"Asset",
		EExtensionHook::After,
		GetToolkitCommands(),
		FToolBarExtensionDelegate::CreateStatic(&Local::FillToolbar)
	);
}

UBlueprint* FControlRigEditor::GetBlueprintObj() const
{
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	for (UObject* Obj : EditingObjs)
	{
		if (Obj->IsA<UControlRigBlueprint>()) 
		{
			return (UBlueprint*)Obj;
		}
	}
	return nullptr;
}

void FControlRigEditor::SetDetailObjects(const TArray<UObject*>& InObjects)
{
	Inspector->ShowDetailsForObjects(InObjects);
}

void FControlRigEditor::SetDetailObject(UObject* Obj)
{
	TArray<UObject*> Objects;
	if (Obj)
	{
		Objects.Add(Obj);
	}
	SetDetailObjects(Objects);
}

void FControlRigEditor::SetDetailStruct(TSharedPtr<FStructOnScope> StructToDisplay)
{
	Inspector->ShowSingleStruct(StructToDisplay);
}

void FControlRigEditor::ClearDetailObject()
{
	Inspector->ShowDetailsForObjects(TArray<UObject*>());
	Inspector->ShowSingleStruct(TSharedPtr<FStructOnScope>());
}


void FControlRigEditor::CreateDefaultCommands() 
{
	if (GetBlueprintObj())
	{
		FBlueprintEditor::CreateDefaultCommands();
	}
	else
	{
		ToolkitCommands->MapAction( FGenericCommands::Get().Undo, 
			FExecuteAction::CreateSP( this, &FControlRigEditor::UndoAction ));
		ToolkitCommands->MapAction( FGenericCommands::Get().Redo, 
			FExecuteAction::CreateSP( this, &FControlRigEditor::RedoAction ));
	}
}

void FControlRigEditor::OnCreateGraphEditorCommands(TSharedPtr<FUICommandList> GraphEditorCommandsList)
{

}

void FControlRigEditor::Compile()
{
	ClearDetailObject();
	FBlueprintEditor::Compile();
}

FName FControlRigEditor::GetToolkitFName() const
{
	return FName("ControlRigEditor");
}

FText FControlRigEditor::GetBaseToolkitName() const
{
	return LOCTEXT("AppLabel", "Control Rig Editor");
}

FText FControlRigEditor::GetToolkitToolTipText() const
{
	return FAssetEditorToolkit::GetToolTipTextForObject(GetBlueprintObj());
}

FString FControlRigEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("WorldCentricTabPrefix", "Control Rig Editor ").ToString();
}

FLinearColor FControlRigEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.5f, 0.25f, 0.35f, 0.5f );
}

void FControlRigEditor::OnToolkitHostingStarted(const TSharedRef<class IToolkit>& Toolkit)
{
	TSharedPtr<SWidget> InlineContent = Toolkit->GetInlineContent();
	if (InlineContent.IsValid())
	{
		Toolbox->SetContent(InlineContent.ToSharedRef());
	}
}

void FControlRigEditor::OnToolkitHostingFinished(const TSharedRef<class IToolkit>& Toolkit)
{
	Toolbox->SetContent(SNullWidget::NullWidget);
}

void FControlRigEditor::OnActiveTabChanged( TSharedPtr<SDockTab> PreviouslyActive, TSharedPtr<SDockTab> NewlyActivated )
{
	if (!NewlyActivated.IsValid())
	{
		TArray<UObject*> ObjArray;
		Inspector->ShowDetailsForObjects(ObjArray);
	}
	else 
	{
		FBlueprintEditor::OnActiveTabChanged(PreviouslyActive, NewlyActivated);
	}
}

void FControlRigEditor::PostUndo(bool bSuccess)
{
	DocumentManager->CleanInvalidTabs();
	DocumentManager->RefreshAllTabs();

	OnHierarchyChanged();

	FBlueprintEditor::PostUndo(bSuccess);
}

void FControlRigEditor::PostRedo(bool bSuccess)
{
	DocumentManager->RefreshAllTabs();

	FBlueprintEditor::PostRedo(bSuccess);
}

void FControlRigEditor::UndoAction()
{
	GEditor->UndoTransaction();
}

void FControlRigEditor::RedoAction()
{
	GEditor->RedoTransaction();
}

void FControlRigEditor::CreateDefaultTabContents(const TArray<UBlueprint*>& InBlueprints)
{
	FBlueprintEditor::CreateDefaultTabContents(InBlueprints);
}

FGraphAppearanceInfo FControlRigEditor::GetGraphAppearance(UEdGraph* InGraph) const
{
	FGraphAppearanceInfo AppearanceInfo = FBlueprintEditor::GetGraphAppearance(InGraph);

	if (GetBlueprintObj()->IsA(UControlRigBlueprint::StaticClass()))
	{
		AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText_ControlRig", "RIG");
	}

	return AppearanceInfo;
}

void FControlRigEditor::NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged)
{
	FBlueprintEditor::NotifyPostChange(PropertyChangedEvent, PropertyThatChanged);
}

void FControlRigEditor::Tick(float DeltaTime)
{
	FBlueprintEditor::Tick(DeltaTime);
}

bool FControlRigEditor::IsEditable(UEdGraph* InGraph) const
{
	bool bEditable = FBlueprintEditor::IsEditable(InGraph);
	bEditable &= IsGraphInCurrentBlueprint(InGraph);
	return bEditable;
}

FText FControlRigEditor::GetGraphDecorationString(UEdGraph* InGraph) const
{
	return FText::GetEmpty();
}

TStatId FControlRigEditor::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FControlRigEditor, STATGROUP_Tickables);
}

void FControlRigEditor::OnSelectedNodesChangedImpl(const TSet<class UObject*>& NewSelection)
{
	if(!bSelecting)
	{
		TGuardValue<bool> GuardValue(bSelecting, true);
		// Substitute any control rig nodes for their properties, so we display details for them instead
		TSet<class UObject*> SelectedObjects;
		TArray<FString> PropertyPathStrings;
		for(UObject* Object : NewSelection)
		{
			UClass* ClassUsed = nullptr;
			UClass* Class = GetBlueprintObj()->GeneratedClass.Get();
			UClass* SkeletonClass = GetBlueprintObj()->SkeletonGeneratedClass.Get();
			UControlRigGraphNode* ControlRigGraphNode = Cast<UControlRigGraphNode>(Object);
			if(ControlRigGraphNode)
			{
				UProperty* Property = nullptr;

				if(Class && ControlRigGraphNode)
				{
					Property = Class->FindPropertyByName(ControlRigGraphNode->GetPropertyName());
					ClassUsed = Class;
				}

				if(Property == nullptr)
				{
					if(SkeletonClass && ControlRigGraphNode)
					{
						Property = SkeletonClass->FindPropertyByName(ControlRigGraphNode->GetPropertyName());
						ClassUsed = SkeletonClass;
					}
				}

				if(Property)
				{
					SelectedObjects.Add(Property);

					check(ClassUsed);

					// @TODO: if we ever want to support sub-graphs, we will need a full property path here
					PropertyPathStrings.Add(Property->GetName());
				}
			}
			else
			{
				SelectedObjects.Add(Object);
			}
		}

		OnGraphNodeSelectionChangedDelegate.Broadcast(NewSelection);

		// Let the edit mode know about selection
		FControlRigEditMode& EditMode = GetEditMode();
		EditMode.ClearControlSelection();
		EditMode.SetControlSelection(PropertyPathStrings, true);

		FBlueprintEditor::OnSelectedNodesChangedImpl(SelectedObjects);
	}
}

void FControlRigEditor::SetSelectedNodes(const TArray<FString>& InSelectedPropertyPaths)
{
	if(!bSelecting)
	{
		TGuardValue<bool> GuardValue(bSelecting, true);

		UControlRigBlueprint* ControlRigBlueprint = CastChecked<UControlRigBlueprint>(GetBlueprintObj());
		if(UEdGraph* Graph = GetFocusedGraph())
		{
			TSet<const UEdGraphNode*> Nodes;
			TSet<UObject*> Objects;

			for(UEdGraphNode* GraphNode : Graph->Nodes)
			{
				if(UControlRigGraphNode* ControlRigGraphNode = Cast<UControlRigGraphNode>(GraphNode))
				{
					for(const FString& SelectedPropertyPath : InSelectedPropertyPaths)
					{
						if(ControlRigGraphNode->GetPropertyName().ToString() == SelectedPropertyPath)
						{
							Nodes.Add(GraphNode);
							Objects.Add(GraphNode);
							break;
						}
					}
				}
			}

			FocusedGraphEdPtr.Pin()->ClearSelectionSet();
			Graph->SelectNodeSet(Nodes);

			OnGraphNodeSelectionChangedDelegate.Broadcast(Objects);

			// Let the edit mode know about selection
			FControlRigEditMode& EditMode = GetEditMode();
			EditMode.ClearControlSelection();
			EditMode.SetControlSelection(InSelectedPropertyPaths, true);
		}
	}
}

void FControlRigEditor::HandleHideItem()
{
	UControlRigBlueprint* ControlRigBlueprint = CastChecked<UControlRigBlueprint>(GetBlueprintObj());

	TSet<UObject*> SelectedNodes = GetSelectedNodes();
	if(SelectedNodes.Num() > 0)
	{
		FScopedTransaction Transaction(LOCTEXT("HideRigItem", "Hide rig item"));

		ControlRigBlueprint->Modify();

		for(UObject* SelectedNodeObject : SelectedNodes)
		{
			if(UControlRigGraphNode* SelectedNode = Cast<UControlRigGraphNode>(SelectedNodeObject))
			{
				FBlueprintEditorUtils::RemoveNode(ControlRigBlueprint, SelectedNode, true);
			}
		}
	}
}

bool FControlRigEditor::CanHideItem() const
{
	return GetNumberOfSelectedNodes() > 0;
}

void FControlRigEditor::OnBlueprintChangedImpl(UBlueprint* InBlueprint, bool bIsJustBeingCompiled)
{
	FBlueprintEditor::OnBlueprintChangedImpl(InBlueprint, bIsJustBeingCompiled);

	if(InBlueprint == GetBlueprintObj())
	{
		if(bIsJustBeingCompiled)
		{
			UpdateControlRig();
		}

		OnSelectedNodesChangedImpl(GetSelectedNodes());
	}
}

void FControlRigEditor::HandleViewportCreated(const TSharedRef<class IPersonaViewport>& InViewport)
{
	// TODO: this is duplicated code from FAnimBlueprintEditor, would be nice to consolidate. 
	auto GetCompilationStateText = [this]()
	{
		if (UBlueprint* Blueprint = GetBlueprintObj())
		{
			switch (Blueprint->Status)
			{
			case BS_UpToDate:
			case BS_UpToDateWithWarnings:
				// Fall thru and return empty string
				break;
			case BS_Dirty:
				return LOCTEXT("ControlRigBP_Dirty", "Preview out of date");
			case BS_Error:
				return LOCTEXT("ControlRigBP_CompileError", "Compile Error");
			default:
				return LOCTEXT("ControlRigBP_UnknownStatus", "Unknown Status");
			}
		}

		return FText::GetEmpty();
	};

	auto GetCompilationStateVisibility = [this]()
	{
		if (UBlueprint* Blueprint = GetBlueprintObj())
		{
			const bool bUpToDate = (Blueprint->Status == BS_UpToDate) || (Blueprint->Status == BS_UpToDateWithWarnings);
			return bUpToDate ? EVisibility::Collapsed : EVisibility::Visible;
		}

		return EVisibility::Collapsed;
	};

	auto GetCompileButtonVisibility = [this]()
	{
		if (UBlueprint* Blueprint = GetBlueprintObj())
		{
			return (Blueprint->Status == BS_Dirty) ? EVisibility::Visible : EVisibility::Collapsed;
		}

		return EVisibility::Collapsed;
	};

	auto CompileBlueprint = [this]()
	{
		if (UBlueprint* Blueprint = GetBlueprintObj())
		{
			if (!Blueprint->IsUpToDate())
			{
				Compile();
			}
		}

		return FReply::Handled();
	};

	auto GetErrorSeverity = [this]()
	{
		if (UBlueprint* Blueprint = GetBlueprintObj())
		{
			return (Blueprint->Status == BS_Error) ? EMessageSeverity::Error : EMessageSeverity::Warning;
		}

		return EMessageSeverity::Warning;
	};

	auto GetIcon = [this]()
	{
		if (UBlueprint* Blueprint = GetBlueprintObj())
		{
			return (Blueprint->Status == BS_Error) ? FEditorFontGlyphs::Exclamation_Triangle : FEditorFontGlyphs::Eye;
		}

		return FEditorFontGlyphs::Eye;
	};

	InViewport->AddNotification(MakeAttributeLambda(GetErrorSeverity),
		false,
		SNew(SHorizontalBox)
		.Visibility_Lambda(GetCompilationStateVisibility)
		+SHorizontalBox::Slot()
		.FillWidth(1.0f)
		.Padding(4.0f, 4.0f)
		[
			SNew(SHorizontalBox)
			.ToolTipText_Lambda(GetCompilationStateText)
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0.0f, 0.0f, 4.0f, 0.0f)
			[
				SNew(STextBlock)
				.TextStyle(FEditorStyle::Get(), "AnimViewport.MessageText")
				.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.9"))
				.Text_Lambda(GetIcon)
			]
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.FillWidth(1.0f)
			[
				SNew(STextBlock)
				.Text_Lambda(GetCompilationStateText)
				.TextStyle(FEditorStyle::Get(), "AnimViewport.MessageText")
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(2.0f, 0.0f)
		[
			SNew(SButton)
			.ForegroundColor(FSlateColor::UseForeground())
			.ButtonStyle(FEditorStyle::Get(), "FlatButton.Success")
			.Visibility_Lambda(GetCompileButtonVisibility)
			.ToolTipText(LOCTEXT("ControlRigBPViewportCompileButtonToolTip", "Compile this Animation Blueprint to update the preview to reflect any recent changes."))
			.OnClicked_Lambda(CompileBlueprint)
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.Padding(0.0f, 0.0f, 4.0f, 0.0f)
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "AnimViewport.MessageText")
					.Font(FEditorStyle::Get().GetFontStyle("FontAwesome.9"))
					.Text(FEditorFontGlyphs::Cog)
				]
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.TextStyle(FEditorStyle::Get(), "AnimViewport.MessageText")
					.Text(LOCTEXT("ControlRigBPViewportCompileButtonLabel", "Compile"))
				]
			]
		]
	);
}

void FControlRigEditor::HandlePreviewSceneCreated(const TSharedRef<IPersonaPreviewScene>& InPersonaPreviewScene)
{
	AAnimationEditorPreviewActor* Actor = InPersonaPreviewScene->GetWorld()->SpawnActor<AAnimationEditorPreviewActor>(AAnimationEditorPreviewActor::StaticClass(), FTransform::Identity);
	InPersonaPreviewScene->SetActor(Actor);

	// Create the preview component
	UControlRigSkeletalMeshComponent* EditorSkelComp = NewObject<UControlRigSkeletalMeshComponent>(Actor);
	EditorSkelComp->SetSkeletalMesh(InPersonaPreviewScene->GetPersonaToolkit()->GetPreviewMesh());
	InPersonaPreviewScene->SetPreviewMeshComponent(EditorSkelComp);
	UAnimCustomInstance::BindToSkeletalMeshComponent<UControlRigSequencerAnimInstance>(EditorSkelComp);
	InPersonaPreviewScene->AddComponent(EditorSkelComp, FTransform::Identity);

	// set root component, so we can attach to it. 
	Actor->SetRootComponent(EditorSkelComp);

	// set to use custom default mode defined in mesh component
	InPersonaPreviewScene->SetDefaultAnimationMode(EPreviewSceneDefaultAnimationMode::Custom);
}

void FControlRigEditor::UpdateControlRig()
{
	if(UClass* Class = GetBlueprintObj()->GeneratedClass)
	{
		UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
		UControlRigSequencerAnimInstance* AnimInstance = Cast<UControlRigSequencerAnimInstance>(EditorSkelComp->GetAnimInstance());

		if (AnimInstance)
		{
			if (ControlRig == nullptr)
			{
				ControlRig = NewObject<UControlRig>(EditorSkelComp, Class);
				// this is editing time rig
				ControlRig->ExecutionType = ERigExecutionType::Editing;
			}

			// When the control rig is re-instanced on compile, it loses its binding, so we refresh it here if needed
			if (!ControlRig->GetObjectBinding().IsValid())
			{
				ControlRig->SetObjectBinding(MakeShared<FControlRigSkeletalMeshBinding>());
			}

			// initialize is moved post reinstance
			FInputBlendPose Filter;
			AnimInstance->UpdateControlRig(ControlRig, 0, false, false, Filter, 1.0f);
			AnimInstance->RecalcRequiredBones();
			
			// since rig has changed, rebuild draw skeleton
			EditorSkelComp->RebuildDebugDrawSkeleton();
			GetEditMode().SetObjects(ControlRig, FGuid());
		}
	}
}

void FControlRigEditor::AddReferencedObjects( FReferenceCollector& Collector )
{
	FBlueprintEditor::AddReferencedObjects(Collector);

	Collector.AddReferencedObject(ControlRig);
}

void FControlRigEditor::HandlePreviewMeshChanged(USkeletalMesh* InOldSkeletalMesh, USkeletalMesh* InNewSkeletalMesh)
{
	RebindToSkeletalMeshComponent();
}

void FControlRigEditor::RebindToSkeletalMeshComponent()
{
	UDebugSkelMeshComponent* MeshComponent = GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent();
	if (MeshComponent)
	{
		UAnimCustomInstance::BindToSkeletalMeshComponent<UControlRigSequencerAnimInstance>(MeshComponent);
	}
}

void FControlRigEditor::SetupGraphEditorEvents(UEdGraph* InGraph, SGraphEditor::FGraphEditorEvents& InEvents)
{
	FBlueprintEditor::SetupGraphEditorEvents(InGraph, InEvents);

	InEvents.OnCreateActionMenu = SGraphEditor::FOnCreateActionMenu::CreateSP(this, &FControlRigEditor::HandleCreateGraphActionMenu);
}

FActionMenuContent FControlRigEditor::HandleCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed)
{
	return FBlueprintEditor::OnCreateGraphActionMenu(InGraph, InNodePosition, InDraggedPins, bAutoExpand, InOnMenuClosed);
}

void FControlRigEditor::SelectJoint(const FName& InJoint)
{
	// edit mode has to know
	GetEditMode().SelectJoint(InJoint);
	// copy locally, we use this for copying back to template when modified

	SelectedJoint = InJoint;
	UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
	if (EditorSkelComp)
	{
		EditorSkelComp->BonesOfInterest.Reset();

		int32 Index = ControlRig->Hierarchy.BaseHierarchy.GetIndex(InJoint);
		if (Index != INDEX_NONE)
		{
			EditorSkelComp->BonesOfInterest.Add(Index);
		}
	}
}

FTransform FControlRigEditor::GetJointTransform(const FName& InJoint, bool bLocal) const
{
	// @todo: think about transform mode
	if (bLocal)
	{
		return ControlRig->Hierarchy.BaseHierarchy.GetLocalTransform(InJoint);
	}

	return ControlRig->Hierarchy.BaseHierarchy.GetGlobalTransform(InJoint);
}

void FControlRigEditor::SetJointTransform(const FName& InJoint, const FTransform& InTransform)
{
	// execution should be off
	ensure(!ControlRig->bExecutionOn);

	FScopedTransaction Transaction(LOCTEXT("Move Joint", "Move joint transform"));
	UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint();
	ControlRigBP->Modify();

	// moving ref pose warning
	// update init/global transform
	// @todo: this needs revision once we decide how we allow users to modify init/global transform
	// for now, updating init/global of the joint from instances, but only modify init transform for archetype
	// get local transform of current
	// apply init based on parent init * current local 

	ControlRig->Hierarchy.BaseHierarchy.SetInitialTransform(InJoint, InTransform);
	ControlRig->Hierarchy.BaseHierarchy.SetGlobalTransform(InJoint, InTransform);

	ControlRigBP->Hierarchy.SetInitialTransform(InJoint, InTransform);
	
	UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
	if (EditorSkelComp)
	{
		EditorSkelComp->RebuildDebugDrawSkeleton();
	}

	// I don't think I have to mark dirty here. 
	// FBlueprintEditorUtils::MarkBlueprintAsModified(GetControlRigBlueprint());
}

void FControlRigEditor::OnFinishedChangingProperties(const FPropertyChangedEvent& PropertyChangedEvent)
{
//	UE_LOG(LogControlRigEditor, Warning, TEXT("Current Property being modified : %s"), *GetNameSafe(PropertyChangedEvent.Property));

	FName PropertyName = (PropertyChangedEvent.Property != NULL) ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FRigJoint, InitialTransform))
	{
		// if init transform changes, it updates to the base
		UControlRigBlueprint* ControlRigBP = GetControlRigBlueprint();
		if (ControlRig && ControlRigBP)
		{
			if (SelectedJoint != NAME_None)
			{
				const int32 JointIndex = ControlRig->Hierarchy.BaseHierarchy.GetIndex(SelectedJoint);
				if (JointIndex != INDEX_NONE)
				{
					FTransform InitialTransform = ControlRig->Hierarchy.BaseHierarchy.GetInitialTransform(JointIndex);
					// update CDO  @todo - re-think about how we wrap around this nicer
					// copy currently selected joint to base hierarchy			
					ControlRigBP->Hierarchy.SetInitialTransform(JointIndex, InitialTransform);
				}
			}
		}
	}
}

void FControlRigEditor::OnHierarchyChanged()
{
	ClearDetailObject();

	FBlueprintEditorUtils::MarkBlueprintAsModified(GetControlRigBlueprint());

	UControlRigSkeletalMeshComponent* EditorSkelComp = Cast<UControlRigSkeletalMeshComponent>(GetPersonaToolkit()->GetPreviewScene()->GetPreviewMeshComponent());
	if (EditorSkelComp)
	{
		// restart animation 
		EditorSkelComp->InitAnim(true);
		UpdateControlRig();
	}

	// notification
	FNotificationInfo Info(LOCTEXT("HierarchyChangeHelpMessage", "Hierarchy has been successfully modified. If you want to move the joint, compile and turn off execution mode."));
	Info.bFireAndForget = true;
	Info.FadeOutDuration = 10.0f;
	Info.ExpireDuration = 0.0f;

	TSharedPtr<SNotificationItem> NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
	NotificationPtr->SetCompletionState(SNotificationItem::CS_Success);
}
#undef LOCTEXT_NAMESPACE
