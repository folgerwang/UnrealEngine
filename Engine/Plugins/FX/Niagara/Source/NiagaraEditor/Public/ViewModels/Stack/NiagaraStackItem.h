// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "Layout/Visibility.h"
#include "NiagaraStackItem.generated.h"

class UNiagaraStackSpacer;
class UNiagaraStackAdvancedExpander;
class UNiagaraNode;

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackItem : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	DECLARE_DELEGATE(FOnModifiedGroupItems);

public:
	void Initialize(FRequiredEntryData InRequiredEntryData, FString InStackEditorDataKey);

	virtual EStackRowStyle GetStackRowStyle() const override;

	void SetOnModifiedGroupItems(FOnModifiedGroupItems OnModifiedGroupItems);

	uint32 GetRecursiveStackIssuesCount() const;
	EStackIssueSeverity GetHighestStackIssueSeverity() const;
	
protected:
	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues) override;

	virtual void PostRefreshChildrenInternal() override;

	virtual int32 GetChildIndentLevel() const override;

	virtual UNiagaraNode* GetOwningNiagaraNode() const;

	virtual void ChlildStructureChangedInternal() override;

private:
	bool FilterAdvancedChildren(const UNiagaraStackEntry& Child) const;

	bool FilterShowAdvancedChild(const UNiagaraStackEntry& Child) const;

	void ToggleShowAdvanced();

protected:
	FOnModifiedGroupItems ModifiedGroupItemsDelegate;

private:
	bool bHasAdvancedContent;

	UPROPERTY()
	UNiagaraStackSpacer* FooterSpacer;

	UPROPERTY()
	UNiagaraStackAdvancedExpander* ShowAdvancedExpander;

	/** How many errors this entry has along its tree. */
	mutable TOptional<uint32> RecursiveStackIssuesCount;
	/** The highest severity of issues along this entry's tree. */
	mutable TOptional<EStackIssueSeverity> HighestIssueSeverity;
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackItemContent : public UNiagaraStackEntry
{
	GENERATED_BODY()

public:
	void Initialize(FRequiredEntryData InRequiredEntryData, bool bInIsAdvanced, FString InOwningStackItemEditorDataKey, FString InStackEditorDataKey);

	virtual EStackRowStyle GetStackRowStyle() const override;

	bool GetIsAdvanced() const;

protected:
	FString GetOwnerStackItemEditorDataKey() const;

	void SetIsAdvanced(bool bInIsAdvanced);

private:
	bool FilterAdvancedChildren(const UNiagaraStackEntry& Child) const;

private:
	FString OwningStackItemEditorDataKey;
	bool bIsAdvanced;
};
