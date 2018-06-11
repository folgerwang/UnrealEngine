// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "NiagaraMetaDataCollectionViewModel.h"
#include "NiagaraTypes.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSourceBase.h"
#include "NiagaraScriptSource.h"
#include "NiagaraGraph.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraConstants.h"
#include "NiagaraMetaDataViewModel.h"
#include "GraphEditAction.h"
#include "EdGraphSchema_Niagara.h"

#define LOCTEXT_NAMESPACE "NiagaraMetaDataCollectionViewModel"

FNiagaraMetaDataCollectionViewModel::FNiagaraMetaDataCollectionViewModel()
	: ModuleGraph(nullptr)
	, bNeedsRefresh(false)
	, bInternalGraphChange(false)
{
}

void FNiagaraMetaDataCollectionViewModel::Tick(float DeltaTime)
{
	if (bNeedsRefresh)
	{
		Refresh();
		bNeedsRefresh = false;
	}
}

bool FNiagaraMetaDataCollectionViewModel::IsTickable() const
{
	return true;
}

TStatId FNiagaraMetaDataCollectionViewModel::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FNiagaraMetaDataCollectionViewModel, STATGROUP_Tickables);
}

void FNiagaraMetaDataCollectionViewModel::SetGraph(UNiagaraGraph* InGraph)
{
	if (ModuleGraph != nullptr)
	{
		Cleanup();
	}

	if (InGraph == nullptr)
	{
		return;
	}
	// now build variables
	ModuleGraph = InGraph;
	
	Refresh();

	OnGraphChangedHandle = ModuleGraph->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateRaw(this, &FNiagaraMetaDataCollectionViewModel::OnGraphChanged));
	OnRecompileHandle = ModuleGraph->AddOnGraphNeedsRecompileHandler(FOnGraphChanged::FDelegate::CreateRaw(this, &FNiagaraMetaDataCollectionViewModel::OnGraphChanged));
}

TSharedPtr<FNiagaraMetaDataViewModel> FNiagaraMetaDataCollectionViewModel::GetMetadataViewModelForVariable(FNiagaraVariable InVariable)
{
	for (TSharedPtr<FNiagaraMetaDataViewModel> Model : MetaDataViewModels)
	{
		if (InVariable == Model->GetVariable())
			return Model;
	}
	return TSharedPtr<FNiagaraMetaDataViewModel>();
}

TArray<TSharedRef<FNiagaraMetaDataViewModel>>& FNiagaraMetaDataCollectionViewModel::GetVariableModels()
{
	return MetaDataViewModels;
}

void FNiagaraMetaDataCollectionViewModel::RequestRefresh()
{
	bNeedsRefresh = true;
}

void FNiagaraMetaDataCollectionViewModel::OnGraphChanged(const struct FEdGraphEditAction& InAction)
{
	if (!bInternalGraphChange)
	{
		RequestRefresh();
	}
} 

void FNiagaraMetaDataCollectionViewModel::ChildMetadataChanged()
{
	TGuardValue<bool> UpdateGuard(bInternalGraphChange, true);
	OnCollectionChangedDelegate.Broadcast();
	ModuleGraph->NotifyGraphNeedsRecompile();
}

void FNiagaraMetaDataCollectionViewModel::Refresh()
{
	if (!ModuleGraph)
	{
		return;
	}

	// Get the variable metadata from the graph
	CleanupMetadata();
	for (auto& MetadataElement : ModuleGraph->GetAllMetaData())
	{
		TSharedPtr<FNiagaraMetaDataViewModel> MetadataViewModel = GetMetadataViewModelForVariable(MetadataElement.Key);
		if (!MetadataViewModel.IsValid())
		{
			MetadataViewModel = MakeShared<FNiagaraMetaDataViewModel>(MetadataElement.Key, *ModuleGraph);
			MetadataViewModel->OnMetadataChanged().AddRaw(this, &FNiagaraMetaDataCollectionViewModel::ChildMetadataChanged);
			MetaDataViewModels.Add(MetadataViewModel.ToSharedRef());
		}

		FNiagaraVariableMetaData& Metadata = MetadataElement.Value;
		if (Metadata.ReferencerNodes.Num() > 0)
		{
			UNiagaraNode* Node = Cast<UNiagaraNode>(Metadata.ReferencerNodes[0].Get());
			if (Node)
			{
				MetadataViewModel->AssociateNode(Node);
			}
		}
	}

	SortViewModels();

	OnCollectionChangedDelegate.Broadcast();
}

void FNiagaraMetaDataCollectionViewModel::SortViewModels()
{
	TMap<FString, int32> CategoryPriorityMap;
	for (auto Metadata : MetaDataViewModels)
	{
		Metadata->RefreshMetaDataValue();
		FString CategoryName = Metadata->GetGraphMetaData()->CategoryName.ToString();
		if (!CategoryName.IsEmpty() && 
			(!CategoryPriorityMap.Contains(CategoryName) || (CategoryPriorityMap.Contains(CategoryName) && CategoryPriorityMap[CategoryName] > Metadata->GetEditorSortPriority())))
		{
			CategoryPriorityMap.Add(CategoryName, Metadata->GetEditorSortPriority());
		}
	}
	auto SortVars = [&](const TSharedRef<FNiagaraMetaDataViewModel>& A, const TSharedRef<FNiagaraMetaDataViewModel>& B)
	{
		int32 CategoryPriorityA = CategoryPriorityMap.Contains(A->GetGraphMetaData()->CategoryName.ToString()) ? CategoryPriorityMap[A->GetGraphMetaData()->CategoryName.ToString()] : MIN_int32;
		int32 CategoryPriorityB = CategoryPriorityMap.Contains(B->GetGraphMetaData()->CategoryName.ToString()) ? CategoryPriorityMap[B->GetGraphMetaData()->CategoryName.ToString()] : MIN_int32;
		if (CategoryPriorityA < CategoryPriorityB)
		{
			return true;
		}
		else if (CategoryPriorityA > CategoryPriorityB)
		{
			return false;
		}
		if (A->GetEditorSortPriority() < B->GetEditorSortPriority())
		{
			return true;
		}
		else if (A->GetEditorSortPriority() > B->GetEditorSortPriority())
		{
			return false;
		}
		//If equal priority, sort alphabetically.
		return A->GetName().ToString() < B->GetName().ToString();
	};
	MetaDataViewModels.Sort(SortVars);
}

void FNiagaraMetaDataCollectionViewModel::Cleanup()
{
	CleanupMetadata();
	ModuleGraph->RemoveOnGraphChangedHandler(OnGraphChangedHandle);
	ModuleGraph->RemoveOnGraphNeedsRecompileHandler(OnRecompileHandle);
}

void FNiagaraMetaDataCollectionViewModel::CleanupMetadata()
{
	for (int32 i = 0; i < MetaDataViewModels.Num(); i++)
	{
		MetaDataViewModels[i]->OnMetadataChanged().RemoveAll(this);
	}
	MetaDataViewModels.Empty();
}

FNiagaraMetaDataCollectionViewModel::~FNiagaraMetaDataCollectionViewModel()
{
	Cleanup();
}

#undef LOCTEXT_NAMESPACE // NiagaraMetaDataCollectionViewModel
