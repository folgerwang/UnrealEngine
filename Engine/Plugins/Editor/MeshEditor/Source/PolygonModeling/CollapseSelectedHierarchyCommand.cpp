// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CollapseSelectedHierarchyCommand.h"
#include "IMeshEditorModeEditingContract.h"
#include "ScopedTransaction.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "MeshAttributes.h"
#include "MeshDescription.h"
#include "PackageTools.h"
#include "MeshFractureSettings.h"
#include "EditableMeshFactory.h"
#include "GeometryCollection/GeometryCollection.h"
#include "GeometryCollection/GeometryCollectionActor.h"
#include "GeometryCollection/GeometryCollectionComponent.h"
#include "GeometryCollection/GeometryCollectionObject.h"
#include "EditorSupportDelegates.h"
#include "GeometryCollection/GeometryCollectionConversion.h"
#include "GeometryCollection/GeometryCollectionClusteringUtility.h"


#define LOCTEXT_NAMESPACE "CollapseSelectedHierarchyCommand"

DEFINE_LOG_CATEGORY(LogCollapseSelectedHierarchyCommand);


void UCollapseSelectedHierarchyCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "CollapseSelectedHierarchy", "Uncluster", "Performs collpase of hierarchy at selected nodes.", EUserInterfaceActionType::Button, FInputChord() );
}

void UCollapseSelectedHierarchyCommand::Execute(IMeshEditorModeEditingContract& MeshEditorMode)
{
	if (MeshEditorMode.GetActiveAction() != NAME_None)
	{
		return;
	}

	if (MeshEditorMode.GetSelectedEditableMeshes().Num() == 0)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("CollapseSelectedHierarchy", "Uncluster"));

	MeshEditorMode.CommitSelectedMeshes();

	TArray<UEditableMesh*> SelectedActors = MeshEditorMode.GetSelectedEditableMeshes();

	CollapseHierarchies(MeshEditorMode, SelectedActors);

	UpdateExplodedView(MeshEditorMode, EViewResetType::RESET_TRANSFORMS);
}

void UCollapseSelectedHierarchyCommand::CollapseHierarchies(IMeshEditorModeEditingContract& MeshEditorMode, TArray<UEditableMesh*>& SelectedMeshes)
{
	for (UEditableMesh* EditableMesh : SelectedMeshes)
	{
		UGeometryCollectionComponent* GeometryCollectionComponent = GetGeometryCollectionComponent(EditableMesh);
		if (GeometryCollectionComponent != nullptr)
		{
			// scoped edit of collection
			FGeometryCollectionEdit GeometryCollectionEdit = GeometryCollectionComponent->EditRestCollection();
			if (UGeometryCollection* GeometryCollectionObject = GeometryCollectionEdit.GetRestCollection())
			{
				TSharedPtr<FGeometryCollection> GeometryCollectionPtr = GeometryCollectionObject->GetGeometryCollection();
				if (FGeometryCollection* GeometryCollection = GeometryCollectionPtr.Get())
				{

					AddAdditionalAttributesIfRequired(GeometryCollectionObject);

					UE_LOG(LogCollapseSelectedHierarchyCommand, Log, TEXT("Hierarchy Before Collapsing ..."));
					LogHierarchy(GeometryCollectionObject);

					const UMeshFractureSettings* FratureSettings = MeshEditorMode.GetFractureSettings();
					int8 FractureLevel = FratureSettings->CommonSettings->GetFractureLevelNumber();
					FGeometryCollectionClusteringUtility::CollapseSelectedHierarchy(FractureLevel, GeometryCollectionComponent->GetSelectedBones(), GeometryCollection);
					FScopedColorEdit EditBoneColor = GeometryCollectionComponent->EditBoneSelection();
					EditBoneColor.ResetBoneSelection();

					UE_LOG(LogCollapseSelectedHierarchyCommand, Log, TEXT("Hierarchy After Collapsing ..."));
					LogHierarchy(GeometryCollectionObject);

					GeometryCollectionComponent->MarkRenderDynamicDataDirty();
					GeometryCollectionComponent->MarkRenderStateDirty();
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
