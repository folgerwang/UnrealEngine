// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEntry.generated.h"

class FNiagaraSystemViewModel;
class FNiagaraEmitterViewModel;
class FNiagaraScriptViewModel;
class UNiagaraStackEditorData;
class UNiagaraStackErrorItem;

UENUM()
enum class EStackIssueSeverity : uint8
{
	Error = 0,
	Warning, 
	Info
};

UCLASS()
class NIAGARAEDITOR_API UNiagaraStackEntry : public UObject
{
	GENERATED_BODY()

public:
	struct FDropResult
	{
		explicit FDropResult(bool bInCanDrop, FText InDropMessage = FText())
			: bCanDrop(bInCanDrop)
			, DropMessage(InDropMessage)
		{
		}

		const bool bCanDrop;
		const FText DropMessage;
	};

	DECLARE_MULTICAST_DELEGATE(FOnStructureChanged);
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnDataObjectModified, UObject*);
	DECLARE_MULTICAST_DELEGATE(FOnRequestFullRefresh);
	DECLARE_MULTICAST_DELEGATE(FOnRequestFullRefreshDeferred);
	DECLARE_DELEGATE_RetVal_TwoParams(TOptional<FDropResult>, FOnRequestDrop, const UNiagaraStackEntry&, const TArray<UNiagaraStackEntry*>&);
	DECLARE_DELEGATE_RetVal_OneParam(bool, FOnFilterChild, const UNiagaraStackEntry&);
	DECLARE_DELEGATE(FStackIssueFixDelegate);

public:
	struct NIAGARAEDITOR_API FExecutionCategoryNames
	{
		static const FName System;
		static const FName Emitter;
		static const FName Particle;
		static const FName Render;
	};

	struct NIAGARAEDITOR_API FExecutionSubcategoryNames
	{
		static const FName Parameters;
		static const FName Spawn;
		static const FName Update;
		static const FName Event;
	};

	enum class EStackRowStyle
	{
		None,
		GroupHeader,
		ItemHeader,
		ItemContent,
		ItemContentAdvanced,
		ItemFooter,
		ItemCategory,
		StackIssue
	};

	struct FRequiredEntryData
	{
		FRequiredEntryData(TSharedRef<FNiagaraSystemViewModel> InSystemViewModel, TSharedRef<FNiagaraEmitterViewModel> InEmitterViewModel, FName InExecutionCategoryName, FName InExecutionSubcategoryName, UNiagaraStackEditorData& InStackEditorData)
			: SystemViewModel(InSystemViewModel)
			, EmitterViewModel(InEmitterViewModel)
			, ExecutionCategoryName(InExecutionCategoryName)
			, ExecutionSubcategoryName(InExecutionSubcategoryName)
			, StackEditorData(&InStackEditorData)
		{
		}

		const TSharedRef<FNiagaraSystemViewModel> SystemViewModel;
		const TSharedRef<FNiagaraEmitterViewModel> EmitterViewModel;
		const FName ExecutionCategoryName;
		const FName ExecutionSubcategoryName;
		UNiagaraStackEditorData* const StackEditorData;
	};

	struct FStackSearchItem
	{
		FName Key;
		FText Value;
		
		inline bool operator==(FStackSearchItem Item) {
			return (Item.Key == Key 
				&& Item.Value.ToString() == Value.ToString());
		}
	};

	// stack issue stuff
	struct FStackIssueFix
	{
		FStackIssueFix();

		FStackIssueFix(FText InDescription, FStackIssueFixDelegate InFixDelegate);

		bool IsValid() const;

		const FText& GetDescription() const;

		void SetFixDelegate(const FStackIssueFixDelegate& InFixDelegate);

		const FStackIssueFixDelegate& GetFixDelegate() const;

		const FString& GetUniqueIdentifier() const;

	private:
		FText Description;
		FStackIssueFixDelegate FixDelegate;
		FString UniqueIdentifier;
	};

	struct FStackIssue
	{
		FStackIssue();

		FStackIssue(EStackIssueSeverity InSeverity, FText InShortDescription, FText InLongDescription, FString InStackEditorDataKey, bool bInCanBeDismissed, const TArray<FStackIssueFix>& InFixes);

		FStackIssue(EStackIssueSeverity InSeverity, FText InShortDescription, FText InLongDescription, FString InStackEditorDataKey, bool bInCanBeDismissed, FStackIssueFix InFix);

		FStackIssue(EStackIssueSeverity InSeverity, FText InShortDescription, FText InLongDescription, FString InStackEditorDataKey, bool bInCanBeDismissed);

		bool IsValid();

		EStackIssueSeverity GetSeverity() const;

		const FText& GetShortDescription() const;

		const FText& GetLongDescription() const;

		const FString& GetUniqueIdentifier() const;

		bool GetCanBeDismissed() const;

		const TArray<FStackIssueFix>& GetFixes() const;

	private:
		EStackIssueSeverity Severity;
		FText ShortDescription;
		FText LongDescription;
		FString UniqueIdentifier;
		bool bCanBeDismissed;
		TArray<FStackIssueFix> Fixes;
	};

public:
	UNiagaraStackEntry();

	void Initialize(FRequiredEntryData InRequiredEntryData, FString InStackEditorDataKey);

	void Finalize();

	virtual FText GetDisplayName() const;

	UNiagaraStackEditorData& GetStackEditorData() const;

	FString GetStackEditorDataKey() const;

	virtual FText GetTooltipText() const;

	virtual bool GetCanExpand() const;

	virtual bool IsExpandedByDefault() const;

	bool GetIsExpanded() const;

	// Calling this doesn't broadcast structure change automatically due to the expense of synchronizing
	// expanded state with the tree which is done to prevent items being expanded on tick.
	void SetIsExpanded(bool bInExpanded);

	virtual bool GetIsEnabled() const;

	FName GetExecutionCategoryName() const;

	FName GetExecutionSubcategoryName() const;

	virtual EStackRowStyle GetStackRowStyle() const;

	int32 GetIndentLevel() const;

	virtual bool GetShouldShowInStack() const;

	void GetFilteredChildren(TArray<UNiagaraStackEntry*>& OutFilteredChildren);

	void GetUnfilteredChildren(TArray<UNiagaraStackEntry*>& OutUnfilteredChildren);

	void GetUnfilteredChildren(TArray<UNiagaraStackEntry*>& OutUnfilteredChildren) const;

	FOnStructureChanged& OnStructureChanged();

	FOnDataObjectModified& OnDataObjectModified();

	FOnRequestFullRefresh& OnRequestFullRefresh();

	FOnRequestFullRefresh& OnRequestFullRefreshDeferred();

	void RefreshChildren();


	FDelegateHandle AddChildFilter(FOnFilterChild ChildFilter);
	void RemoveChildFilter(FDelegateHandle FilterHandle);

	TSharedRef<FNiagaraSystemViewModel> GetSystemViewModel() const;
	TSharedRef<FNiagaraEmitterViewModel> GetEmitterViewModel() const;

	template<typename ChildType, typename PredicateType>
	static ChildType* FindCurrentChildOfTypeByPredicate(const TArray<UNiagaraStackEntry*>& CurrentChildren, PredicateType Predicate)
	{
		for (UNiagaraStackEntry* CurrentChild : CurrentChildren)
		{
			ChildType* TypedCurrentChild = Cast<ChildType>(CurrentChild);
			if (TypedCurrentChild != nullptr && Predicate(TypedCurrentChild))
			{
				return TypedCurrentChild;
			}
		}
		return nullptr;
	}

	void GetSearchItems(TArray<FStackSearchItem>& SearchItems) const;

	virtual UObject* GetExternalAsset() const;

	virtual bool CanDrag() const;

	TOptional<FDropResult> CanDrop(const TArray<UNiagaraStackEntry*>& DraggedEntries);

	TOptional<FDropResult> Drop(const TArray<UNiagaraStackEntry*>& DraggedEntries);

	void SetOnRequestCanDrop(FOnRequestDrop InOnRequestCanDrop);

	void SetOnRequestDrop(FOnRequestDrop InOnRequestCanDrop);

protected:
	virtual void BeginDestroy() override;

	virtual void RefreshChildrenInternal(const TArray<UNiagaraStackEntry*>& CurrentChildren, TArray<UNiagaraStackEntry*>& NewChildren, TArray<FStackIssue>& NewIssues);

	virtual void PostRefreshChildrenInternal();

	FRequiredEntryData CreateDefaultChildRequiredData() const;

	virtual int32 GetChildIndentLevel() const;

	virtual void GetAdditionalSearchItemsInternal(TArray<FStackSearchItem>& SearchItems) const;

	virtual TOptional<FDropResult> CanDropInternal(const TArray<UNiagaraStackEntry*>& DraggedEntries);

	virtual TOptional<FDropResult> DropInternal(const TArray<UNiagaraStackEntry*>& DraggedEntries);

	virtual TOptional<FDropResult> ChildRequestCanDropInternal(const UNiagaraStackEntry& TargetChild, const TArray<UNiagaraStackEntry*>& DraggedEntries);

	virtual TOptional<FDropResult> ChildRequestDropInternal(const UNiagaraStackEntry& TargetChild, const TArray<UNiagaraStackEntry*>& DraggedEntries);

	virtual void ChlildStructureChangedInternal();

	virtual void FinalizeInternal();

private:
	void ChildStructureChanged();
	
	void ChildDataObjectModified(UObject* ChangedObject);

	void ChildRequestFullRefresh();

	void ChildRequestFullRefreshDeferred();

	TOptional<FDropResult> ChildRequestCanDrop(const UNiagaraStackEntry& TargetChild, const TArray<UNiagaraStackEntry*>& DraggedEntries);

	TOptional<FDropResult> ChildRequestDrop(const UNiagaraStackEntry& TargetChild, const TArray<UNiagaraStackEntry*>& DraggedEntries);

	void RefreshStackErrorChildren();

	void IssueModified();
	
private:
	TWeakPtr<FNiagaraSystemViewModel> SystemViewModel;
	TWeakPtr<FNiagaraEmitterViewModel> EmitterViewModel;

	UNiagaraStackEditorData* StackEditorData;

	FString StackEditorDataKey;

	FOnStructureChanged StructureChangedDelegate;

	FOnDataObjectModified DataObjectModifiedDelegate;

	FOnRequestFullRefresh RequestFullRefreshDelegate;

	FOnRequestFullRefresh RequestFullRefreshDeferredDelegate;

	TArray<FOnFilterChild> ChildFilters;

	UPROPERTY()
	TArray<UNiagaraStackEntry*> Children;

	UPROPERTY()
	TArray<UNiagaraStackErrorItem*> ErrorChildren;

	mutable TOptional<bool> bIsExpandedCache;

	int32 IndentLevel;

	FName ExecutionCategoryName;
	FName ExecutionSubcategoryName;

	FOnRequestDrop OnRequestCanDropDelegate;
	FOnRequestDrop OnRequestDropDelegate;
	
	TArray<FStackIssue> StackIssues;

	bool bIsFinalized;
};