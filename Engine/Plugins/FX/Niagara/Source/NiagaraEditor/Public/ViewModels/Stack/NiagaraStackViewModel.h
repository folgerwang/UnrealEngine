// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"
#include "EditorUndoClient.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "UObject/ObjectKey.h"
#include "NiagaraStackViewModel.generated.h"

class FNiagaraSystemViewModel;
class FNiagaraEmitterHandleViewModel;
class UNiagaraStackEntry;
class UNiagaraStackRoot;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackViewModel : public UObject, public FEditorUndoClient
{
	GENERATED_BODY()

public:
	DECLARE_MULTICAST_DELEGATE(FOnStructureChanged);
	DECLARE_MULTICAST_DELEGATE(FOnSearchCompleted);
public:
	struct FSearchResult
	{
		TArray<UNiagaraStackEntry*> EntryPath;
		UNiagaraStackEntry::FStackSearchItem MatchingItem;
		UNiagaraStackEntry* GetEntry()
		{
			return EntryPath.Num() > 0 ? 
				EntryPath[EntryPath.Num() - 1] : 
				nullptr;
		}
	};
public:
	TSharedPtr<FNiagaraEmitterHandleViewModel> GetEmitterHandleViewModel();
	TSharedPtr<FNiagaraSystemViewModel> GetSystemViewModel();
	void Initialize(TSharedPtr<FNiagaraSystemViewModel> InSystemViewModel, TSharedPtr<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel);

	void Finalize();

	virtual void BeginDestroy() override;

	TArray<UNiagaraStackEntry*>& GetRootEntries();

	FOnStructureChanged& OnStructureChanged();
	FOnSearchCompleted& OnSearchCompleted();

	bool GetShowAllAdvanced() const;
	void SetShowAllAdvanced(bool bInShowAllAdvanced);

	bool GetShowOutputs() const;
	void SetShowOutputs(bool bInShowOutputs);

	double GetLastScrollPosition() const;
	void SetLastScrollPosition(double InLastScrollPosition);

	void NotifyStructureChanged();

	//~ FEditorUndoClient Interface
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

	virtual void Tick();
	//~ stack search stuff
	void OnSearchTextChanged(const FText& SearchText);
	FText GetCurrentSearchText() const { return CurrentSearchText; };
	bool IsSearching();
	const TArray<FSearchResult>& GetCurrentSearchResults();
	int GetCurrentFocusedMatchIndex() const { return CurrentFocusedSearchMatchIndex; }
	UNiagaraStackEntry* GetCurrentFocusedEntry();
	void AddSearchScrollOffset(int NumberOfSteps);

	void GetPathForEntry(UNiagaraStackEntry* Entry, TArray<UNiagaraStackEntry*>& EntryPath);

	/** Starts recursing through all entries to expand all groups and collapse all items. */
	void CollapseToHeaders();
	
	void UndismissAllIssues();

	bool HasDismissedStackIssues();

private:

	/** Recursively Expands all groups and collapses all items in the stack. */
	void CollapseToHeadersRecursive(TArray<UNiagaraStackEntry*> Entries);

	struct FSearchWorkItem
	{
		TArray<UNiagaraStackEntry*> EntryPath;
		UNiagaraStackEntry* GetEntry()
		{
			return EntryPath.Num() > 0 ?
				EntryPath[EntryPath.Num() - 1] :
				nullptr;
		}
	};

private:
	void EntryStructureChanged();
	void EntryDataObjectModified(UObject* ChangedObject);
	void EntryRequestFullRefresh();
	void EntryRequestFullRefreshDeferred();
	void OnSystemCompiled();
	void OnEmitterCompiled();
	/** Called by the tick function to perform partial search */
	void SearchTick();
	void GenerateTraversalEntries(UNiagaraStackEntry* Root, TArray<UNiagaraStackEntry*> ParentChain, 
		TArray<FSearchWorkItem>& TraversedArray);
	bool ItemMatchesSearchCriteria(UNiagaraStackEntry::FStackSearchItem SearchItem);
	void GeneratePathForEntry(UNiagaraStackEntry* Root, UNiagaraStackEntry* Entry, TArray<UNiagaraStackEntry*> CurrentPath, TArray<UNiagaraStackEntry*>& EntryPath);

private:
	TSharedPtr<FNiagaraEmitterHandleViewModel> EmitterHandleViewModel;
	TSharedPtr<FNiagaraSystemViewModel> SystemViewModel;
	
	TArray<UNiagaraStackEntry*> RootEntries;

	UPROPERTY()
	UNiagaraStackRoot* RootEntry;

	FOnStructureChanged StructureChangedDelegate;

	// ~Search stuff
	FText CurrentSearchText;
	int CurrentFocusedSearchMatchIndex;
	FOnSearchCompleted SearchCompletedDelegate;
	TArray<FSearchWorkItem> ItemsToSearch;
	TArray<FSearchResult> CurrentSearchResults;
	static const double MaxSearchTime;
	bool bRestartSearch;
	bool bRefreshPending;
};