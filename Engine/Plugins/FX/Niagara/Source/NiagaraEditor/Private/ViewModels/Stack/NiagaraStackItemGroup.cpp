// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "ViewModels/Stack/NiagaraStackItemGroup.h"
#include "ViewModels/Stack/NiagaraStackItem.h"
#include "ViewModels/Stack/NiagaraStackErrorItem.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "NiagaraStackEditorData.h"
#include "ViewModels/Stack/NiagaraStackSpacer.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"


void UNiagaraStackItemGroup::Initialize(FRequiredEntryData InRequiredEntryData, FText InDisplayName, FText InToolTip, INiagaraStackItemGroupAddUtilities* InAddUtilities)
{
	Super::Initialize(InRequiredEntryData, InDisplayName.ToString());
	GroupDisplayName = InDisplayName;
	GroupToolTip = InToolTip;
	AddUtilities = InAddUtilities;
}

FText UNiagaraStackItemGroup::GetDisplayName() const
{
	return GroupDisplayName;
}

UNiagaraStackEntry::EStackRowStyle UNiagaraStackItemGroup::GetStackRowStyle() const
{
	return EStackRowStyle::GroupHeader;
}

FText UNiagaraStackItemGroup::GetTooltipText() const 
{
	return GroupToolTip;
}

INiagaraStackItemGroupAddUtilities* UNiagaraStackItemGroup::GetAddUtilities() const
{
	return AddUtilities;
}

uint32 UNiagaraStackItemGroup::GetRecursiveStackIssuesCount() const
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

EStackIssueSeverity UNiagaraStackItemGroup::GetHighestStackIssueSeverity() const
{
	if (HighestIssueSeverity.IsSet() == false)
	{
		GetRecursiveStackIssuesCount();
	}
	return HighestIssueSeverity.GetValue();
}

void UNiagaraStackItemGroup::SetDisplayName(FText InDisplayName)
{
	GroupDisplayName = InDisplayName;
}

void UNiagaraStackItemGroup::RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues)
{
	UNiagaraStackSpacer* SeparatorSpacer = FindCurrentChildOfTypeByPredicate<UNiagaraStackSpacer>(CurrentChildren,
		[=](UNiagaraStackSpacer* CurrentSpacer) { return CurrentSpacer->GetSpacerKey() == "SeparatorSpacer"; });
	if (SeparatorSpacer == nullptr)
	{
		SeparatorSpacer = NewObject<UNiagaraStackSpacer>(this);
		FRequiredEntryData RequiredEntryData(GetSystemViewModel(), GetEmitterViewModel(), GetExecutionCategoryName(), NAME_None, GetStackEditorData());
		SeparatorSpacer->Initialize(RequiredEntryData, "SeparatorSpacer");
	}
	NewChildren.Add(SeparatorSpacer);
	RecursiveStackIssuesCount.Reset();
	HighestIssueSeverity.Reset();
}

int32 UNiagaraStackItemGroup::GetChildIndentLevel() const
{
	return GetIndentLevel();
}

void UNiagaraStackItemGroup::ChlildStructureChangedInternal()
{
	Super::ChlildStructureChangedInternal();
	RecursiveStackIssuesCount.Reset();
	HighestIssueSeverity.Reset();
}
