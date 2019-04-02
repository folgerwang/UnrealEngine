// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "DisplayNodes/SequencerObjectBindingNode.h"
#include "Modules/ModuleManager.h"
#include "UObject/UnrealType.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/Commands/UICommandList.h"
#include "Widgets/Layout/SSpacer.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "EditorStyleSet.h"
#include "GameFramework/Actor.h"
#include "Containers/ArrayBuilder.h"
#include "KeyParams.h"
#include "KeyPropertyParams.h"
#include "MovieSceneSpawnable.h"
#include "MovieSceneSection.h"
#include "ISequencerModule.h"
#include "SequencerCommands.h"
#include "MovieScene.h"
#include "Sequencer.h"
#include "SSequencer.h"
#include "SSequencerLabelEditor.h"
#include "MovieSceneSequence.h"
#include "SequencerTrackNode.h"
#include "ObjectEditorUtils.h"
#include "SequencerUtilities.h"
#include "Styling/SlateIconFinder.h"
#include "ScopedTransaction.h"
#include "SequencerDisplayNodeDragDropOp.h"
#include "SequencerFolderNode.h"
#include "SequencerNodeSortingMethods.h"
#include "DisplayNodes/SequencerFolderNode.h"
#include "MovieSceneSequence.h"
#include "MovieScene.h"
#include "MovieSceneFolder.h"
#include "ISequencerTrackEditor.h"

#include "Tracks/MovieSceneSpawnTrack.h"
#include "Sections/MovieSceneSpawnSection.h"
#include "LevelUtils.h"
#include "Engine/LevelStreaming.h"
#include "Engine/World.h"
#include "Editor.h"
#include "Engine/Selection.h"

#define LOCTEXT_NAMESPACE "FObjectBindingNode"


namespace SequencerNodeConstants
{
	extern const float CommonPadding;
}


void GetKeyablePropertyPaths(UClass* Class, void* ValuePtr, UStruct* PropertySource, FPropertyPath PropertyPath, FSequencer& Sequencer, TArray<FPropertyPath>& KeyablePropertyPaths)
{
	//@todo need to resolve this between UMG and the level editor sequencer
	const bool bRecurseAllProperties = Sequencer.IsLevelEditorSequencer();

	for (TFieldIterator<UProperty> PropertyIterator(PropertySource); PropertyIterator; ++PropertyIterator)
	{
		UProperty* Property = *PropertyIterator;

		if (Property && !Property->HasAnyPropertyFlags(CPF_Deprecated))
		{
			PropertyPath.AddProperty(FPropertyInfo(Property));

			bool bIsPropertyKeyable = Sequencer.CanKeyProperty(FCanKeyPropertyParams(Class, PropertyPath));
			if (bIsPropertyKeyable)
			{
				KeyablePropertyPaths.Add(PropertyPath);
			}

			UArrayProperty* ArrayProperty = Cast<UArrayProperty>(Property);
			if (!bIsPropertyKeyable && ArrayProperty)
			{
				FScriptArrayHelper ArrayHelper(ArrayProperty, ArrayProperty->ContainerPtrToValuePtr<void>(ValuePtr));
				for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
				{
					PropertyPath.AddProperty(FPropertyInfo(ArrayProperty->Inner, Index));

					if (Sequencer.CanKeyProperty(FCanKeyPropertyParams(Class, PropertyPath)))
					{
						KeyablePropertyPaths.Add(PropertyPath);
						bIsPropertyKeyable = true;
					}
					else if (UStructProperty* StructProperty = Cast<UStructProperty>(ArrayProperty->Inner))
					{
						GetKeyablePropertyPaths(Class, ArrayHelper.GetRawPtr(Index), StructProperty->Struct, PropertyPath, Sequencer, KeyablePropertyPaths);
					}

					PropertyPath = *PropertyPath.TrimPath(1);
				}
			}

			if (!bIsPropertyKeyable || bRecurseAllProperties)
			{
				if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
				{
					GetKeyablePropertyPaths(Class, StructProperty->ContainerPtrToValuePtr<void>(ValuePtr), StructProperty->Struct, PropertyPath, Sequencer, KeyablePropertyPaths);
				}
			}

			PropertyPath = *PropertyPath.TrimPath(1);
		}
	}
}


struct PropertyMenuData
{
	FString MenuName;
	FPropertyPath PropertyPath;
};



FSequencerObjectBindingNode::FSequencerObjectBindingNode(FName NodeName, const FText& InDisplayName, const FGuid& InObjectBinding, TSharedPtr<FSequencerDisplayNode> InParentNode, FSequencerNodeTree& InParentTree)
	: FSequencerDisplayNode(NodeName, InParentNode, InParentTree)
	, ObjectBinding(InObjectBinding)
	, DefaultDisplayName(InDisplayName)
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene->FindPossessable(ObjectBinding))
	{
		BindingType = EObjectBindingType::Possessable;
	}
	else if (MovieScene->FindSpawnable(ObjectBinding))
	{
		BindingType = EObjectBindingType::Spawnable;
	}
	else
	{
		BindingType = EObjectBindingType::Unknown;
	}
}

void FSequencerObjectBindingNode::AddTrackNode( TSharedRef<FSequencerTrackNode> NewChild )
{
	AddChildAndSetParent( NewChild );
}

/* FSequencerDisplayNode interface
 *****************************************************************************/

void FSequencerObjectBindingNode::BuildContextMenu(FMenuBuilder& MenuBuilder)
{
	ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>("Sequencer");

	UObject* BoundObject = GetSequencer().FindSpawnedObjectOrTemplate(ObjectBinding);

	TSharedRef<FUICommandList> CommandList(new FUICommandList);
	TSharedPtr<FExtender> Extender = SequencerModule.GetObjectBindingContextMenuExtensibilityManager()->GetAllExtenders(CommandList, TArrayBuilder<UObject*>().Add(BoundObject));
	if (Extender.IsValid())
	{
		MenuBuilder.PushExtender(Extender.ToSharedRef());
	}

	if (GetSequencer().IsLevelEditorSequencer())
	{
		UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding);

		if (Spawnable)
		{
			MenuBuilder.BeginSection("Spawnable", LOCTEXT("SpawnableMenuSectionName", "Spawnable"));
	
			MenuBuilder.AddSubMenu(
				LOCTEXT("OwnerLabel", "Spawned Object Owner"),
				LOCTEXT("OwnerTooltip", "Specifies how the spawned object is to be owned"),
				FNewMenuDelegate::CreateSP(this, &FSequencerObjectBindingNode::AddSpawnOwnershipMenu)
			);

			MenuBuilder.AddSubMenu(
				LOCTEXT("SubLevelLabel", "Spawnable Level"),
				LOCTEXT("SubLevelTooltip", "Specifies which level the spawnable should be spawned into"),
				FNewMenuDelegate::CreateSP(this, &FSequencerObjectBindingNode::AddSpawnLevelMenu)
			);

			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().SaveCurrentSpawnableState );
			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ConvertToPossessable );

			MenuBuilder.EndSection();
		}
		else
		{
			const UClass* ObjectClass = GetClassForObjectBinding();
			
			if (ObjectClass->IsChildOf(AActor::StaticClass()))
			{
				FFormatNamedArguments Args;

				MenuBuilder.AddSubMenu(
					FText::Format(LOCTEXT("Assign Actor", "Assign Actor"), Args),
					FText::Format(LOCTEXT("AssignActorTooltip", "Assign an actor to this track"), Args),
					FNewMenuDelegate::CreateSP(this, &FSequencerObjectBindingNode::AddAssignActorMenu));
			}

			MenuBuilder.AddMenuEntry( FSequencerCommands::Get().ConvertToSpawnable );
		}

		MenuBuilder.BeginSection("Import/Export", LOCTEXT("ImportExportMenuSectionName", "Import/Export"));
		
		MenuBuilder.AddMenuEntry(
			LOCTEXT("Import FBX", "Import..."),
			LOCTEXT("ImportFBXTooltip", "Import FBX animation to this object"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ GetSequencer().ImportFBXOntoSelectedNodes(); })
			));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("Export FBX", "Export..."),
			LOCTEXT("ExportFBXTooltip", "Export FBX animation from this object"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ GetSequencer().ExportFBX(); })
			));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("Export to Camera Anim", "Export to Camera Anim..."),
			LOCTEXT("ExportToCameraAnimTooltip", "Exports the animation to a camera anim asset"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateLambda([=]{ GetSequencer().ExportToCameraAnim(); })
			));
		MenuBuilder.EndSection();
	}

	MenuBuilder.BeginSection("Organize", LOCTEXT("OrganizeContextMenuSectionName", "Organize"));
	{
		MenuBuilder.AddSubMenu(
			LOCTEXT("LabelsSubMenuText", "Labels"),
			LOCTEXT("LabelsSubMenuTip", "Add or remove labels on this track"),
			FNewMenuDelegate::CreateSP(this, &FSequencerObjectBindingNode::HandleLabelsSubMenuCreate)
		);
	}
	MenuBuilder.EndSection();

	GetSequencer().BuildCustomContextMenuForGuid(MenuBuilder, ObjectBinding);

	FSequencerDisplayNode::BuildContextMenu(MenuBuilder);
}

void FSequencerObjectBindingNode::AddSpawnOwnershipMenu(FMenuBuilder& MenuBuilder)
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding);
	if (!Spawnable)
	{
		return;
	}
	auto Callback = [=](ESpawnOwnership NewOwnership){

		FScopedTransaction Transaction(LOCTEXT("SetSpawnOwnership", "Set Spawnable Ownership"));

		Spawnable->SetSpawnOwnership(NewOwnership);

		// Overwrite the completion state for all spawn sections to ensure the expected behaviour.
		EMovieSceneCompletionMode NewCompletionMode = NewOwnership == ESpawnOwnership::InnerSequence ? EMovieSceneCompletionMode::RestoreState : EMovieSceneCompletionMode::KeepState;

		// Make all spawn sections retain state
		UMovieSceneSpawnTrack* SpawnTrack = MovieScene->FindTrack<UMovieSceneSpawnTrack>(ObjectBinding);
		if (SpawnTrack)
		{
			for (UMovieSceneSection* Section : SpawnTrack->GetAllSections())
			{
				Section->Modify();
				Section->EvalOptions.CompletionMode = NewCompletionMode;
			}
		}
	};

	MenuBuilder.AddMenuEntry(
		LOCTEXT("ThisSequence_Label", "This Sequence"),
		LOCTEXT("ThisSequence_Tooltip", "Indicates that this sequence will own the spawned object. The object will be destroyed at the end of the sequence."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(Callback, ESpawnOwnership::InnerSequence),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Spawnable->GetSpawnOwnership() == ESpawnOwnership::InnerSequence; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("MasterSequence_Label", "Master Sequence"),
		LOCTEXT("MasterSequence_Tooltip", "Indicates that the outermost sequence will own the spawned object. The object will be destroyed when the outermost sequence stops playing."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(Callback, ESpawnOwnership::MasterSequence),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Spawnable->GetSpawnOwnership() == ESpawnOwnership::MasterSequence; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("External_Label", "External"),
		LOCTEXT("External_Tooltip", "Indicates this object's lifetime is managed externally once spawned. It will not be destroyed by sequencer."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda(Callback, ESpawnOwnership::External),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=]{ return Spawnable->GetSpawnOwnership() == ESpawnOwnership::External; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);
}


void FSequencerObjectBindingNode::AddSpawnLevelMenu(FMenuBuilder& MenuBuilder)
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding);
	if (!Spawnable)
	{
		return;
	}

	MenuBuilder.AddMenuEntry(
		NSLOCTEXT("UnrealEd", "PersistentLevel", "Persistent Level"),
		NSLOCTEXT("UnrealEd", "PersistentLevel", "Persistent Level"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] { GetSequencer().SetSelectedNodesSpawnableLevel(NAME_None); }),
			FCanExecuteAction(),
			FIsActionChecked::CreateLambda([=] { return Spawnable->GetLevelName() == NAME_None; })
		),
		NAME_None,
		EUserInterfaceActionType::ToggleButton
	);

	for (ULevelStreaming* LevelStreaming : GWorld->GetStreamingLevels())
	{
		if (LevelStreaming)
		{
			FName LevelName = FPackageName::GetShortFName( LevelStreaming->GetWorldAssetPackageFName() );

			MenuBuilder.AddMenuEntry(
				FText::FromName(LevelName),
				FText::FromName(LevelName),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateLambda([=] { GetSequencer().SetSelectedNodesSpawnableLevel(LevelName); }),
					FCanExecuteAction(),
					FIsActionChecked::CreateLambda([=] { return Spawnable->GetLevelName() == LevelName; })
				),
				NAME_None,
				EUserInterfaceActionType::ToggleButton
			);
		}
	}
}

void FSequencerObjectBindingNode::AddAssignActorMenu(FMenuBuilder& MenuBuilder)
{
	TArray<AActor*> SelectedActors;
	GEditor->GetSelectedActors()->GetSelectedObjects<AActor>(SelectedActors);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AddSelectedToBinding", "Add Selected"),
		LOCTEXT("AddSelectedToBindingTooltip", "Add selected objects to this track"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] { GetSequencer().AddActorsToBinding(ObjectBinding, SelectedActors); }),
			FCanExecuteAction::CreateLambda([=] { return SelectedActors.Num() > 0; })
			)
		);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("ReplaceBindingWithSelected", "Replace with Selected"),
		LOCTEXT("ReplaceBindingWithSelectedTooltip", "Replace the object binding with selected objects"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] { GetSequencer().ReplaceBindingWithActors(ObjectBinding, SelectedActors); }),
			FCanExecuteAction::CreateLambda([=] { return SelectedActors.Num() > 0; })
		)
	);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("RemoveSelectedFromBinding", "Remove Selected"),
		LOCTEXT("RemoveSelectedFromBindingTooltip", "Remove selected objects from this track"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] { GetSequencer().RemoveActorsFromBinding(ObjectBinding, SelectedActors); }),
			FCanExecuteAction::CreateLambda([=] { return SelectedActors.Num() > 0; })
		)
	);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("RemoveAllBindings", "Remove All"),
		LOCTEXT("RemoveAllBindingsTooltip", "Remove all bound objects from this track"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] { GetSequencer().RemoveAllBindings(ObjectBinding); })
		)
	);
	MenuBuilder.AddMenuEntry(
		LOCTEXT("RemoveMissing", "Remove Missing"),
		LOCTEXT("RemoveMissingooltip", "Remove missing objects bound to this track"),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateLambda([=] { GetSequencer().RemoveInvalidBindings(ObjectBinding); })
		)
	);

	GetSequencer().AssignActor(MenuBuilder, ObjectBinding);
}


bool FSequencerObjectBindingNode::CanRenameNode() const
{
	return true;
}

TSharedRef<SWidget> FSequencerObjectBindingNode::GetCustomOutlinerContent()
{
	// Create a container edit box
	TSharedRef<SHorizontalBox> BoxPanel = SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SSpacer)
		];


	TAttribute<bool> HoverState = TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FSequencerDisplayNode::IsHovered));

	BoxPanel->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			FSequencerUtilities::MakeAddButton(LOCTEXT("TrackText", "Track"), FOnGetContent::CreateSP(this, &FSequencerObjectBindingNode::HandleAddTrackComboButtonGetMenuContent), HoverState, GetSequencer().AsShared())
		];

	const UClass* ObjectClass = GetClassForObjectBinding();
	GetSequencer().BuildObjectBindingEditButtons(BoxPanel, ObjectBinding, ObjectClass);

	return BoxPanel;
}


FText FSequencerObjectBindingNode::GetDisplayName() const
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene != nullptr)
	{
		return MovieScene->GetObjectDisplayName(ObjectBinding);
	}

	return DefaultDisplayName;
}

FLinearColor FSequencerObjectBindingNode::GetDisplayNameColor() const
{
	FSequencer& Sequencer = ParentTree.GetSequencer();

	TArrayView<TWeakObjectPtr<> > BoundObjects = Sequencer.FindBoundObjects(ObjectBinding, Sequencer.GetFocusedTemplateID());

	if (BoundObjects.Num() > 0)
	{
		int32 NumValidObjects = 0;
		for (const TWeakObjectPtr<>& BoundObject : BoundObjects)
		{
			if (BoundObject.IsValid())
			{
				++NumValidObjects;
			}
		}

		if (NumValidObjects == BoundObjects.Num())
		{
			return FSequencerDisplayNode::GetDisplayNameColor();
		}
	
		return FLinearColor::Yellow;
	}

	// Spawnables don't have valid object bindings when their track hasn't spawned them yet,
	// so we override the default behavior of red with a gray so that users don't think there is something wrong.
	
	TSharedPtr<FSequencerDisplayNode> CurrentNode = SharedThis((FSequencerDisplayNode*)this);

	while (CurrentNode.IsValid())
	{
		if (CurrentNode->GetType() == ESequencerNode::Object)
		{
			if (StaticCastSharedPtr<FSequencerObjectBindingNode>(CurrentNode)->GetBindingType() == EObjectBindingType::Spawnable)
			{
				return FLinearColor::Gray;
			}
		}

		CurrentNode = CurrentNode->GetParent();
	}


	return FLinearColor::Red;
}

FText FSequencerObjectBindingNode::GetDisplayNameToolTipText() const
{
	FSequencer& Sequencer = ParentTree.GetSequencer();

	TArrayView<TWeakObjectPtr<>> BoundObjects = Sequencer.FindObjectsInCurrentSequence(ObjectBinding);

	if ( BoundObjects.Num() == 0 )
	{
		return LOCTEXT("InvalidBoundObjectToolTip", "The object bound to this track is missing.");
	}
	else if (BoundObjects.Num() > 1)
	{
		TArray<FString> ValidBoundObjectLabels;
		bool bAddEllipsis = false;
		int32 NumMissing = 0;
		for (const TWeakObjectPtr<>& Ptr : BoundObjects)
		{
			UObject* Obj = Ptr.Get();

			if (Obj == nullptr)
			{
				++NumMissing;
				continue;
			}

			if (AActor* Actor = Cast<AActor>(Obj))
			{
				ValidBoundObjectLabels.Add(Actor->GetActorLabel());
			}
			else
			{
				ValidBoundObjectLabels.Add(Obj->GetName());
			}

			if (ValidBoundObjectLabels.Num() > 3)
			{
				bAddEllipsis = true;
				break;
			}
		}

		FString MultipleBoundObjectLabel = FString::Join(ValidBoundObjectLabels, TEXT(", "));
		if (bAddEllipsis)
		{
			MultipleBoundObjectLabel += FString::Printf(TEXT("... %d more"), BoundObjects.Num()-3);
		}

		if (NumMissing != 0)
		{
			MultipleBoundObjectLabel += FString::Printf(TEXT(" (%d missing)"), NumMissing);
		}

		return FText::FromString(MultipleBoundObjectLabel);
	}
	else
	{
		return FText();
	}
}

const FSlateBrush* FSequencerObjectBindingNode::GetIconBrush() const
{
	return FSlateIconFinder::FindIconBrushForClass(GetClassForObjectBinding());
}

const FSlateBrush* FSequencerObjectBindingNode::GetIconOverlayBrush() const
{
	if (BindingType == EObjectBindingType::Spawnable)
	{
		return FEditorStyle::GetBrush("Sequencer.SpawnableIconOverlay");
	}
	else
	{
		FSequencer& Sequencer = ParentTree.GetSequencer();
		const int32 NumBoundObjects = Sequencer.FindObjectsInCurrentSequence(ObjectBinding).Num();

		if (NumBoundObjects > 1)
		{
			return FEditorStyle::GetBrush("Sequencer.MultipleIconOverlay");
		}
	}
	return nullptr;
}

FText FSequencerObjectBindingNode::GetIconToolTipText() const
{
	if (BindingType == EObjectBindingType::Spawnable)
	{
		return LOCTEXT("SpawnableToolTip", "This item is spawned by sequencer according to this object's spawn track.");
	}
	else if (BindingType == EObjectBindingType::Possessable)
	{
		return LOCTEXT("PossessableToolTip", "This item is a possessable reference to an existing object.");
	}

	return FText();
}

float FSequencerObjectBindingNode::GetNodeHeight() const
{
	return SequencerLayoutConstants::ObjectNodeHeight + SequencerNodeConstants::CommonPadding*2;
}


FNodePadding FSequencerObjectBindingNode::GetNodePadding() const
{
	return FNodePadding(0.f);//SequencerNodeConstants::CommonPadding);
}


ESequencerNode::Type FSequencerObjectBindingNode::GetType() const
{
	return ESequencerNode::Object;
}


void FSequencerObjectBindingNode::SetDisplayName(const FText& NewDisplayName)
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene != nullptr)
	{
		FScopedTransaction Transaction(LOCTEXT("SetTrackName", "Set Track Name"));

		// Modify the movie scene so that it gets marked dirty and renames are saved consistently.
		MovieScene->Modify();
		MovieScene->SetObjectDisplayName(ObjectBinding, NewDisplayName);

		FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(GetObjectBinding());
		if (Spawnable)
		{
			Spawnable->SetName(NewDisplayName.ToString());
		}

		FMovieScenePossessable* Possessable = MovieScene->FindPossessable(GetObjectBinding());
		if (Possessable)
		{
			Possessable->SetName(NewDisplayName.ToString());
		}
	}
}


bool FSequencerObjectBindingNode::CanDrag() const
{
	TSharedPtr<FSequencerDisplayNode> ParentSeqNode = GetParent();
	return ParentSeqNode.IsValid() == false || ParentSeqNode->GetType() != ESequencerNode::Object;
}

TOptional<EItemDropZone> FSequencerObjectBindingNode::CanDrop(FSequencerDisplayNodeDragDropOp& DragDropOp, EItemDropZone ItemDropZone) const
{
	DragDropOp.ResetToDefaultToolTip();

	// Prevent taking any parent that's part of the dragged node hierarchy from being put inside a child of itself
	// This is done first before the other checks so that the UI stays consistent as you move between them, otherwise
	// when you are above/below a node it reports this error, but if you were on top of a node it would do the standard
	// no-drag-drop due to OntoItem being blocked. 
	TSharedPtr<FSequencerDisplayNode> CurrentNode = SharedThis((FSequencerDisplayNode*)this);
	while (CurrentNode.IsValid())
	{
		if (DragDropOp.GetDraggedNodes().Contains(CurrentNode))
		{
			DragDropOp.CurrentHoverText = NSLOCTEXT("SequencerFolderNode", "ParentIntoChildDragErrorFormat", "Can't drag a parent node into one of it's children.");
			return TOptional<EItemDropZone>();
		}
		CurrentNode = CurrentNode->GetParent();
	}

	// Override Onto and Below to be Above to smooth out the UI changes as you scroll over many items.
	// This removes a confusing "above" -> "blocked" -> "above/below" transition.
	if (ItemDropZone == EItemDropZone::OntoItem || ItemDropZone == EItemDropZone::BelowItem)
	{
		ItemDropZone = EItemDropZone::AboveItem;
	}

	if (GetParent().IsValid() && GetParent()->GetType() != ESequencerNode::Folder)
	{
		// Object Binding Nodes can have other binding nodes as their parents and we
		// don't allow re-arranging tracks within a binding node.
		return TOptional<EItemDropZone>();
	}

	for (TSharedRef<FSequencerDisplayNode> Node : DragDropOp.GetDraggedNodes())
	{
		bool bValidType = Node->GetType() == ESequencerNode::Folder || Node->GetType() == ESequencerNode::Object || Node->GetType() == ESequencerNode::Track;
		if (!bValidType)
		{
			return TOptional<EItemDropZone>();
		}

		TSharedPtr<FSequencerDisplayNode> ParentSeqNode = Node->GetParent();

		if (ParentSeqNode.IsValid())
		{
			if (ParentSeqNode->GetType() != ESequencerNode::Folder)
			{
				// If we have a parent who is not a folder (ie: The node is a component track on an actor) then it can't be rearranged.
				return TOptional<EItemDropZone>();
			}
		}
	}

	TArray<UMovieSceneFolder*> AdjacentFolders;
	if (GetParent().IsValid())
	{
		// We are either trying to drop adjacent to ourself (when nestled), or as a child of ourself, so we add either our siblings or our children
		// to the list of possibly conflicting names.
		for (TSharedRef <FSequencerDisplayNode> Child : GetParent()->GetChildNodes())
		{
			if (Child->GetType() == ESequencerNode::Folder)
			{
				TSharedRef<FSequencerFolderNode> FolderNode = StaticCastSharedRef<FSequencerFolderNode>(Child);
				AdjacentFolders.Add(&FolderNode->GetFolder());
			}
		}
	}
	else
	{
		// If this folder has no parent then this is a root level folder, so we need to check the Movie Scene's child list for conflicting children names.
		UMovieScene* FocusedMovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
		AdjacentFolders.Append(FocusedMovieScene->GetRootFolders());
	}

	// Check each node we're dragging to see if any of them have a name conflict - if so, block the whole drag/drop operation.
	for (TSharedRef<FSequencerDisplayNode> DraggedNode : DragDropOp.GetDraggedNodes())
	{
		if (DraggedNode->GetType() == ESequencerNode::Folder)
		{
			TSharedRef<FSequencerFolderNode> DraggedFolder = StaticCastSharedRef<FSequencerFolderNode>(DraggedNode);

			// Name Conflicts are only an issue on folders.
			bool bHasNameConflict = false;
			for (UMovieSceneFolder* Folder : AdjacentFolders)
			{
				// We don't allow a folder with the same name to become a sibling, but we need to not check the dragged node if it is already at that
				// hierarchy depth so that we can rearrange them by triggering EItemDropZone::AboveItem / EItemDropZone::BelowItem on the same hierarchy.
				if (&DraggedFolder->GetFolder() != Folder && DraggedFolder->GetFolder().GetFolderName() == Folder->GetFolderName())
				{
					bHasNameConflict = true;
					break;
				}
			}

			if (bHasNameConflict)
			{
				DragDropOp.CurrentHoverText = FText::Format(
					NSLOCTEXT("SequencerFolderNode", "DuplicateFolderDragErrorFormat", "Folder with name '{0}' already exists."),
					FText::FromName(DraggedFolder->GetFolder().GetFolderName()));

				return TOptional<EItemDropZone>();
			}
		}
	}

	// The dragged nodes were either all in folders, or all at the sequencer root.
	return ItemDropZone;
}

void FSequencerObjectBindingNode::Drop(const TArray<TSharedRef<FSequencerDisplayNode>>& DraggedNodes, EItemDropZone ItemDropZone)
{
	const FScopedTransaction Transaction(NSLOCTEXT("SequencerObjectBindingNode", "MoveItems", "Move items."));
	for (TSharedRef<FSequencerDisplayNode> DraggedNode : DraggedNodes)
	{
		TSharedPtr<FSequencerDisplayNode> DraggedSeqNodeParent = DraggedNode->GetParent();

		if (GetParent().IsValid())
		{
			// If the object is coming from the root or it's coming from another folder then we can allow it to move adjacent to us.
			if (!DraggedSeqNodeParent.IsValid() || (DraggedSeqNodeParent.IsValid() && DraggedSeqNodeParent->GetType() == ESequencerNode::Folder))
			{
				checkf(GetParent()->GetType() == ESequencerNode::Folder, TEXT("Cannot reorder when parent is not a folder."));
				TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>(GetParent());

				// Let the folder we're going into remove us from our old parent and put us as a child of it first.
				ParentFolder->MoveDisplayNodeToFolder(DraggedNode);
			}
		}
		else
		{
			// We're at root and they're placing above or below us
			ParentTree.MoveDisplayNodeToRoot(DraggedNode);
		}
	}

	if (DraggedNodes.Num() > 0)
	{
		if (GetParent().IsValid())
		{
			checkf(GetParent()->GetType() == ESequencerNode::Folder, TEXT("Cannot reorder when parent is not a folder."));
			TSharedPtr<FSequencerFolderNode> ParentFolder = StaticCastSharedPtr<FSequencerFolderNode>(GetParent());

			// Sort our dragged nodes relative to our siblings.
			SortAndSetSortingOrder(DraggedNodes, ParentFolder->GetChildNodes(), ItemDropZone, FDisplayNodeTreePositionSorter(), SharedThis(this));
		}
		else
		{
			// We're at root and they're placing above or below us
			SortAndSetSortingOrder(DraggedNodes, GetSequencer().GetNodeTree()->GetRootNodes(), ItemDropZone, FDisplayNodeTreePositionSorter(), SharedThis(this));
		}
	}

	ParentTree.GetSequencer().NotifyMovieSceneDataChanged(EMovieSceneDataChangeType::MovieSceneStructureItemsChanged);
}


/* FSequencerObjectBindingNode implementation
 *****************************************************************************/

void FSequencerObjectBindingNode::AddPropertyMenuItems(FMenuBuilder& AddTrackMenuBuilder, TArray<FPropertyPath> KeyableProperties, int32 PropertyNameIndexStart, int32 PropertyNameIndexEnd)
{
	TArray<PropertyMenuData> KeyablePropertyMenuData;

	for (auto KeyableProperty : KeyableProperties)
	{
		TArray<FString> PropertyNames;
		if (PropertyNameIndexEnd == -1)
		{
			PropertyNameIndexEnd = KeyableProperty.GetNumProperties();
		}

		//@todo
		if (PropertyNameIndexStart >= KeyableProperty.GetNumProperties())
		{
			continue;
		}

		for (int32 PropertyNameIndex = PropertyNameIndexStart; PropertyNameIndex < PropertyNameIndexEnd; ++PropertyNameIndex)
		{
			PropertyNames.Add(KeyableProperty.GetPropertyInfo(PropertyNameIndex).Property.Get()->GetDisplayNameText().ToString());
		}

		PropertyMenuData KeyableMenuData;
		{
			KeyableMenuData.PropertyPath = KeyableProperty;
			KeyableMenuData.MenuName = FString::Join( PropertyNames, TEXT( "." ) );
		}

		KeyablePropertyMenuData.Add(KeyableMenuData);
	}

	// Sort on the menu name
	KeyablePropertyMenuData.Sort([](const PropertyMenuData& A, const PropertyMenuData& B)
	{
		int32 CompareResult = A.MenuName.Compare(B.MenuName);
		return CompareResult < 0;
	});

	// Add menu items
	for (int32 MenuDataIndex = 0; MenuDataIndex < KeyablePropertyMenuData.Num(); ++MenuDataIndex)
	{
		FUIAction AddTrackMenuAction(FExecuteAction::CreateSP(this, &FSequencerObjectBindingNode::HandlePropertyMenuItemExecute, KeyablePropertyMenuData[MenuDataIndex].PropertyPath));
		AddTrackMenuBuilder.AddMenuEntry(FText::FromString(KeyablePropertyMenuData[MenuDataIndex].MenuName), FText(), FSlateIcon(), AddTrackMenuAction);
	}
}


const UClass* FSequencerObjectBindingNode::GetClassForObjectBinding() const
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
	FMovieSceneSpawnable* Spawnable = MovieScene->FindSpawnable(ObjectBinding);
	FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ObjectBinding);
	
	// should exist, but also shouldn't be both a spawnable and a possessable
	check((Spawnable != nullptr) ^ (Possessable != nullptr));
	const UClass* ObjectClass = Spawnable ? Spawnable->GetObjectTemplate()->GetClass() : Possessable->GetPossessedObjectClass();

	return ObjectClass;
}

/* FSequencerObjectBindingNode callbacks
 *****************************************************************************/

TSharedRef<SWidget> FSequencerObjectBindingNode::HandleAddTrackComboButtonGetMenuContent()
{
	FSequencer& Sequencer = GetSequencer();

	//@todo need to resolve this between UMG and the level editor sequencer
	const bool bUseSubMenus = Sequencer.IsLevelEditorSequencer();

	UObject* BoundObject = GetSequencer().FindSpawnedObjectOrTemplate(ObjectBinding);
	const UClass* ObjectClass = GetClassForObjectBinding();

	ISequencerModule& SequencerModule = FModuleManager::GetModuleChecked<ISequencerModule>( "Sequencer" );
	TSharedRef<FUICommandList> CommandList(new FUICommandList);

	TSharedRef<FExtender> Extender = SequencerModule.GetAddTrackMenuExtensibilityManager()->GetAllExtenders(CommandList, TArrayBuilder<UObject*>().Add(BoundObject)).ToSharedRef();

	for (const TSharedPtr<ISequencerTrackEditor>& TrackEditor : GetSequencer().GetTrackEditors())
	{
		TrackEditor->ExtendObjectBindingTrackMenu(Extender, ObjectBinding, ObjectClass);
	}

	FMenuBuilder AddTrackMenuBuilder(true, nullptr, Extender);

	const int32 NumStartingBlocks = AddTrackMenuBuilder.GetMultiBox()->GetBlocks().Num();

	AddTrackMenuBuilder.BeginSection("Tracks", LOCTEXT("TracksMenuHeader" , "Tracks"));
	GetSequencer().BuildObjectBindingTrackMenu(AddTrackMenuBuilder, ObjectBinding, ObjectClass);
	AddTrackMenuBuilder.EndSection();

	TArray<FPropertyPath> KeyablePropertyPaths;

	if (BoundObject != nullptr)
	{
		FPropertyPath PropertyPath;
		GetKeyablePropertyPaths(BoundObject->GetClass(), BoundObject, BoundObject->GetClass(), PropertyPath, Sequencer, KeyablePropertyPaths);
	}

	// [Aspect Ratio]
	// [PostProcess Settings] [Bloom1Tint] [X]
	// [PostProcess Settings] [Bloom1Tint] [Y]
	// [PostProcess Settings] [ColorGrading]
	// [Ortho View]

	// Create property menu data based on keyable property paths
	TArray<PropertyMenuData> KeyablePropertyMenuData;
	for (const FPropertyPath& KeyablePropertyPath : KeyablePropertyPaths)
	{
		UProperty* Property = KeyablePropertyPath.GetRootProperty().Property.Get();
		if (Property)
		{
			PropertyMenuData KeyableMenuData;
			KeyableMenuData.PropertyPath = KeyablePropertyPath;
			if (KeyablePropertyPath.GetRootProperty().ArrayIndex != INDEX_NONE)
			{
				KeyableMenuData.MenuName = FText::Format(LOCTEXT("PropertyMenuTextFormat", "{0} [{1}]"), Property->GetDisplayNameText(), FText::AsNumber(KeyablePropertyPath.GetRootProperty().ArrayIndex)).ToString();
			}
			else
			{
				KeyableMenuData.MenuName = Property->GetDisplayNameText().ToString();
			}
			KeyablePropertyMenuData.Add(KeyableMenuData);
		}
	}

	// Sort on the menu name
	KeyablePropertyMenuData.Sort([](const PropertyMenuData& A, const PropertyMenuData& B)
	{
		int32 CompareResult = A.MenuName.Compare(B.MenuName);
		return CompareResult < 0;
	});
	

	// Add menu items
	AddTrackMenuBuilder.BeginSection( SequencerMenuExtensionPoints::AddTrackMenu_PropertiesSection, LOCTEXT("PropertiesMenuHeader" , "Properties"));
	for (int32 MenuDataIndex = 0; MenuDataIndex < KeyablePropertyMenuData.Num(); )
	{
		TArray<FPropertyPath> KeyableSubMenuPropertyPaths;

		KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);

		// If this menu data only has one property name, add the menu item
		if (KeyablePropertyMenuData[MenuDataIndex].PropertyPath.GetNumProperties() == 1 || !bUseSubMenus)
		{
			AddPropertyMenuItems(AddTrackMenuBuilder, KeyableSubMenuPropertyPaths);
			++MenuDataIndex;
		}
		// Otherwise, look to the next menu data to gather up new data
		else
		{
			for (; MenuDataIndex < KeyablePropertyMenuData.Num()-1; )
			{
				if (KeyablePropertyMenuData[MenuDataIndex].MenuName == KeyablePropertyMenuData[MenuDataIndex+1].MenuName)
				{	
					++MenuDataIndex;
					KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);
				}
				else
				{
					break;
				}
			}

			AddTrackMenuBuilder.AddSubMenu(
				FText::FromString(KeyablePropertyMenuData[MenuDataIndex].MenuName),
				FText::GetEmpty(), 
				FNewMenuDelegate::CreateSP(this, &FSequencerObjectBindingNode::HandleAddTrackSubMenuNew, KeyableSubMenuPropertyPaths, 0));

			++MenuDataIndex;
		}
	}
	AddTrackMenuBuilder.EndSection();

	if (AddTrackMenuBuilder.GetMultiBox()->GetBlocks().Num() == NumStartingBlocks)
	{
		TSharedRef<SWidget> EmptyTip = SNew(SBox)
			.Padding(FMargin(15.f, 7.5f))
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NoKeyablePropertiesFound", "No keyable properties or tracks"))
				.ColorAndOpacity(FSlateColor::UseSubduedForeground())
			];

		AddTrackMenuBuilder.AddWidget(EmptyTip, FText(), true, false);
	}

	return AddTrackMenuBuilder.MakeWidget();
}


void FSequencerObjectBindingNode::HandleAddTrackSubMenuNew(FMenuBuilder& AddTrackMenuBuilder, TArray<FPropertyPath> KeyablePropertyPaths, int32 PropertyNameIndexStart)
{
	// [PostProcessSettings] [Bloom1Tint] [X]
	// [PostProcessSettings] [Bloom1Tint] [Y]
	// [PostProcessSettings] [ColorGrading]

	// Create property menu data based on keyable property paths
	TArray<UProperty*> PropertiesTraversed;
	TArray<int32> ArrayIndicesTraversed;
	TArray<PropertyMenuData> KeyablePropertyMenuData;
	for (const FPropertyPath& KeyablePropertyPath : KeyablePropertyPaths)
	{
		PropertyMenuData KeyableMenuData;
		KeyableMenuData.PropertyPath = KeyablePropertyPath;

		// If the path is greater than 1, keep track of the actual properties (not channels) and only add these properties once since we can't do single channel keying of a property yet.
		if (KeyablePropertyPath.GetNumProperties() > 1) //@todo
		{
			const FPropertyInfo& PropertyInfo = KeyablePropertyPath.GetPropertyInfo(1);
			UProperty* Property = PropertyInfo.Property.Get();

			// Search for any array elements
			int32 ArrayIndex = INDEX_NONE;
			for (int32 PropertyInfoIndex = 0; PropertyInfoIndex < KeyablePropertyPath.GetNumProperties(); ++PropertyInfoIndex)
			{
				const FPropertyInfo& ArrayPropertyInfo = KeyablePropertyPath.GetPropertyInfo(PropertyInfoIndex);
				if (ArrayPropertyInfo.ArrayIndex != INDEX_NONE)
				{
					ArrayIndex = ArrayPropertyInfo.ArrayIndex;
					break;
				}
			}

			bool bFound = false;
			for (int32 TraversedIndex = 0; TraversedIndex < PropertiesTraversed.Num(); ++TraversedIndex)
			{
				if (PropertiesTraversed[TraversedIndex] == Property && ArrayIndicesTraversed[TraversedIndex] == ArrayIndex)
				{
					bFound = true;
					break;
				}
			}

			if (bFound)
			{
				continue;
			}

			if (ArrayIndex != INDEX_NONE)
			{
				KeyableMenuData.MenuName = FText::Format(LOCTEXT("ArrayElementFormat", "{0} [{1}]"), Property->GetDisplayNameText(), FText::AsNumber(ArrayIndex)).ToString();
			}
			else
			{
				KeyableMenuData.MenuName = FObjectEditorUtils::GetCategoryFName(Property).ToString();
			}

			PropertiesTraversed.Add(Property);
			ArrayIndicesTraversed.Add(ArrayIndex);
		}
		else
		{
			// No sub menus items, so skip
			continue; 
		}
		KeyablePropertyMenuData.Add(KeyableMenuData);
	}

	// Sort on the menu name
	KeyablePropertyMenuData.Sort([](const PropertyMenuData& A, const PropertyMenuData& B)
	{
		int32 CompareResult = A.MenuName.Compare(B.MenuName);
		return CompareResult < 0;
	});

	// Add menu items
	for (int32 MenuDataIndex = 0; MenuDataIndex < KeyablePropertyMenuData.Num(); )
	{
		TArray<FPropertyPath> KeyableSubMenuPropertyPaths;
		KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);

		for (; MenuDataIndex < KeyablePropertyMenuData.Num()-1; )
		{
			if (KeyablePropertyMenuData[MenuDataIndex].MenuName == KeyablePropertyMenuData[MenuDataIndex+1].MenuName)
			{
				++MenuDataIndex;
				KeyableSubMenuPropertyPaths.Add(KeyablePropertyMenuData[MenuDataIndex].PropertyPath);
			}
			else
			{
				break;
			}
		}

		AddTrackMenuBuilder.AddSubMenu(
			FText::FromString(KeyablePropertyMenuData[MenuDataIndex].MenuName),
			FText::GetEmpty(), 
			FNewMenuDelegate::CreateSP(this, &FSequencerObjectBindingNode::AddPropertyMenuItems, KeyableSubMenuPropertyPaths, PropertyNameIndexStart + 1, PropertyNameIndexStart + 2));

		++MenuDataIndex;
	}
}


void FSequencerObjectBindingNode::HandleLabelsSubMenuCreate(FMenuBuilder& MenuBuilder)
{
	const TSet< TSharedRef<FSequencerDisplayNode> >& SelectedNodes = GetSequencer().GetSelection().GetSelectedOutlinerNodes();
	TArray<FGuid> ObjectBindingIds;
	for (TSharedRef<const FSequencerDisplayNode> SelectedNode : SelectedNodes )
	{
		if (SelectedNode->GetType() == ESequencerNode::Object)
		{
			TSharedRef<const FSequencerObjectBindingNode> ObjectBindingNode = StaticCastSharedRef<const FSequencerObjectBindingNode>(SelectedNode);
			FGuid ObjectBindingId = ObjectBindingNode->GetObjectBinding();
			if (ObjectBindingId.IsValid())
			{
				ObjectBindingIds.Add(ObjectBindingId);
			}
		}
	}

	MenuBuilder.AddWidget(SNew(SSequencerLabelEditor, GetSequencer(), ObjectBindingIds), FText::GetEmpty(), true);
}


void FSequencerObjectBindingNode::HandlePropertyMenuItemExecute(FPropertyPath PropertyPath)
{
	FSequencer& Sequencer = GetSequencer();
	UObject* BoundObject = GetSequencer().FindSpawnedObjectOrTemplate(ObjectBinding);

	TArray<UObject*> KeyableBoundObjects;
	if (BoundObject != nullptr)
	{
		if (Sequencer.CanKeyProperty(FCanKeyPropertyParams(BoundObject->GetClass(), PropertyPath)))
		{
			KeyableBoundObjects.Add(BoundObject);
		}
	}

	// When auto setting track defaults are disabled, force add a key so that the changed
	// value is saved and is propagated to the property.
	FKeyPropertyParams KeyPropertyParams(KeyableBoundObjects, PropertyPath, Sequencer.GetAutoSetTrackDefaults() == false ? ESequencerKeyMode::ManualKeyForced : ESequencerKeyMode::ManualKey);

	Sequencer.KeyProperty(KeyPropertyParams);
}

int32 FSequencerObjectBindingNode::GetSortingOrder() const
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();
	const FMovieSceneBinding* MovieSceneBinding = MovieScene->GetBindings().FindByPredicate([&](FMovieSceneBinding& Binding)
	{
		return Binding.GetObjectGuid() == ObjectBinding;
	});

	if (MovieSceneBinding)
	{
		return MovieSceneBinding->GetSortingOrder();
	}

	return 0;
}

void FSequencerObjectBindingNode::SetSortingOrder(const int32 InSortingOrder)
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();

	FMovieSceneBinding* MovieSceneBinding = (FMovieSceneBinding*) MovieScene->GetBindings().FindByPredicate([&](FMovieSceneBinding& Binding)
	{
		return Binding.GetObjectGuid() == ObjectBinding;
	});

	if (MovieSceneBinding)
	{
		MovieSceneBinding->SetSortingOrder(InSortingOrder);
	}
}

void FSequencerObjectBindingNode::ModifyAndSetSortingOrder(const int32 InSortingOrder)
{
	UMovieScene* MovieScene = GetSequencer().GetFocusedMovieSceneSequence()->GetMovieScene();

	if (MovieScene->IsReadOnly())
	{
		return;
	}

	MovieScene->Modify();
	SetSortingOrder(InSortingOrder);
}

#undef LOCTEXT_NAMESPACE
