// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MeshInstancingTool/SMeshInstancingDialog.h"
#include "MeshInstancingTool/MeshInstancingTool.h"
#include "EditorStyleSet.h"
#include "PropertyEditorModule.h"
#include "IDetailChildrenBuilder.h"
#include "Modules/ModuleManager.h"
#include "SlateOptMacros.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Input/SCheckBox.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "Components/ChildActorComponent.h"
#include "Components/ShapeComponent.h"

#include "IDetailsView.h"

#define LOCTEXT_NAMESPACE "SMeshInstancingDialog"

//////////////////////////////////////////////////////////////////////////
// SMeshInstancingDialog

SMeshInstancingDialog::SMeshInstancingDialog()
{
	bRefreshListView = false;
	NumSelectedMeshComponents = 0;
}

SMeshInstancingDialog::~SMeshInstancingDialog()
{
	// Remove all delegates
	USelection::SelectionChangedEvent.RemoveAll(this);
	USelection::SelectObjectEvent.RemoveAll(this);	
	FEditorDelegates::MapChange.RemoveAll(this);
	FEditorDelegates::NewCurrentLevel.RemoveAll(this);
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SMeshInstancingDialog::Construct(const FArguments& InArgs, FMeshInstancingTool* InTool)
{
	checkf(InTool != nullptr, TEXT("Invalid owner tool supplied"));
	Tool = InTool;

	UpdateSelectedStaticMeshComponents();
	CreateSettingsView();
	
	// Create widget layout
	this->ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				// Static mesh component selection
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(LOCTEXT("MergeStaticMeshComponentsLabel", "Mesh Components to be replaced by instances:"))
					]
				]
				+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
					[
						SAssignNew(ComponentsListView, SListView<TSharedPtr<FInstanceComponentData>>)
						.ListItemsSource(&SelectedComponents)
						.OnGenerateRow(this, &SMeshInstancingDialog::MakeComponentListItemWidget)
						.ToolTipText(LOCTEXT("SelectedComponentsListBoxToolTip", "The selected mesh components will be incorporated replaced by instances"))
					]
			]
		]

		+ SVerticalBox::Slot()
		.Padding(0, 10, 0, 0)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)
				// Static mesh component selection
				+ SVerticalBox::Slot()
				.Padding(FEditorStyle::GetMargin("StandardDialog.ContentPadding"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					[
						SettingsView->AsShared()
					]
				]
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor::Yellow)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Visibility_Lambda([this]()->EVisibility { return this->GetContentEnabledState() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(STextBlock)
				.Text(LOCTEXT("DeleteUndo", "Insufficient mesh components found for instance replacement"))
			]
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(10)
		[
			SNew(SBorder)
			.BorderBackgroundColor(FLinearColor::Green)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			.Visibility_Lambda([this]()->EVisibility { return this->GetPredictedResultsText().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible; })
			[
				SNew(STextBlock)
				.Text(this, &SMeshInstancingDialog::GetPredictedResultsText)
			]
		]
	];	


	// Selection change
	USelection::SelectionChangedEvent.AddRaw(this, &SMeshInstancingDialog::OnLevelSelectionChanged);
	USelection::SelectObjectEvent.AddRaw(this, &SMeshInstancingDialog::OnLevelSelectionChanged);
	FEditorDelegates::MapChange.AddSP(this, &SMeshInstancingDialog::OnMapChange);
	FEditorDelegates::NewCurrentLevel.AddSP(this, &SMeshInstancingDialog::OnNewCurrentLevel);

	InstancingSettings = UMeshInstancingSettingsObject::Get();
	SettingsView->SetObject(InstancingSettings);

	Reset();
}

void SMeshInstancingDialog::OnMapChange(uint32 MapFlags)
{
	Reset();
}

void SMeshInstancingDialog::OnNewCurrentLevel()
{
	Reset();
}


void SMeshInstancingDialog::OnSettingChanged(const FPropertyChangedEvent& InPropertyChangedEvent)
{
	Reset();
}

void SMeshInstancingDialog::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	// Check if we need to update selected components and the listbox
	if (bRefreshListView == true)
	{
		StoreCheckBoxState();
		UpdateSelectedStaticMeshComponents();
		RefreshPredictedResultsText();
		ComponentsListView->ClearSelection();
		ComponentsListView->RequestListRefresh();
		bRefreshListView = false;		
	}
}

void SMeshInstancingDialog::Reset()
{	
	bRefreshListView = true;
}

bool SMeshInstancingDialog::GetContentEnabledState() const
{
	return (GetNumSelectedMeshComponents() >= 1); // Only enabled if a mesh is selected
}

void SMeshInstancingDialog::UpdateSelectedStaticMeshComponents()
{	
	NumSelectedMeshComponents = 0;

	// Retrieve selected actors
	USelection* SelectedActors = GEditor->GetSelectedActors();
	TArray<AActor*> Actors;
	TArray<ULevel*> UniqueLevels;
	for (FSelectionIterator Iter(*SelectedActors); Iter; ++Iter)
	{
		AActor* Actor = Cast<AActor>(*Iter);
		if (Actor)
		{
			Actors.Add(Actor);
			UniqueLevels.AddUnique(Actor->GetLevel());
		}
	}

	// Retrieve static mesh components from selected actors
	SelectedComponents.Empty();
	for (int32 ActorIndex = 0; ActorIndex < Actors.Num(); ++ActorIndex )
	{
		AActor* Actor = Actors[ActorIndex];
		check(Actor != nullptr);

		TArray<UChildActorComponent*> ChildActorComponents;
		Actor->GetComponents<UChildActorComponent>(ChildActorComponents);
		for (UChildActorComponent* ChildComponent : ChildActorComponents)
		{
			// Push actor at the back of array so we will process it
			AActor* ChildActor = ChildComponent->GetChildActor();
			if (ChildActor)
			{
				Actors.Add(ChildActor);
			}
		}
		
		TArray<UPrimitiveComponent*> PrimComponents;
		Actor->GetComponents<UPrimitiveComponent>(PrimComponents);
		for (UPrimitiveComponent* PrimComponent : PrimComponents)
		{
			bool bInclude = false; // Should put into UI list
			bool bShouldIncorporate = false; // Should default to part of merged mesh
			bool bIsMesh = false;
			if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(PrimComponent))
			{
				bShouldIncorporate = (StaticMeshComponent->GetStaticMesh() != nullptr);
				bInclude = true;
				bIsMesh = true;
			}

			if (bInclude)
			{
				SelectedComponents.Add(TSharedPtr<FInstanceComponentData>(new FInstanceComponentData(PrimComponent)));
				TSharedPtr<FInstanceComponentData>& ComponentData = SelectedComponents.Last();

				ComponentData->bShouldIncorporate = bShouldIncorporate;

				// See if we stored a checkbox state for this mesh component, and set accordingly
				ECheckBoxState* StoredState = StoredCheckBoxStates.Find(PrimComponent);
				if (StoredState != nullptr)
				{
					ComponentData->bShouldIncorporate = (*StoredState == ECheckBoxState::Checked);
				}

				// Keep count of selected meshes
				if (ComponentData->bShouldIncorporate && bIsMesh)
				{
					NumSelectedMeshComponents++;
				}
			}

		}		
	}
}

TSharedRef<ITableRow> SMeshInstancingDialog::MakeComponentListItemWidget(TSharedPtr<FInstanceComponentData> ComponentData, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(ComponentData->PrimComponent != nullptr);

	// Retrieve information about the mesh component
	const FString OwningActorName = ComponentData->PrimComponent->GetOwner()->GetName();

	// If box should be enabled
	bool bEnabled = true;
	bool bIsMesh = false;

	FString ComponentInfo;
	if (UStaticMeshComponent* StaticMeshComponent = Cast<UStaticMeshComponent>(ComponentData->PrimComponent.Get()))
	{
		ComponentInfo = (StaticMeshComponent->GetStaticMesh() != nullptr) ? StaticMeshComponent->GetStaticMesh()->GetName() : TEXT("No Static Mesh Available");
		bEnabled = (StaticMeshComponent->GetStaticMesh() != nullptr);
		bIsMesh = true;
	}

	const FString ComponentName = ComponentData->PrimComponent->GetName();

	// See if we stored a checkbox state for this mesh component, and set accordingly
	ECheckBoxState State = bEnabled ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	ECheckBoxState* StoredState = StoredCheckBoxStates.Find(ComponentData->PrimComponent.Get());
	if (StoredState)
	{
		State = *StoredState;
	}
	

	return SNew(STableRow<TSharedPtr<FInstanceComponentData>>, OwnerTable)
		[
			SNew(SBox)
			[
				// Disable UI element if this static mesh component has invalid static mesh data
				SNew(SHorizontalBox)				
				.IsEnabled(bEnabled)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SCheckBox)
					.IsChecked(State)
					.ToolTipText(LOCTEXT("IncorporateCheckBoxToolTip", "When ticked the Component will be incorporated into the merge"))
					
					.OnCheckStateChanged_Lambda([=](ECheckBoxState NewState)
					{
						ComponentData->bShouldIncorporate = (NewState == ECheckBoxState::Checked);

						if (bIsMesh)
						{
							this->NumSelectedMeshComponents += (NewState == ECheckBoxState::Checked) ? 1 : -1;
						}
					})
				]

				+ SHorizontalBox::Slot()
				.Padding(5.0, 0, 0, 0)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Text(FText::FromString(OwningActorName + " - " + ComponentInfo + " - " + ComponentName))
				]
			]
		];

}

void SMeshInstancingDialog::CreateSettingsView()
{
	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bUpdatesFromSelection = true;
	DetailsViewArgs.bLockable = true;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::ComponentsAndActorsUseNameArea;
	DetailsViewArgs.bCustomNameAreaLocation = false;
	DetailsViewArgs.bCustomFilterAreaLocation = true;
	DetailsViewArgs.DefaultsOnlyVisibility = EEditDefaultsOnlyNodeVisibility::Hide;
	
	SettingsView = EditModule.CreateDetailView(DetailsViewArgs);

	SettingsView->OnFinishedChangingProperties().AddSP(this, &SMeshInstancingDialog::OnSettingChanged);
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMeshInstancingDialog::OnLevelSelectionChanged(UObject* Obj)
{
	Reset();
}

void SMeshInstancingDialog::StoreCheckBoxState()
{
	StoredCheckBoxStates.Empty();

	// Loop over selected mesh component and store its checkbox state
	for (TSharedPtr<FInstanceComponentData> SelectedComponent : SelectedComponents )
	{
		UPrimitiveComponent* PrimComponent = SelectedComponent->PrimComponent.Get();
		const ECheckBoxState State = SelectedComponent->bShouldIncorporate ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
		StoredCheckBoxStates.Add(PrimComponent, State);
	}
}

void SMeshInstancingDialog::RefreshPredictedResultsText()
{
	PredictedResultsText = Tool->GetPredictedResultsText();
}

FText SMeshInstancingDialog::GetPredictedResultsText() const
{
	return PredictedResultsText;
}

#undef LOCTEXT_NAMESPACE
