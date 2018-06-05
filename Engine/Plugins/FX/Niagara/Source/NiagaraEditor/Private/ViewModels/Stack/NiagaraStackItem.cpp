// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackItem.h"
#include "ViewModels/Stack/NiagaraStackSpacer.h"
#include "ViewModels/Stack/NiagaraStackAdvancedExpander.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "ViewModels/Stack/NiagaraStackErrorItem.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "NiagaraStackEditorData.h"

void UNiagaraStackItem::Initialize(FRequiredEntryData InRequiredEntryData, FString InStackEditorDataKey)
{
	Super::Initialize(InRequiredEntryData, InStackEditorDataKey);
	AddChildFilter(FOnFilterChild::CreateUObject(this, &UNiagaraStackItem::FilterAdvancedChildren));
	AddChildFilter(FOnFilterChild::CreateUObject(this, &UNiagaraStackItem::FilterShowAdvancedChild));
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackItem::GetStackRowStyle() const
{
	return UNiagaraStackEntry::EStackRowStyle::ItemHeader;
}

void UNiagaraStackItem::SetOnModifiedGroupItems(FOnModifiedGroupItems OnModifiedGroupItems)
{
	ModifiedGroupItemsDelegate = OnModifiedGroupItems;
}

uint32 UNiagaraStackItem::GetRecursiveStackIssuesCount() const
{
	if (RecursiveStackIssuesCount.IsSet() == false)
	{
		TArray<UNiagaraStackErrorItem*> RecursiveIssues;
		FNiagaraStackGraphUtilities::GetStackIssuesRecursively(this, RecursiveIssues);
		RecursiveStackIssuesCount = RecursiveIssues.Num();
		EStackIssueSeverity MinSeverity = EStackIssueSeverity::Info;
		for (auto Issue : RecursiveIssues)
		{
			if (Issue->GetStackIssue().GetSeverity() < MinSeverity)
			{
				MinSeverity = Issue->GetStackIssue().GetSeverity();
			}
		}
		HighestIssueSeverity = MinSeverity;
	}
	return RecursiveStackIssuesCount.GetValue();
}

EStackIssueSeverity UNiagaraStackItem::GetHighestStackIssueSeverity() const
{
	if (HighestIssueSeverity.IsSet() == false)
	{
		GetRecursiveStackIssuesCount();
	}
	return HighestIssueSeverity.GetValue();
}

void UNiagaraStackItem::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	if (FooterSpacer == nullptr)
	{
		FooterSpacer = NewObject<UNiagaraStackSpacer>(this);
		FooterSpacer->Initialize(CreateDefaultChildRequiredData(), "ItemFooterSpacer", 1, UNiagaraStackEntry::EStackRowStyle::ItemContent);
	}

	if (ShowAdvancedExpander == nullptr)
	{
		ShowAdvancedExpander = NewObject<UNiagaraStackAdvancedExpander>(this);
		ShowAdvancedExpander->Initialize(CreateDefaultChildRequiredData(), GetStackEditorDataKey(), GetOwningNiagaraNode());
		ShowAdvancedExpander->SetOnToggleShowAdvanced(UNiagaraStackAdvancedExpander::FOnToggleShowAdvanced::CreateUObject(this, &UNiagaraStackItem::ToggleShowAdvanced));
	}

	NewChildren.Add(FooterSpacer);
	NewChildren.Add(ShowAdvancedExpander);
	RecursiveStackIssuesCount.Reset();
	HighestIssueSeverity.Reset();
}

void GetContentChildren(UNiagaraStackEntry& CurrentEntry, TArray<UNiagaraStackItemContent*>& ContentChildren)
{
	TArray<UNiagaraStackEntry*> Children;
	CurrentEntry.GetUnfilteredChildren(Children);
	for (UNiagaraStackEntry* Child : Children)
	{
		UNiagaraStackItemContent* ContentChild = Cast<UNiagaraStackItemContent>(Child);
		if (ContentChild != nullptr)
		{
			ContentChildren.Add(ContentChild);
		}
		GetContentChildren(*Child, ContentChildren);
	}
}

void UNiagaraStackItem::PostRefreshChildrenInternal()
{
	Super::PostRefreshChildrenInternal();
	bHasAdvancedContent = false;
	TArray<UNiagaraStackItemContent*> ContentChildren;
	GetContentChildren(*this, ContentChildren);
	for (UNiagaraStackItemContent* ContentChild : ContentChildren)
	{
		if (ContentChild->GetIsAdvanced())
		{
			bHasAdvancedContent = true;
			break;
		}
	}
}

int32 UNiagaraStackItem::GetChildIndentLevel() const
{
	return GetIndentLevel();
}

UNiagaraNode* UNiagaraStackItem::GetOwningNiagaraNode() const
{
	return nullptr;
}

void UNiagaraStackItem::ChlildStructureChangedInternal()
{
	Super::ChlildStructureChangedInternal();
	RecursiveStackIssuesCount.Reset();
	HighestIssueSeverity.Reset();
}

bool UNiagaraStackItem::FilterAdvancedChildren(const UNiagaraStackEntry& Child) const
{
	const UNiagaraStackItemContent* ItemContent = Cast<UNiagaraStackItemContent>(&Child);
	if (ItemContent == nullptr || ItemContent->GetIsAdvanced() == false)
	{
		return true;
	}
	else
	{
		return GetStackEditorData().GetShowAllAdvanced() || GetStackEditorData().GetStackItemShowAdvanced(GetStackEditorDataKey(), false);
	}
}

bool UNiagaraStackItem::FilterShowAdvancedChild(const UNiagaraStackEntry& Child) const 
{
	if (&Child == ShowAdvancedExpander && bHasAdvancedContent == false)
	{
		return false;
	}
	else if (&Child == FooterSpacer && bHasAdvancedContent)
	{
		return false;
	}
	else
	{
		return true;
	}
}

void UNiagaraStackItem::ToggleShowAdvanced()
{
	bool bCurrentShowAdvanced = GetStackEditorData().GetStackItemShowAdvanced(GetStackEditorDataKey(), false);
	GetStackEditorData().SetStackItemShowAdvanced(GetStackEditorDataKey(), !bCurrentShowAdvanced);
	OnStructureChanged().Broadcast();
}

void UNiagaraStackItemContent::Initialize(FRequiredEntryData InRequiredEntryData, bool bInIsAdvanced, FString InOwningStackItemEditorDataKey, FString InStackEditorDataKey)
{
	Super::Initialize(InRequiredEntryData, InStackEditorDataKey);
	OwningStackItemEditorDataKey = InOwningStackItemEditorDataKey;
	bIsAdvanced = bInIsAdvanced;
	AddChildFilter(FOnFilterChild::CreateUObject(this, &UNiagaraStackItemContent::FilterAdvancedChildren));
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackItemContent::GetStackRowStyle() const
{
	return bIsAdvanced ? EStackRowStyle::ItemContentAdvanced : EStackRowStyle::ItemContent;
}

bool UNiagaraStackItemContent::GetIsAdvanced() const
{
	return bIsAdvanced;
}

FString UNiagaraStackItemContent::GetOwnerStackItemEditorDataKey() const
{
	return OwningStackItemEditorDataKey;
}

void UNiagaraStackItemContent::SetIsAdvanced(bool bInIsAdvanced)
{
	if (bIsAdvanced != bInIsAdvanced)
	{
		// When changing advanced, invalidate the structure so that the filters run again.
		bIsAdvanced = bInIsAdvanced;
		OnStructureChanged().Broadcast();
	}
}

bool UNiagaraStackItemContent::FilterAdvancedChildren(const UNiagaraStackEntry& Child) const
{
	const UNiagaraStackItemContent* ItemContent = Cast<UNiagaraStackItemContent>(&Child);
	if (ItemContent == nullptr || ItemContent->GetIsAdvanced() == false)
	{
		return true;
	}
	else
	{
		return GetStackEditorData().GetShowAllAdvanced() || GetStackEditorData().GetStackItemShowAdvanced(OwningStackItemEditorDataKey, false);
	}
}
