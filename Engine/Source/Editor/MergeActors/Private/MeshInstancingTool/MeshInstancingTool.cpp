// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "MeshInstancingTool/MeshInstancingTool.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "Misc/PackageName.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/Selection.h"
#include "Editor.h"
#include "Dialogs/Dialogs.h"
#include "MeshUtilities.h"
#include "MeshInstancingTool/SMeshInstancingDialog.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "AssetRegistryModule.h"
#include "ScopedTransaction.h"
#include "MeshMergeModule.h"


#define LOCTEXT_NAMESPACE "MeshInstancingTool"

FMeshInstancingTool::FMeshInstancingTool()
{
	SettingsObject = UMeshInstancingSettingsObject::Get();
}

TSharedRef<SWidget> FMeshInstancingTool::GetWidget()
{
	SAssignNew(InstancingDialog, SMeshInstancingDialog, this);
	return InstancingDialog.ToSharedRef();
}

FText FMeshInstancingTool::GetTooltipText() const
{
	return LOCTEXT("MeshInstancingToolTooltip", "Harvest geometry from selected actors and merge them into an actor with multiple instanced static mesh components.");
}

FString FMeshInstancingTool::GetDefaultPackageName() const
{
	return FString();
}

bool FMeshInstancingTool::RunMerge(const FString& PackageName)
{
	const IMeshMergeUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
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

	// This restriction is only for replacement of selected actors with merged mesh actor
	if (UniqueLevels.Num() > 1)
	{
		FText Message = NSLOCTEXT("UnrealEd", "FailedToInstanceActorsSublevels_Msg", "The selected actors should be in the same level");
		OpenMsgDlgInt(EAppMsgType::Ok, Message, NSLOCTEXT("UnrealEd", "FailedToInstanceActors_Title", "Unable to replace actors with instanced meshes"));
		return false;
	}

	// Instance...
	{
		FScopedSlowTask SlowTask(0, LOCTEXT("MergingActorsSlowTask", "Instancing actors..."));
		SlowTask.MakeDialog();

		// Extracting static mesh components from the selected mesh components in the dialog
		const TArray<TSharedPtr<FInstanceComponentData>>& SelectedComponents = InstancingDialog->GetSelectedComponents();
		TArray<UPrimitiveComponent*> ComponentsToMerge;

		for ( const TSharedPtr<FInstanceComponentData>& SelectedComponent : SelectedComponents)
		{
			// Determine whether or not this component should be incorporated according the user settings
			if (SelectedComponent->bShouldIncorporate && SelectedComponent->PrimComponent.IsValid())
			{
				ComponentsToMerge.Add(SelectedComponent->PrimComponent.Get());
			}
		}

		if (ComponentsToMerge.Num())
		{
			// spawn the actor that will contain out instances
			UWorld* World = ComponentsToMerge[0]->GetWorld();
			checkf(World != nullptr, TEXT("Invalid World retrieved from Mesh components"));
			MeshUtilities.MergeComponentsToInstances(ComponentsToMerge, World, UniqueLevels[0], SettingsObject->Settings);
		}
	}

	InstancingDialog->Reset();

	return true;
}

FText FMeshInstancingTool::GetPredictedResultsText()
{
	const IMeshMergeUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshMergeModule>("MeshMergeUtilities").GetUtilities();
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

	// This restriction is only for replacement of selected actors with merged mesh actor
	if (UniqueLevels.Num() > 1)
	{
		return NSLOCTEXT("UnrealEd", "FailedToInstanceActorsSublevels_Msg", "The selected actors should be in the same level");
	}

	// Extracting static mesh components from the selected mesh components in the dialog
	const TArray<TSharedPtr<FInstanceComponentData>>& SelectedComponents = InstancingDialog->GetSelectedComponents();
	TArray<UPrimitiveComponent*> ComponentsToMerge;

	for ( const TSharedPtr<FInstanceComponentData>& SelectedComponent : SelectedComponents)
	{
		// Determine whether or not this component should be incorporated according the user settings
		if (SelectedComponent->bShouldIncorporate)
		{
			ComponentsToMerge.Add(SelectedComponent->PrimComponent.Get());
		}
	}
		
	FText OutResultsText;
	if(ComponentsToMerge.Num() > 0)
	{
		UWorld* World = ComponentsToMerge[0]->GetWorld();
		checkf(World != nullptr, TEXT("Invalid World retrieved from Mesh components"));
		MeshUtilities.MergeComponentsToInstances(ComponentsToMerge, World, UniqueLevels[0], SettingsObject->Settings, false, &OutResultsText);
	}
	else
	{
		OutResultsText = LOCTEXT("InstanceMergePredictedResultsNone", "The current settings will not result in any instanced meshes being created");
	}

	return OutResultsText;
}

bool FMeshInstancingTool::CanMerge() const
{	
	return InstancingDialog->GetNumSelectedMeshComponents() >= 1;
}

#undef LOCTEXT_NAMESPACE
