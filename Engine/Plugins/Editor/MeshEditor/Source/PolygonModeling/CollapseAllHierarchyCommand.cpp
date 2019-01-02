// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "CollapseAllHierarchyCommand.h"
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


#define LOCTEXT_NAMESPACE "CollapseAllHierarchyCommand"

DEFINE_LOG_CATEGORY(LogCollapseAllHierarchyCommand);


void UCollapseAllHierarchyCommand::RegisterUICommand( FBindingContext* BindingContext )
{
	UI_COMMAND_EXT( BindingContext, /* Out */ UICommandInfo, "FlattenHierarchy", "Flatten Hierarchy", "Performs flattening of entire hierarchy at given view level. When viewing 'All Levels' it will collapse all nodes be flat under the root.", EUserInterfaceActionType::Button, FInputChord() );
}

void UCollapseAllHierarchyCommand::Execute(IMeshEditorModeEditingContract& MeshEditorMode)
{
	if (MeshEditorMode.GetActiveAction() != NAME_None)
	{
		return;
	}

	if (MeshEditorMode.GetSelectedEditableMeshes().Num() == 0)
	{
		return;
	}

	FScopedTransaction Transaction(LOCTEXT("Collapse All Hierarchy", "Collapse All Hierarchy"));

	MeshEditorMode.CommitSelectedMeshes();

	TArray<UEditableMesh*> SelectedActors = MeshEditorMode.GetSelectedEditableMeshes();

	CollapseHierarchies(MeshEditorMode, SelectedActors);

	UpdateExplodedView(MeshEditorMode, EViewResetType::RESET_TRANSFORMS);
}

void UCollapseAllHierarchyCommand::CollapseHierarchies(IMeshEditorModeEditingContract& MeshEditorMode, TArray<UEditableMesh*>& SelectedMeshes)
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
						AddSingleRootNodeIfRequired(GeometryCollectionObject);

						//UE_LOG(LogCollapseAllHierarchyCommand, Log, TEXT("Hierarchy Before Collapsing ..."));
						//LogHierarchy(GeometryCollectionObject);
	
						TManagedArray<FGeometryCollectionBoneNode>& Hierarchy = *GeometryCollection->BoneHierarchy;

						TArray<int32> Elements;

						for (int Element = 0; Element < Hierarchy.Num(); Element++)
						{
							const FGeometryCollectionBoneNode& Node = Hierarchy[Element];
							if (Node.IsGeometry())
							{
								Elements.Add(Element);
							}
						}

						if (Elements.Num() > 0)
						{
							FGeometryCollectionClusteringUtility::ClusterBonesUnderExistingRoot(GeometryCollection, Elements);
						}

						FScopedColorEdit EditBoneColor = GeometryCollectionComponent->EditBoneSelection();
						EditBoneColor.ResetBoneSelection();

						//UE_LOG(LogCollapseAllHierarchyCommand, Log, TEXT("Hierarchy After Collapsing ..."));
						//LogHierarchy(GeometryCollectionObject);

						GeometryCollectionComponent->MarkRenderDynamicDataDirty();
						GeometryCollectionComponent->MarkRenderStateDirty();
					}
				}
			}
		}

}

#undef LOCTEXT_NAMESPACE
