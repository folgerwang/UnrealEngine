// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "ViewModels/Stack/NiagaraStackErrorItem.h"
#include "NiagaraStackEditorData.h"
#include "Misc/SecureHash.h"

const FName UNiagaraStackEntry::FExecutionCategoryNames::System = TEXT("System");
const FName UNiagaraStackEntry::FExecutionCategoryNames::Emitter = TEXT("Emitter");
const FName UNiagaraStackEntry::FExecutionCategoryNames::Particle = TEXT("Particle");
const FName UNiagaraStackEntry::FExecutionCategoryNames::Render = TEXT("Render");

const FName UNiagaraStackEntry::FExecutionSubcategoryNames::Parameters = TEXT("Parameters");
const FName UNiagaraStackEntry::FExecutionSubcategoryNames::Spawn = TEXT("Spawn");
const FName UNiagaraStackEntry::FExecutionSubcategoryNames::Update = TEXT("Update");
const FName UNiagaraStackEntry::FExecutionSubcategoryNames::Event = TEXT("Event");

UNiagaraStackEntry::FStackIssueFix::FStackIssueFix()
{
}

UNiagaraStackEntry::FStackIssueFix::FStackIssueFix(FText InDescription, FStackIssueFixDelegate InFixDelegate)
	: Description(InDescription)
	, FixDelegate(InFixDelegate)
	, UniqueIdentifier(FMD5::HashAnsiString(*FString::Printf(TEXT("%s"), *InDescription.ToString())))
{
	checkf(Description.IsEmptyOrWhitespace() == false, TEXT("Description can not be empty."));
	checkf(InFixDelegate.IsBound(), TEXT("Fix delegate must be bound."));
}

bool UNiagaraStackEntry::FStackIssueFix::IsValid() const
{
	return FixDelegate.IsBound();
}

const FText& UNiagaraStackEntry::FStackIssueFix::GetDescription() const
{
	return Description;
}

void UNiagaraStackEntry::FStackIssueFix::SetFixDelegate(const FStackIssueFixDelegate& InFixDelegate)
{
	FixDelegate = InFixDelegate;
}

const FString& UNiagaraStackEntry::FStackIssueFix::GetUniqueIdentifier() const
{
	return UniqueIdentifier;
}

const UNiagaraStackEntry::FStackIssueFixDelegate& UNiagaraStackEntry::FStackIssueFix::GetFixDelegate() const
{
	return FixDelegate;
}

UNiagaraStackEntry::FStackIssue::FStackIssue()
{
}

UNiagaraStackEntry::FStackIssue::FStackIssue(EStackIssueSeverity InSeverity, FText InShortDescription, FText InLongDescription, FString InStackEditorDataKey, bool bInCanBeDismissed, const TArray<FStackIssueFix>& InFixes)
	: Severity(InSeverity)
	, ShortDescription(InShortDescription)
	, LongDescription(InLongDescription)
	, UniqueIdentifier(FMD5::HashAnsiString(*FString::Printf(TEXT("%s-%s"), *InStackEditorDataKey, *InLongDescription.ToString())))
	, bCanBeDismissed(bInCanBeDismissed)
	, Fixes(InFixes)
{
	checkf(ShortDescription.IsEmptyOrWhitespace() == false, TEXT("Short description can not be empty."));
	checkf(LongDescription.IsEmptyOrWhitespace() == false, TEXT("Long description can not be empty."));
	checkf(InStackEditorDataKey.IsEmpty() == false, TEXT("Stack editor data key can not be empty."));
}

UNiagaraStackEntry::FStackIssue::FStackIssue(EStackIssueSeverity InSeverity, FText InShortDescription, FText InLongDescription, FString InStackEditorDataKey, bool bInCanBeDismissed, FStackIssueFix InFix)
	: FStackIssue(InSeverity, InShortDescription, InLongDescription, InStackEditorDataKey, bInCanBeDismissed, TArray<FStackIssueFix>({InFix }))
{
}

UNiagaraStackEntry::FStackIssue::FStackIssue(EStackIssueSeverity InSeverity, FText InShortDescription, FText InLongDescription, FString InStackEditorDataKey, bool bInCanBeDismissed)
	: FStackIssue(InSeverity, InShortDescription, InLongDescription, InStackEditorDataKey, bInCanBeDismissed, TArray<FStackIssueFix>())
{
}

bool UNiagaraStackEntry::FStackIssue::IsValid()
{
	return UniqueIdentifier.IsEmpty() == false;
}

EStackIssueSeverity UNiagaraStackEntry::FStackIssue::GetSeverity() const
{
	return Severity;
}

const FText& UNiagaraStackEntry::FStackIssue::GetShortDescription() const
{
	return ShortDescription;
}

const FText& UNiagaraStackEntry::FStackIssue::GetLongDescription() const
{
	return LongDescription;
}

bool UNiagaraStackEntry::FStackIssue::GetCanBeDismissed() const
{
	return bCanBeDismissed;
}

const FString& UNiagaraStackEntry::FStackIssue::GetUniqueIdentifier() const
{
	return UniqueIdentifier;
}

const TArray<UNiagaraStackEntry::FStackIssueFix>& UNiagaraStackEntry::FStackIssue::GetFixes() const
{
	return Fixes;
}

UNiagaraStackEntry::UNiagaraStackEntry()
	: IndentLevel(0)
	, bIsFinalized(false)
{
}

void UNiagaraStackEntry::Initialize(FRequiredEntryData InRequiredEntryData, FString InStackEditorDataKey)
{
	checkf(bIsFinalized == false, TEXT("Can not initialize an entry after it has been finalized."));
	SystemViewModel = InRequiredEntryData.SystemViewModel;
	EmitterViewModel = InRequiredEntryData.EmitterViewModel;
	ExecutionCategoryName = InRequiredEntryData.ExecutionCategoryName;
	ExecutionSubcategoryName = InRequiredEntryData.ExecutionSubcategoryName;
	StackEditorData = InRequiredEntryData.StackEditorData;
	StackEditorDataKey = InStackEditorDataKey;
}

void UNiagaraStackEntry::Finalize()
{
	FinalizeInternal();
	checkf(bIsFinalized, TEXT("Parent FinalizeInternal not called from overriden FinalizeInternal"));

	SystemViewModel.Reset();
	EmitterViewModel.Reset();
	StackEditorData = nullptr;

	for (UNiagaraStackEntry* Child : Children)
	{
		Child->Finalize();
	}
	Children.Empty();

	for (UNiagaraStackEntry* ErrorChild : ErrorChildren)
	{
		ErrorChild->Finalize();
	}
	ErrorChildren.Empty();
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

UNiagaraStackEntry::FOnRequestFullRefresh& UNiagaraStackEntry::OnRequestFullRefresh()
{
	return RequestFullRefreshDelegate;
}

UNiagaraStackEntry::FOnRequestFullRefresh& UNiagaraStackEntry::OnRequestFullRefreshDeferred()
{
	return RequestFullRefreshDeferredDelegate;
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

void UNiagaraStackEntry::FinalizeInternal()
{
	bIsFinalized = true;
}

void UNiagaraStackEntry::RefreshChildren()
{
	checkf(bIsFinalized == false, TEXT("Can not refresh children on an entry after it has been finalized."));
	checkf(SystemViewModel.IsValid() && EmitterViewModel.IsValid(), TEXT("Base stack entry not initialized."));

	for (UNiagaraStackEntry* Child : Children)
	{
		Child->OnStructureChanged().RemoveAll(this);
		Child->OnDataObjectModified().RemoveAll(this);
		Child->OnRequestFullRefresh().RemoveAll(this);
		Child->OnRequestFullRefreshDeferred().RemoveAll(this);
		Child->SetOnRequestCanDrop(FOnRequestDrop());
		Child->SetOnRequestDrop(FOnRequestDrop());
	}
	for (UNiagaraStackErrorItem* ErrorChild : ErrorChildren)
	{
		ErrorChild->OnStructureChanged().RemoveAll(this);
		ErrorChild->OnDataObjectModified().RemoveAll(this);
		ErrorChild->OnRequestFullRefresh().RemoveAll(this);
		ErrorChild->OnRequestFullRefreshDeferred().RemoveAll(this);
		ErrorChild->OnIssueModified().RemoveAll(this);
	}

	TArray<UNiagaraStackEntry*> NewChildren;
	TArray<FStackIssue> NewStackIssues;
	RefreshChildrenInternal(Children, NewChildren, NewStackIssues);

	// If any of the current children were not moved to the new children collection than finalize them since
	// they weren't reused.
	for (UNiagaraStackEntry* Child : Children)
	{
		if (NewChildren.Contains(Child) == false)
		{
			Child->Finalize();
		}
	}

	Children.Empty();
	Children.Append(NewChildren);

	for (UNiagaraStackEntry* Child : Children)
	{
		Child->IndentLevel = GetChildIndentLevel();
		Child->RefreshChildren();
		Child->OnStructureChanged().AddUObject(this, &UNiagaraStackEntry::ChildStructureChanged);
		Child->OnDataObjectModified().AddUObject(this, &UNiagaraStackEntry::ChildDataObjectModified);
		Child->OnRequestFullRefresh().AddUObject(this, &UNiagaraStackEntry::ChildRequestFullRefresh);
		Child->OnRequestFullRefreshDeferred().AddUObject(this, &UNiagaraStackEntry::ChildRequestFullRefreshDeferred);
		Child->SetOnRequestCanDrop(FOnRequestDrop::CreateUObject(this, &UNiagaraStackEntry::ChildRequestCanDrop));
		Child->SetOnRequestDrop(FOnRequestDrop::CreateUObject(this, &UNiagaraStackEntry::ChildRequestDrop));
	}
	
	// Stack issues refresh
	NewStackIssues.RemoveAll([=](const FStackIssue& Issue) { return Issue.GetCanBeDismissed() && GetStackEditorData().GetDismissedStackIssueIds().Contains(Issue.GetUniqueIdentifier()); }); 

	StackIssues.Empty();
	StackIssues.Append(NewStackIssues);
	RefreshStackErrorChildren();
	for (UNiagaraStackErrorItem* ErrorChild : ErrorChildren)
	{
		ErrorChild->IndentLevel = GetChildIndentLevel();
		ErrorChild->RefreshChildren();
		ErrorChild->OnStructureChanged().AddUObject(this, &UNiagaraStackEntry::ChildStructureChanged);
		ErrorChild->OnDataObjectModified().AddUObject(this, &UNiagaraStackEntry::ChildDataObjectModified);
		ErrorChild->OnRequestFullRefresh().AddUObject(this, &UNiagaraStackEntry::ChildRequestFullRefresh);
		ErrorChild->OnRequestFullRefreshDeferred().AddUObject(this, &UNiagaraStackEntry::ChildRequestFullRefreshDeferred);
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
			[&](UNiagaraStackErrorItem* CurrentChild) { return CurrentChild->GetStackIssue().GetUniqueIdentifier() == Issue.GetUniqueIdentifier(); });
		if (Found == nullptr)
		{
			ErrorEntry = NewObject<UNiagaraStackErrorItem>(this);
			ErrorEntry->Initialize(CreateDefaultChildRequiredData(), Issue, GetStackEditorDataKey());
		}
		else
		{
			ErrorEntry = *Found;
			ErrorEntry->SetStackIssue(Issue); // we found the entry by id but we want to properly refresh the subentries of the issue (specifically its fixes), too
		}
		NewErrorChildren.Add(ErrorEntry);
	}

	// If any of the current error children were not moved to the new error children collection than finalize them since
	// they weren't reused.
	for (UNiagaraStackEntry* ErrorChild : ErrorChildren)
	{
		if(NewErrorChildren.Contains(ErrorChild) == false)
		{
			ErrorChild->Finalize();
		}
	}

	ErrorChildren.Empty();
	ErrorChildren.Append(NewErrorChildren);
}

void UNiagaraStackEntry::IssueModified()
{
	RefreshChildren();
}

void UNiagaraStackEntry::BeginDestroy()
{
	ensureMsgf(HasAnyFlags(RF_ClassDefaultObject) || bIsFinalized, TEXT("Stack entry being destroyed but it was not finalized."));
	Super::BeginDestroy();
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

void UNiagaraStackEntry::ChildRequestFullRefresh()
{
	RequestFullRefreshDelegate.Broadcast();
}

void UNiagaraStackEntry::ChildRequestFullRefreshDeferred()
{
	RequestFullRefreshDeferredDelegate.Broadcast();
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
