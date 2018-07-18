// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "ViewModels/Stack/INiagaraStackItemGroupAddUtilities.h"
#include "Layout/Visibility.h"
#include "NiagaraStackItemGroup.generated.h"

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackItemGroup : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	void Initialize(FRequiredEntryData InRequiredEntryData, FText InDisplayName, FText InToolTip, INiagaraStackItemGroupAddUtilities* InAddUtilities);

	//~ UNiagaraStackEntry interface
	virtual FText GetDisplayName() const override;
	virtual EStackRowStyle GetStackRowStyle() const override;
	virtual FText GetTooltipText() const override;

	virtual bool CanDelete() const { return false; }
	virtual bool Delete() { return false; }

	INiagaraStackItemGroupAddUtilities* GetAddUtilities() const;

	uint32 GetRecursiveStackIssuesCount() const;
	EStackIssueSeverity GetHighestStackIssueSeverity() const;
	
protected:
	void SetDisplayName(FText InDisplayName);
	
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	virtual int32 GetChildIndentLevel() const override;

	virtual void ChlildStructureChangedInternal() override;

private:
	INiagaraStackItemGroupAddUtilities* AddUtilities;

	FText GroupDisplayName;
	FText GroupToolTip;

	/** How many errors this entry has along its tree. */
	mutable TOptional<uint32> RecursiveStackIssuesCount;
	/** The highest severity of issues along this entry's tree. */
	mutable TOptional<EStackIssueSeverity> HighestIssueSeverity;
};
