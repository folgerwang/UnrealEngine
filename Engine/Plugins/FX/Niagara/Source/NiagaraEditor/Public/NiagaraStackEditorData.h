// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStackEditorData.generated.h"
struct FStackIssue;

/** Editor only UI data for emitters. */
UCLASS()
class UNiagaraStackEditorData : public UObject
{
	GENERATED_BODY()

public:
	/*
	* Gets whether or not a module has a rename pending.
	* @param ModuleInputKey A unique key for the module.
	*/
	bool GetModuleInputIsRenamePending(const FString& ModuleInputKey) const;

	/*
	* Sets whether or not a module is pinned.
	* @param ModuleInputKey A unique key for the module.
	* @param bIsRenamePending Whether or not the module is pinned.
	*/
	void SetModuleInputIsRenamePending(const FString& ModuleInputKey, bool bIsRenamePending);

	/*
	 * Gets whether or not a stack entry is Expanded.
	 * @param bIsExpandedDefault The default value to return if the expanded state hasn't been set for the stack entry.
	 * @param StackItemKey A unique key for the entry.
	 */
	bool GetStackEntryIsExpanded(const FString& StackEntryKey, bool bIsExpandedDefault) const;

	/*
	 * Sets whether or not a stack entry is Expanded.
	 * @param StackEntryKey A unique key for the entry.
	 * @param bIsExpanded Whether or not the entry is expanded.
	 */
	void SetStackEntryIsExpanded(const FString& StackEntryKey, bool bIsExpanded);

	/*
	 * Gets whether or not a stack entry was Expanded before triggering a stack search.
	 * @param bWasExpandedPreSearchDefault The default value to to return if the pre-search expanded state hasn't been set for the stack entry.
	 * @param StackEntryKey a unique key for the entry.
	 */
	bool GetStackEntryWasExpandedPreSearch(const FString& StackEntryKey, bool bWasExpandedPreSearchDefault) const;

	/*
	 * Sets whether or not a stack entry was Expanded before a stack search was triggered.
	 * @param StackEntryKey A unique key for the entry.
	 * @param bWasExpandedPreSearch Whether or not the entry was expanded pre-search.
	 */
	void SetStackEntryWasExpandedPreSearch(const FString& StackEntryKey, bool bWasExpandedPreSearch);

	/*
	* Gets whether or not a stack item is showing advanced items.
	* @param StackItemKey A unique key for the entry.
	* @param bIsExpandedDefault The default value to return if the expanded state hasn't been set for the stack entry.
	*/
	bool GetStackItemShowAdvanced(const FString& StackEntryKey, bool bShowAdvancedDefault) const;

	/*
	* Sets whether or not a stack entry is Expanded.
	* @param StackEntryKey A unique key for the entry.
	* @param bIsExpanded Whether or not the entry is expanded.
	*/
	void SetStackItemShowAdvanced(const FString& StackEntryKey, bool bShowAdanced);

	/* Gets whether or not all advanced items should be shown in the stack. */
	bool GetShowAllAdvanced() const;

	/* Sets whether or not all advanced items should be shown in the stack. */
	void SetShowAllAdvanced(bool bInShowAllAdvanced);

	/* Gets whether or not item outputs should be shown in the stack. */
	bool GetShowOutputs() const;

	/* Sets whether or not item outputs should be shown in the stack. */
	void SetShowOutputs(bool bInShowOutputs);

	/* Gets whether or not item linked script inputs should be shown in the stack. */
	bool GetShowLinkedInputs() const;

	/* Sets whether or not item linked script inputs should be shown in the stack. */
	void SetShowLinkedInputs(bool bInShowLinkedInputs);

	/* Gets the last scroll position for the associated stack. */
	double GetLastScrollPosition() const;

	/* Sets the last scroll position for the associated stack. */
	void SetLastScrollPosition(double InLastScrollPosition);

	/*
	* @param Issue the issue to be dismissed (not fixed).
	*/
	void DismissStackIssue(FString IssueId);

	/* Restores all the dismissed issues so that the user can see them and choose what to do. */
	NIAGARAEDITOR_API void UndismissAllIssues();

	/* Gets a reference to the dismissed stack issue array */
	NIAGARAEDITOR_API const TArray<FString>& GetDismissedStackIssueIds();

private:
	UPROPERTY()
	TMap<FString, bool> ModuleInputKeyToRenamePendingMap;

	UPROPERTY()
	TMap<FString, bool> StackEntryKeyToExpandedMap;

	UPROPERTY()
	TMap<FString, bool> StackEntryKeyToPreSearchExpandedMap;

	UPROPERTY()
	TMap<FString, bool> StackItemKeyToShowAdvancedMap;

	UPROPERTY()
	bool bShowAllAdvanced;

	UPROPERTY()
	bool bShowOutputs;

	UPROPERTY()
	bool bShowLinkedInputs;

	UPROPERTY()
	double LastScrollPosition;

	UPROPERTY()
	TArray<FString> DismissedStackIssueIds;
};