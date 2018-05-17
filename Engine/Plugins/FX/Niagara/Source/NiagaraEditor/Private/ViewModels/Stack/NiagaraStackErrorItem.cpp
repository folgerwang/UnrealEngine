// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackErrorItem.h"
#include "NiagaraStackEditorData.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "NiagaraStackErrorItem"
//UNiagaraStackErrorItem

void UNiagaraStackErrorItem::Initialize(FRequiredEntryData InRequiredEntryData, FStackIssue InStackIssue, FString InStackEditorDataKey)
{
	FString ErrorStackEditorDataKey = FString::Printf(TEXT("%s-Error-%s"), *InStackEditorDataKey, InStackIssue.UniqueIdentifier.GetPlainANSIString());
	Super::Initialize(InRequiredEntryData, ErrorStackEditorDataKey);
	StackIssue = InStackIssue;
	EntryStackEditorDataKey = InStackEditorDataKey;
}

FText UNiagaraStackErrorItem::GetDisplayName() const
{
	return StackIssue.ShortDescription;
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackErrorItem::GetStackRowStyle() const
{
	return EStackRowStyle::StackIssue;
}

UNiagaraStackErrorItem::FOnIssueNotify& UNiagaraStackErrorItem::OnIssueModified()
{
	return IssueModifiedDelegate;
}

void UNiagaraStackErrorItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	for (UNiagaraStackEntry* Child : CurrentChildren)
	{
		UNiagaraStackErrorItemFix* FixChild = Cast<UNiagaraStackErrorItemFix>(Child);
		if (FixChild != nullptr)
		{
			FixChild->OnIssueFixed().RemoveAll(this);
		}
	}
	// long description
	UNiagaraStackErrorItemLongDescription* ErrorEntryLongDescription = FindCurrentChildOfTypeByPredicate<UNiagaraStackErrorItemLongDescription>(CurrentChildren,
		[&](UNiagaraStackErrorItemLongDescription* CurrentChild) { return true; });
	if (ErrorEntryLongDescription == nullptr)
	{
		ErrorEntryLongDescription = NewObject<UNiagaraStackErrorItemLongDescription>(this);
		ErrorEntryLongDescription->Initialize(CreateDefaultChildRequiredData(), StackIssue, GetStackEditorDataKey());
	}
	NewChildren.Add(ErrorEntryLongDescription);

	// fixes
	for (int i = 0; i < StackIssue.Fixes.Num(); i++)
	{
		UNiagaraStackErrorItemFix* ErrorEntryFix = FindCurrentChildOfTypeByPredicate<UNiagaraStackErrorItemFix>(CurrentChildren,
			[&](UNiagaraStackErrorItemFix* CurrentChild) { return CurrentChild->GetStackIssueFix() == StackIssue.Fixes[i]; });
		if (ErrorEntryFix == nullptr)
		{
			ErrorEntryFix = NewObject<UNiagaraStackErrorItemFix>(this);
			ErrorEntryFix->Initialize(CreateDefaultChildRequiredData(), StackIssue, StackIssue.Fixes[i], EntryStackEditorDataKey);
		}
		ErrorEntryFix->OnIssueFixed().AddUObject(this, &UNiagaraStackErrorItem::IssueFixed);
		NewChildren.Add(ErrorEntryFix);
	}
	// dismiss button
	if (StackIssue.bCanBeDismissed)
	{
		UNiagaraStackErrorItemDismiss* ErrorEntryDismiss = FindCurrentChildOfTypeByPredicate<UNiagaraStackErrorItemDismiss>(CurrentChildren,
			[&](UNiagaraStackErrorItemDismiss* CurrentChild) { return true; });
		if (ErrorEntryDismiss == nullptr)
		{
			ErrorEntryDismiss = NewObject<UNiagaraStackErrorItemDismiss>(this);
			ErrorEntryDismiss->Initialize(CreateDefaultChildRequiredData(), StackIssue, EntryStackEditorDataKey);
		}
		ErrorEntryDismiss->OnIssueFixed().AddUObject(this, &UNiagaraStackErrorItem::IssueFixed);
		NewChildren.Add(ErrorEntryDismiss);
	}
}

void UNiagaraStackErrorItem::IssueFixed()
{
	OnIssueModified().Broadcast();
}

//UNiagaraStackErrorItemLongDescription
void UNiagaraStackErrorItemLongDescription::Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStackEntry::FStackIssue InStackIssue, FString InStackEditorDataKey)
{
	FString ErrorStackEditorDataKey = FString::Printf(TEXT("Long-%s"), *InStackEditorDataKey);
	Super::Initialize(InRequiredEntryData, ErrorStackEditorDataKey);
	StackIssue = InStackIssue;
}

FText UNiagaraStackErrorItemLongDescription::GetDisplayName() const
{
	return StackIssue.LongDescription;
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackErrorItemLongDescription::GetStackRowStyle() const
{
	return EStackRowStyle::StackIssue;
}

//UNiagaraStackErrorItemFix

void UNiagaraStackErrorItemFix::Initialize(FRequiredEntryData InRequiredEntryData, FStackIssue InStackIssue, FStackIssueFix InIssueFix, FString InStackEditorDataKey)
{
	FString ErrorStackEditorDataKey = FString::Printf(TEXT("Fix-%s"), *InStackEditorDataKey);
	Super::Initialize(InRequiredEntryData, ErrorStackEditorDataKey);
	StackIssue = InStackIssue;
	IssueFix = InIssueFix;
}

FText UNiagaraStackErrorItemFix::FixDescription() const
{
	if (IssueFix.Description.IsEmptyOrWhitespace())
	{
		ensureMsgf(false, TEXT("Fix description is empty or whitespace."));
		return LOCTEXT("FixIssue", "Fix the issue");
	}
	return IssueFix.Description;
}

FReply UNiagaraStackErrorItemFix::OnTryFixError()
{
	ensureMsgf(IssueFix.FixDelegate.IsBound(), TEXT("Fix delegate is not bound!"));
	IssueFix.FixDelegate.ExecuteIfBound();
	OnIssueFixed().Broadcast();
	return FReply::Handled();
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackErrorItemFix::GetStackRowStyle() const
{
	return EStackRowStyle::StackIssue;
}

FText UNiagaraStackErrorItemFix::GetFixButtonText() const
{
	return LOCTEXT("FixIssue", "Fix issue");
}

UNiagaraStackErrorItem::FOnIssueNotify& UNiagaraStackErrorItemFix::OnIssueFixed()
{
	return IssueFixedDelegate;
}

//UNiagaraStackErrorItemDismiss

void UNiagaraStackErrorItemDismiss::Initialize(FRequiredEntryData InRequiredEntryData, UNiagaraStackEntry::FStackIssue InStackIssue, FString InStackEditorDataKey)
{
	FString ErrorStackEditorDataKey = FString::Printf(TEXT("Dismiss-%s"), *InStackEditorDataKey);
	UNiagaraStackEntry::Initialize(InRequiredEntryData, ErrorStackEditorDataKey);
	StackIssue = InStackIssue;
	IssueFix.Description = LOCTEXT("DismissError", "Dismiss the issue without fixing (I know what I'm doing).");
}

void UNiagaraStackErrorItemDismiss::DismissIssue()
{
	GetStackEditorData().Modify();
	GetStackEditorData().DismissStackIssue(StackIssue.UniqueIdentifier);
}

FReply UNiagaraStackErrorItemDismiss::OnTryFixError()
{
	ensureMsgf(IssueFixedDelegate.IsBound(), TEXT("Issue fixed delegate is not bound on the dismiss entry!"));
	FScopedTransaction ScopedTransaction(LOCTEXT("DismissIssue", "Dismiss issue"));
	DismissIssue();
	OnIssueFixed().Broadcast();
	return FReply::Handled();
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackErrorItemDismiss::GetStackRowStyle() const
{
	return EStackRowStyle::StackIssue;
}

FText UNiagaraStackErrorItemDismiss::GetFixButtonText() const
{
	return LOCTEXT("DismissIssue", "Dismiss issue");
}

#undef LOCTEXT_NAMESPACE