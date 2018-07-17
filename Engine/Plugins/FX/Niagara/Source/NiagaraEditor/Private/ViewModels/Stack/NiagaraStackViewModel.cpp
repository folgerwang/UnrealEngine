// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/Stack/NiagaraStackRoot.h"
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraScriptViewModel.h"
#include "NiagaraSystemEditorData.h"
#include "NiagaraEmitterEditorData.h"
#include "NiagaraStackEditorData.h"
#include "NiagaraEmitter.h"
#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "ViewModels/Stack/NiagaraStackItem.h"
#include "ScopedTransaction.h"

#include "Editor.h"

#define LOCTEXT_NAMESPACE "NiagaraStackViewModel"
const double UNiagaraStackViewModel::MaxSearchTime = .02f; // search at 50 fps
TSharedPtr<FNiagaraSystemViewModel> UNiagaraStackViewModel::GetSystemViewModel()
{
	return SystemViewModel;
}

TSharedPtr<FNiagaraEmitterHandleViewModel> UNiagaraStackViewModel::GetEmitterHandleViewModel()
{
	return EmitterHandleViewModel;
}

void UNiagaraStackViewModel::Initialize(TSharedPtr<FNiagaraSystemViewModel> InSystemViewModel, TSharedPtr<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel)
{
	if (RootEntry != nullptr)
	{
		RootEntry->OnStructureChanged().RemoveAll(this);
		RootEntry->OnDataObjectModified().RemoveAll(this);
		RootEntry->OnRequestFullRefresh().RemoveAll(this);
		RootEntry->OnRequestFullRefreshDeferred().RemoveAll(this);
		RootEntries.Empty();
		RootEntry->Finalize();
		RootEntry = nullptr;
		GEditor->UnregisterForUndo(this);
	}

	if (EmitterHandleViewModel.IsValid() && EmitterHandleViewModel->GetEmitterViewModel().IsValid())
	{
		EmitterHandleViewModel->GetEmitterViewModel()->OnScriptCompiled().RemoveAll(this);
	}

	if (SystemViewModel.IsValid())
	{
		SystemViewModel->OnSystemCompiled().RemoveAll(this);
	}

	SystemViewModel = InSystemViewModel;
	EmitterHandleViewModel = InEmitterHandleViewModel;
	TSharedPtr<FNiagaraEmitterViewModel> EmitterViewModel = InEmitterHandleViewModel.IsValid() ? InEmitterHandleViewModel->GetEmitterViewModel() : TSharedPtr<FNiagaraEmitterViewModel>();

	if (SystemViewModel.IsValid() && EmitterViewModel.IsValid() && EmitterViewModel->GetSharedScriptViewModel()->GetGraphViewModel()->GetGraph() != nullptr)
	{
		GEditor->RegisterForUndo(this);

		EmitterViewModel->OnScriptCompiled().AddUObject(this, &UNiagaraStackViewModel::OnEmitterCompiled);
		SystemViewModel->OnSystemCompiled().AddUObject(this, &UNiagaraStackViewModel::OnSystemCompiled);
		
		RootEntry = NewObject<UNiagaraStackRoot>(this);
		UNiagaraStackEntry::FRequiredEntryData RequiredEntryData(SystemViewModel.ToSharedRef(), EmitterViewModel.ToSharedRef(),
			NAME_None, NAME_None,
			SystemViewModel->GetOrCreateEditorData().GetStackEditorData());
		RootEntry->Initialize(RequiredEntryData);
		RootEntry->RefreshChildren();
		RootEntry->OnStructureChanged().AddUObject(this, &UNiagaraStackViewModel::EntryStructureChanged);
		RootEntry->OnDataObjectModified().AddUObject(this, &UNiagaraStackViewModel::EntryDataObjectModified);
		RootEntry->OnRequestFullRefresh().AddUObject(this, &UNiagaraStackViewModel::EntryRequestFullRefresh);
		RootEntry->OnRequestFullRefreshDeferred().AddUObject(this, &UNiagaraStackViewModel::EntryRequestFullRefreshDeferred);
		RootEntries.Add(RootEntry);
	}

	CurrentFocusedSearchMatchIndex = -1;
	StructureChangedDelegate.Broadcast();
	bRestartSearch = false;
	bRefreshPending = false;
}

void UNiagaraStackViewModel::Finalize()
{
	Initialize(nullptr, nullptr);
}

void UNiagaraStackViewModel::BeginDestroy()
{
	checkf(HasAnyFlags(RF_ClassDefaultObject) || (SystemViewModel.IsValid() == false && EmitterHandleViewModel.IsValid() == false), TEXT("Stack view model not finalized."));
	Super::BeginDestroy();
}

void UNiagaraStackViewModel::Tick()
{
	if (RootEntry)
	{
		if (bRefreshPending)
		{
			RootEntry->RefreshChildren();
			OnSearchTextChanged(CurrentSearchText);
			bRefreshPending = false;
		}

		SearchTick();
	}
}

void UNiagaraStackViewModel::OnSearchTextChanged(const FText& SearchText)
{
	if (RootEntry)
	{
		CurrentSearchText = SearchText;
		// postpone searching until next tick; protects against crashes from the GC
		// also this can be triggered by multiple events, so better wait
		bRestartSearch = true;
	}
}

bool UNiagaraStackViewModel::IsSearching()
{
	return ItemsToSearch.Num() > 0;
}

const TArray<UNiagaraStackViewModel::FSearchResult>& UNiagaraStackViewModel::GetCurrentSearchResults()
{
	return CurrentSearchResults;
}

UNiagaraStackEntry* UNiagaraStackViewModel::GetCurrentFocusedEntry()
{
	if (CurrentFocusedSearchMatchIndex >= 0)
	{
		FSearchResult FocusedMatch = CurrentSearchResults[CurrentFocusedSearchMatchIndex];
		return FocusedMatch.GetEntry();
	}
	return nullptr;
}

void UNiagaraStackViewModel::AddSearchScrollOffset(int NumberOfSteps)
{
	CurrentFocusedSearchMatchIndex += NumberOfSteps;
	if (CurrentFocusedSearchMatchIndex >= CurrentSearchResults.Num())
	{
		CurrentFocusedSearchMatchIndex = 0;
	}
	if (CurrentFocusedSearchMatchIndex < 0)
	{
		CurrentFocusedSearchMatchIndex = CurrentSearchResults.Num() - 1;
	}
}

void UNiagaraStackViewModel::CollapseToHeaders()
{
	CollapseToHeadersRecursive(GetRootEntries());
	NotifyStructureChanged();
}

void UNiagaraStackViewModel::UndismissAllIssues()
{
	FScopedTransaction ScopedTransaction(LOCTEXT("UnDismissIssues", "Undismiss issues"));
	UNiagaraStackEditorData& EmitterData = GetEmitterHandleViewModel()->GetEmitterViewModel()->GetEditorData().GetStackEditorData();
	EmitterData.Modify();
	EmitterData.UndismissAllIssues();

	UNiagaraStackEditorData& SystemData = GetSystemViewModel()->GetEditorData().GetStackEditorData();
	SystemData.Modify();
	SystemData.UndismissAllIssues();
	
	RootEntry->RefreshChildren();
}

bool UNiagaraStackViewModel::HasDismissedStackIssues()
{
	return GetSystemViewModel()->GetEditorData().GetStackEditorData().GetDismissedStackIssueIds().Num() > 0
		|| GetEmitterHandleViewModel()->GetEmitterViewModel()->GetEditorData().GetStackEditorData().GetDismissedStackIssueIds().Num() > 0;
}

void UNiagaraStackViewModel::CollapseToHeadersRecursive(TArray<UNiagaraStackEntry*> Entries)
{
	for (UNiagaraStackEntry* Entry : Entries)
	{
		if (Entry->GetCanExpand())
		{
			if (Entry->IsA<UNiagaraStackItemGroup>())
			{
				Entry->SetIsExpanded(true);
			}
			else if (Entry->IsA<UNiagaraStackItem>())
			{
				Entry->SetIsExpanded(false);
			}
		}

		TArray<UNiagaraStackEntry*> Children;
		Entry->GetUnfilteredChildren(Children);
		CollapseToHeadersRecursive(Children);
	}
}

void UNiagaraStackViewModel::GetPathForEntry(UNiagaraStackEntry* Entry, TArray<UNiagaraStackEntry*>& EntryPath)
{
	GeneratePathForEntry(RootEntry, Entry, TArray<UNiagaraStackEntry*>(), EntryPath);
}

void UNiagaraStackViewModel::OnSystemCompiled()
{
	RootEntry->RefreshChildren();
	// when the entries are refreshed, make sure search results are still valid, the stack counts on them
	OnSearchTextChanged(CurrentSearchText);
}

void UNiagaraStackViewModel::OnEmitterCompiled()
{
	RootEntry->RefreshChildren();
	OnSearchTextChanged(CurrentSearchText);
}

void UNiagaraStackViewModel::SearchTick()
{
	// perform partial searches here, by processing a fixed number of entries (maybe more than one?)
	if (bRestartSearch)
	{
		// clear the search results
		CurrentSearchResults.Empty();
		CurrentFocusedSearchMatchIndex = -1;
		// generates ItemsToSearch, these will be processed on tick, in batches
		if (CurrentSearchText.IsEmpty() == false)
		{
			GenerateTraversalEntries(RootEntry, TArray<UNiagaraStackEntry*>(), ItemsToSearch);
		}
		bRestartSearch = false;
	}
	else if (IsSearching())
	{
		double SearchStartTime= FPlatformTime::Seconds();
		double CurrentSearchLoopTime = SearchStartTime;
		// process at least one item, but don't go over MaxSearchTime for the rest
		while (ItemsToSearch.Num() > 0 && CurrentSearchLoopTime - SearchStartTime < MaxSearchTime)
		{
			UNiagaraStackEntry* EntryToProcess = ItemsToSearch[0].GetEntry();
			ensure(EntryToProcess != nullptr); // should never happen so something went wrong if this is hit
			if (EntryToProcess != nullptr)
			{
				TArray<UNiagaraStackEntry::FStackSearchItem> SearchItems;
				EntryToProcess->GetSearchItems(SearchItems);
				TSet<FName> MatchedKeys;
				for (UNiagaraStackEntry::FStackSearchItem SearchItem : SearchItems)
				{
					if (ItemMatchesSearchCriteria(SearchItem)) 
					{
						if (MatchedKeys.Contains(SearchItem.Key) == false)
						{
							CurrentSearchResults.Add({ ItemsToSearch[0].EntryPath, SearchItem });
							MatchedKeys.Add(SearchItem.Key);
						}
					}
				}
			}
			ItemsToSearch.RemoveAt(0); // can't use RemoveAtSwap because we need to preserve the order
			CurrentSearchLoopTime = FPlatformTime::Seconds();
		}
		if (ItemsToSearch.Num() == 0)
		{
			SearchCompletedDelegate.Broadcast();
		}
	}
}

void UNiagaraStackViewModel::GenerateTraversalEntries(UNiagaraStackEntry* Root, TArray<UNiagaraStackEntry*> ParentChain,
	TArray<FSearchWorkItem>& TraversedArray)
{
	TArray<UNiagaraStackEntry*> Children;
	Root->GetFilteredChildren(Children);
	ParentChain.Add(Root);
	TraversedArray.Add(FSearchWorkItem{ParentChain});
	for (auto Child : Children)
	{
		GenerateTraversalEntries(Child, ParentChain, TraversedArray);
	}
}

bool UNiagaraStackViewModel::ItemMatchesSearchCriteria(UNiagaraStackEntry::FStackSearchItem SearchItem)
{
	// this is a simple text compare, we need to replace this with a complex search on future passes
	return SearchItem.Value.ToString().Contains(CurrentSearchText.ToString());
}

void UNiagaraStackViewModel::GeneratePathForEntry(UNiagaraStackEntry* Root, UNiagaraStackEntry* Entry, TArray<UNiagaraStackEntry*> CurrentPath, TArray<UNiagaraStackEntry*>& EntryPath)
{
	if (EntryPath.Num() > 0)
	{
		return;
	}
	TArray<UNiagaraStackEntry*> Children;
	Root->GetUnfilteredChildren(Children);
	CurrentPath.Add(Root);
	for (auto Child : Children)
	{
		if (Child == Entry)
		{
			EntryPath.Append(CurrentPath);
			return;
		}
		GeneratePathForEntry(Child, Entry, CurrentPath, EntryPath);
	}
}

TArray<UNiagaraStackEntry*>& UNiagaraStackViewModel::GetRootEntries()
{
	return RootEntries;
}

UNiagaraStackViewModel::FOnStructureChanged& UNiagaraStackViewModel::OnStructureChanged()
{
	return StructureChangedDelegate;
}

UNiagaraStackViewModel::FOnSearchCompleted& UNiagaraStackViewModel::OnSearchCompleted()
{
	return SearchCompletedDelegate;
}

bool UNiagaraStackViewModel::GetShowAllAdvanced() const
{
	if (SystemViewModel.IsValid() && EmitterHandleViewModel.IsValid())
	{
		return SystemViewModel->GetEditorData().GetStackEditorData().GetShowAllAdvanced() ||
			EmitterHandleViewModel->GetEmitterViewModel()->GetEditorData().GetStackEditorData().GetShowAllAdvanced();
	}
	return false;
}

void UNiagaraStackViewModel::SetShowAllAdvanced(bool bInShowAllAdvanced)
{
	if (SystemViewModel.IsValid() && EmitterHandleViewModel.IsValid())
	{
		SystemViewModel->GetOrCreateEditorData().GetStackEditorData().SetShowAllAdvanced(bInShowAllAdvanced);
		EmitterHandleViewModel->GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData().SetShowAllAdvanced(bInShowAllAdvanced);
		OnSearchTextChanged(CurrentSearchText);
		StructureChangedDelegate.Broadcast();
	}
}

bool UNiagaraStackViewModel::GetShowOutputs() const
{
	if (SystemViewModel.IsValid() && EmitterHandleViewModel.IsValid())
	{
		return SystemViewModel->GetEditorData().GetStackEditorData().GetShowOutputs() ||
			EmitterHandleViewModel->GetEmitterViewModel()->GetEditorData().GetStackEditorData().GetShowOutputs();
	}
	return false;
}

void UNiagaraStackViewModel::SetShowOutputs(bool bInShowOutputs)
{
	if (SystemViewModel.IsValid() && EmitterHandleViewModel.IsValid())
	{
		SystemViewModel->GetOrCreateEditorData().GetStackEditorData().SetShowOutputs(bInShowOutputs);
		EmitterHandleViewModel->GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData().SetShowOutputs(bInShowOutputs);
		OnSearchTextChanged(CurrentSearchText);

		// Showing outputs changes indenting so a full refresh is needed.
		RootEntry->RefreshChildren();
	}
}

double UNiagaraStackViewModel::GetLastScrollPosition() const
{
	if (EmitterHandleViewModel.IsValid())
	{
		return EmitterHandleViewModel->GetEmitterViewModel()->GetEditorData().GetStackEditorData().GetLastScrollPosition();
	}
	return 0;
}

void UNiagaraStackViewModel::SetLastScrollPosition(double InLastScrollPosition)
{
	if (EmitterHandleViewModel.IsValid())
	{
		EmitterHandleViewModel->GetEmitterViewModel()->GetOrCreateEditorData().GetStackEditorData().SetLastScrollPosition(InLastScrollPosition);
	}
}

void UNiagaraStackViewModel::NotifyStructureChanged()
{
	EntryStructureChanged();
}

void UNiagaraStackViewModel::PostUndo(bool bSuccess)
{
	RootEntry->RefreshChildren();
	OnSearchTextChanged(CurrentSearchText);
}

void UNiagaraStackViewModel::EntryStructureChanged()
{
	StructureChangedDelegate.Broadcast();
	OnSearchTextChanged(CurrentSearchText);
}

void UNiagaraStackViewModel::EntryDataObjectModified(UObject* ChangedObject)
{
	SystemViewModel->NotifyDataObjectChanged(ChangedObject);
	OnSearchTextChanged(CurrentSearchText);
}

void UNiagaraStackViewModel::EntryRequestFullRefresh()
{
	checkf(RootEntry != nullptr, TEXT("Can not process full refresh when the root entry doesn't exist"));
	RootEntry->RefreshChildren();
}

void UNiagaraStackViewModel::EntryRequestFullRefreshDeferred()
{
	bRefreshPending = true;
}

#undef LOCTEXT_NAMESPACE
