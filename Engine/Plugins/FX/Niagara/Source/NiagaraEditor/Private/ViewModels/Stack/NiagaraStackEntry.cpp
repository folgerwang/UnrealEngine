// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "ViewModels/Stack/NiagaraStackErrorItem.h"
#include "NiagaraStackEditorData.h"

const FName UNiagaraStackEntry::FExecutionCategoryNames::System = TEXT("System");
const FName UNiagaraStackEntry::FExecutionCategoryNames::Emitter = TEXT("Emitter");
const FName UNiagaraStackEntry::FExecutionCategoryNames::Particle = TEXT("Particle");
const FName UNiagaraStackEntry::FExecutionCategoryNames::Render = TEXT("Render");

const FName UNiagaraStackEntry::FExecutionSubcategoryNames::Parameters = TEXT("Parameters");
const FName UNiagaraStackEntry::FExecutionSubcategoryNames::Spawn = TEXT("Spawn");
const FName UNiagaraStackEntry::FExecutionSubcategoryNames::Update = TEXT("Update");
const FName UNiagaraStackEntry::FExecutionSubcategoryNames::Event = TEXT("Event");

UNiagaraStackEntry::UNiagaraStackEntry()
	: IndentLevel(0)
{
}

void UNiagaraStackEntry::Initialize(FRequiredEntryData InRequiredEntryData, FString InStackEditorDataKey)
{
	SystemViewModel = InRequiredEntryData.SystemViewModel;
	EmitterViewModel = InRequiredEntryData.EmitterViewModel;
	ExecutionCategoryName = InRequiredEntryData.ExecutionCategoryName;
	ExecutionSubcategoryName = InRequiredEntryData.ExecutionSubcategoryName;
	StackEditorData = InRequiredEntryData.StackEditorData;
	StackEditorDataKey = InStackEditorDataKey;
}

bool UNiagaraStackEntry::IsValid() const
{
	return SystemViewModel.IsValid();
}

FText UNiagaraStackEntry::GetDisplayName() const
{
	return FText();
}

UNiagaraStackEditorData& UNiagaraStackEntry::GetStackEditorData() const
{
	return *StackEditorData;
}

FString UNiagaraStackEntry::GetStackEditorDataKey() const
{
	return StackEditorDataKey;
}

FText UNiagaraStackEntry::GetTooltipText() const
{
	return FText();
}

bool UNiagaraStackEntry::GetCanExpand() const
{
	return true;
}

bool UNiagaraStackEntry::IsExpandedByDefault() const
{
	return true;
}

bool UNiagaraStackEntry::GetIsExpanded() const
{
	if (GetShouldShowInStack() == false || GetCanExpand() == false)
	{
		// Entries that aren't visible, or can't expand are always expanded.
		return true;
	}

	if (bIsExpandedCache.IsSet() == false)
	{
		bIsExpandedCache = StackEditorData->GetStackEntryIsExpanded(GetStackEditorDataKey(), IsExpandedByDefault());
	}
	return bIsExpandedCache.GetValue();
}

void UNiagaraStackEntry::SetIsExpanded(bool bInExpanded)
{
	StackEditorData->SetStackEntryIsExpanded(GetStackEditorDataKey(), bInExpanded);
	bIsExpandedCache.Reset();
}

bool UNiagaraStackEntry::GetIsEnabled() const
{
	return true;
}

FName UNiagaraStackEntry::GetExecutionCategoryName() const
{
	return ExecutionCategoryName;
}

FName UNiagaraStackEntry::GetExecutionSubcategoryName() const
{
	return ExecutionSubcategoryName;
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackEntry::GetStackRowStyle() const
{
	return EStackRowStyle::None;
}

bool UNiagaraStackEntry::GetShouldShowInStack() const
{
	return true;
}

void UNiagaraStackEntry::GetFilteredChildren(TArray<UNiagaraStackEntry*>& OutFilteredChildren)
{
	OutFilteredChildren.Append(ErrorChildren);
	for (UNiagaraStackEntry* Child : Children)
	{
		bool bPassesFilter = true;
		for(const FOnFilterChild& ChildFilter : ChildFilters)
		{ 
			if (ChildFilter.Execute(*Child) == false)
			{
				bPassesFilter = false;
				break;
			}
		}

		if (bPassesFilter)
		{
			OutFilteredChildren.Add(Child);
		}
	}
}

void UNiagaraStackEntry::GetUnfilteredChildren(TArray<UNiagaraStackEntry*>& OutUnfilteredChildren)
{
	OutUnfilteredChildren.Append(ErrorChildren);
	OutUnfilteredChildren.Append(Children);
}

void UNiagaraStackEntry::GetUnfilteredChildren(TArray<UNiagaraStackEntry*>& OutUnfilteredChildren) const
{
	OutUnfilteredChildren.Append(ErrorChildren);
	OutUnfilteredChildren.Append(Children);
}

FDelegateHandle UNiagaraStackEntry::AddChildFilter(FOnFilterChild ChildFilter)
{
	ChildFilters.Add(ChildFilter);
	StructureChangedDelegate.Broadcast();
	return ChildFilters.Last().GetHandle();
}

void UNiagaraStackEntry::RemoveChildFilter(FDelegateHandle FilterHandle)
{
	ChildFilters.RemoveAll([=](const FOnFilterChild& ChildFilter) { return ChildFilter.GetHandle() == FilterHandle; });
	StructureChangedDelegate.Broadcast();
}

TSharedRef<FNiagaraSystemViewModel> UNiagaraStackEntry::GetSystemViewModel() const
{
	TSharedPtr<FNiagaraSystemViewModel> PinnedSystemViewModel = SystemViewModel.Pin();
	checkf(PinnedSystemViewModel.IsValid(), TEXT("Base stack entry not initialized or system view model was already deleted."));
	return PinnedSystemViewModel.ToSharedRef();
}

TSharedRef<FNiagaraEmitterViewModel> UNiagaraStackEntry::GetEmitterViewModel() const
{
	TSharedPtr<FNiagaraEmitterViewModel> PinnedEmitterViewModel = EmitterViewModel.Pin();
	checkf(PinnedEmitterViewModel.IsValid(), TEXT("Base stack entry not initialized or emitter view model was already deleted."));
	return PinnedEmitterViewModel.ToSharedRef();
}

UNiagaraStackEntry::FOnStructureChanged& UNiagaraStackEntry::OnStructureChanged()
{
	return StructureChangedDelegate;
}

UNiagaraStackEntry::FOnDataObjectModified& UNiagaraStackEntry::OnDataObjectModified()
{
	return DataObjectModifiedDelegate;
}

int32 UNiagaraStackEntry::GetIndentLevel() const
{
	return IndentLevel;
}

void UNiagaraStackEntry::GetSearchItems(TArray<UNiagaraStackEntry::FStackSearchItem>& SearchItems) const
{
	SearchItems.Add({FName("DisplayName"), GetDisplayName()}); 
	GetAdditionalSearchItemsInternal(SearchItems);
}

UObject* UNiagaraStackEntry::GetExternalAsset() const
{
	return nullptr;
}

bool UNiagaraStackEntry::CanDrag() const
{
	return false;
}

TOptional<UNiagaraStackEntry::FDropResult> UNiagaraStackEntry::CanDrop(const TArray<UNiagaraStackEntry*>& DraggedEntries)
{
	TOptional<FDropResult> Result = CanDropInternal(DraggedEntries);
	if (Result.IsSet())
	{
		return Result;
	}
	else
	{
		return OnRequestCanDropDelegate.IsBound() 
			? OnRequestCanDropDelegate.Execute(*this, DraggedEntries)
			: TOptional<FDropResult>();
	}
}

TOptional<UNiagaraStackEntry::FDropResult> UNiagaraStackEntry::Drop(const TArray<UNiagaraStackEntry*>& DraggedEntries)
{
	TOptional<FDropResult> Result = DropInternal(DraggedEntries);
	if (Result.IsSet())
	{
		return Result;
	}
	else
	{
		return OnRequestDropDelegate.IsBound()
			? OnRequestDropDelegate.Execute(*this, DraggedEntries)
			: TOptional<FDropResult>();
	}
}

void UNiagaraStackEntry::SetOnRequestCanDrop(FOnRequestDrop InOnRequestCanDrop)
{
	OnRequestCanDropDelegate = InOnRequestCanDrop;
}

void UNiagaraStackEntry::SetOnRequestDrop(FOnRequestDrop InOnRequestDrop)
{
	OnRequestDropDelegate = InOnRequestDrop;
}

void UNiagaraStackEntry::GetAdditionalSearchItemsInternal(TArray<FStackSearchItem>& SearchItems) const
{
}

TOptional<UNiagaraStackEntry::FDropResult> UNiagaraStackEntry::CanDropInternal(const TArray<UNiagaraStackEntry*>& DraggedEntries)
{
	return TOptional<FDropResult>();
}

TOptional<UNiagaraStackEntry::FDropResult> UNiagaraStackEntry::DropInternal(const TArray<UNiagaraStackEntry*>& DraggedEntries)
{
	return TOptional<FDropResult>();
}

TOptional<UNiagaraStackEntry::FDropResult> UNiagaraStackEntry::ChildRequestCanDropInternal(const UNiagaraStackEntry& TargetChild, const TArray<UNiagaraStackEntry*>& DraggedEntries)
{
	return TOptional<FDropResult>();
}

TOptional<UNiagaraStackEntry::FDropResult> UNiagaraStackEntry::ChildRequestDropInternal(const UNiagaraStackEntry& TargetChild, const TArray<UNiagaraStackEntry*>& DraggedEntries)
{
	return TOptional<FDropResult>();
}

void UNiagaraStackEntry::ChlildStructureChangedInternal()
{
}

void UNiagaraStackEntry::RefreshChildren()
{
	checkf(SystemViewModel.IsValid() && EmitterViewModel.IsValid(), TEXT("Base stack entry not initialized."));

	for (UNiagaraStackEntry* Child : Children)
	{
		Child->OnStructureChanged().RemoveAll(this);
		Child->OnDataObjectModified().RemoveAll(this);
		Child->SetOnRequestCanDrop(FOnRequestDrop());
		Child->SetOnRequestDrop(FOnRequestDrop());
	}
	for (UNiagaraStackErrorItem* ErrorChild : ErrorChildren)
	{
		ErrorChild->OnStructureChanged().RemoveAll(this);
		ErrorChild->OnDataObjectModified().RemoveAll(this);
		ErrorChild->OnIssueModified().RemoveAll(this);
	}

	TArray<UNiagaraStackEntry*> NewChildren;
	TArray<FStackIssue> NewStackIssues;
	RefreshChildrenInternal(Children, NewChildren, NewStackIssues);
	Children.Empty();
	Children.Append(NewChildren);

	for (UNiagaraStackEntry* Child : Children)
	{
		Child->IndentLevel = GetChildIndentLevel();
		Child->RefreshChildren();
		Child->OnStructureChanged().AddUObject(this, &UNiagaraStackEntry::ChildStructureChanged);
		Child->OnDataObjectModified().AddUObject(this, &UNiagaraStackEntry::ChildDataObjectModified);
		Child->SetOnRequestCanDrop(FOnRequestDrop::CreateUObject(this, &UNiagaraStackEntry::ChildRequestCanDrop));
		Child->SetOnRequestDrop(FOnRequestDrop::CreateUObject(this, &UNiagaraStackEntry::ChildRequestDrop));
	}
	
	// Stack issues refresh
	NewStackIssues.RemoveAll([=](const FStackIssue& Issue) { return GetStackEditorData().GetDismissedStackIssueIds().Contains(Issue.UniqueIdentifier); }); 

	StackIssues.Empty();
	StackIssues.Append(NewStackIssues);
	RefreshStackErrorChildren();
	for (UNiagaraStackErrorItem* ErrorChild : ErrorChildren)
	{
		ErrorChild->IndentLevel = GetChildIndentLevel();
		ErrorChild->RefreshChildren();
		ErrorChild->OnStructureChanged().AddUObject(this, &UNiagaraStackEntry::ChildStructureChanged);
		ErrorChild->OnDataObjectModified().AddUObject(this, &UNiagaraStackEntry::ChildDataObjectModified);
		ErrorChild->OnIssueModified().AddUObject(this, &UNiagaraStackEntry::IssueModified);
	}

	PostRefreshChildrenInternal();

	StructureChangedDelegate.Broadcast();
}

void UNiagaraStackEntry::RefreshStackErrorChildren()
{
    // keep the error entries that are already built
	TArray<UNiagaraStackErrorItem*> NewErrorChildren;
	for (FStackIssue Issue : StackIssues)
	{
		UNiagaraStackErrorItem* ErrorEntry = nullptr;
		UNiagaraStackErrorItem** Found = ErrorChildren.FindByPredicate(
			[&](UNiagaraStackErrorItem* CurrentChild) { return CurrentChild->GetStackIssue().UniqueIdentifier == Issue.UniqueIdentifier; });
		if (Found == nullptr)
		{
			ErrorEntry = NewObject<UNiagaraStackErrorItem>(this);
			ErrorEntry->Initialize(CreateDefaultChildRequiredData(), Issue, GetStackEditorDataKey());
		}
		else
		{
			ErrorEntry = *Found;
		}
		NewErrorChildren.Add(ErrorEntry);
	}
	ErrorChildren.Empty();
	ErrorChildren.Append(NewErrorChildren);
}

void UNiagaraStackEntry::IssueModified()
{
	RefreshChildren();
}

void UNiagaraStackEntry::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
}

void UNiagaraStackEntry::PostRefreshChildrenInternal()
{
}

UNiagaraStackEntry::FRequiredEntryData UNiagaraStackEntry::CreateDefaultChildRequiredData() const
{
	return FRequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(), GetExecutionCategoryName(), GetExecutionSubcategoryName(), GetStackEditorData());
}

int32 UNiagaraStackEntry::GetChildIndentLevel() const
{
	return GetShouldShowInStack() ? GetIndentLevel() + 1 : GetIndentLevel();
}

void UNiagaraStackEntry::ChildStructureChanged()
{
	ChlildStructureChangedInternal();
	StructureChangedDelegate.Broadcast();
}

void UNiagaraStackEntry::ChildDataObjectModified(UObject* ChangedObject)
{
	DataObjectModifiedDelegate.Broadcast(ChangedObject);
}

TOptional<UNiagaraStackEntry::FDropResult> UNiagaraStackEntry::ChildRequestCanDrop(const UNiagaraStackEntry& TargetChild, const TArray<UNiagaraStackEntry*>& DraggedEntries)
{
	TOptional<FDropResult> Result = ChildRequestCanDropInternal(TargetChild, DraggedEntries);
	if (Result.IsSet())
	{
		return Result;
	}
	else
	{
		return OnRequestCanDropDelegate.IsBound()
			? OnRequestCanDropDelegate.Execute(TargetChild, DraggedEntries)
			: TOptional<FDropResult>();
	}
}

TOptional<UNiagaraStackEntry::FDropResult> UNiagaraStackEntry::ChildRequestDrop(const UNiagaraStackEntry& TargetChild, const TArray<UNiagaraStackEntry*>& DraggedEntries)
{
	TOptional<FDropResult> Result = ChildRequestDropInternal(TargetChild, DraggedEntries);
	if (Result.IsSet())
	{
		return Result;
	}
	else
	{
		return OnRequestDropDelegate.IsBound()
			? OnRequestDropDelegate.Execute(TargetChild, DraggedEntries)
			: TOptional<FDropResult>();
	}
}
